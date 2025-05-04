#include "serialization.h"
#include <cstring>

namespace serialization {
// Convert HtmlStats to a message that can be sent over MPI
std::vector<char> serializeHtmlStats(const html::Stats &stats) {
  std::vector<char> buffer;

  // Calculate total size upfront
  size_t total_size = sizeof(size_t) + stats.path.string().size() +
                      2 * sizeof(size_t) + sizeof(size_t) +
                      stats.links.size() * sizeof(size_t) + sizeof(size_t) +
                      stats.headings.size() * (sizeof(size_t) + sizeof(size_t));

  for (const auto &url : stats.links) {
    total_size += url.toString().size();
  }

  for (const auto &[level, heading] : stats.headings) {
    total_size += heading.size();
  }

  buffer.reserve(total_size);

  // Add path
  const auto path = stats.path.string();
  const size_t pathLen = path.size();
  buffer.resize(sizeof(size_t) + pathLen);
  std::memcpy(buffer.data(), &pathLen, sizeof(size_t));
  std::memcpy(buffer.data() + sizeof(size_t), path.data(), path.size());

  // Add counts
  size_t offset = buffer.size();
  buffer.resize(offset + 2 * sizeof(size_t));
  std::memcpy(buffer.data() + offset, &stats.images, sizeof(size_t));
  std::memcpy(buffer.data() + offset + sizeof(size_t), &stats.forms,
              sizeof(size_t));
  offset += 2 * sizeof(size_t);

  // Add links
  // link_count, link_1_len, link_1, link_2_len, link_2, ...
  size_t linksOffset = buffer.size();
  size_t link_count = stats.links.size();
  buffer.resize(linksOffset + sizeof(size_t));
  std::memcpy(buffer.data() + linksOffset, &link_count, sizeof(size_t));

  for (const auto &url : stats.links) {
    const auto link = url.toString();
    const auto linkLen = link.size();

    auto offset = buffer.size();
    buffer.resize(offset + sizeof(size_t) + linkLen);

    std::memcpy(buffer.data() + offset, &linkLen, sizeof(size_t));
    offset += sizeof(size_t);

    std::memcpy(buffer.data() + offset, link.data(), linkLen);
    offset += linkLen;
  }

  // Add headings
  // heading_count, heading_1_len, heading_1, heading_2_len, heading_2, ...
  size_t headingsOffset = buffer.size();
  size_t heading_count = stats.headings.size();
  buffer.resize(headingsOffset + sizeof(size_t));
  std::memcpy(buffer.data() + headingsOffset, &heading_count, sizeof(size_t));

  for (const auto &[level, heading] : stats.headings) {
    const auto length = heading.size();

    auto offset = buffer.size();
    buffer.resize(offset + sizeof(size_t) + sizeof(level) + length);

    std::memcpy(buffer.data() + offset, &length, sizeof(size_t));
    offset += sizeof(size_t);

    std::memcpy(buffer.data() + offset, &level, sizeof(level));
    offset += sizeof(level);

    std::memcpy(buffer.data() + offset, heading.data(), length);
    offset += length;
  }

  return buffer;
}

// Deserialize message back to HtmlStats
html::Stats deserializeHtmlStats(const std::vector<char> &buffer) {
  html::Stats stats;
  size_t offset = 0;

  // Extract path
  size_t pathLen;
  std::memcpy(&pathLen, buffer.data() + offset, sizeof(size_t));
  offset += sizeof(size_t);

  std::string path(buffer.data() + offset, buffer.data() + offset + pathLen);
  stats.path = path;
  offset += pathLen;

  // Extract counts
  std::memcpy(&stats.images, buffer.data() + offset, sizeof(size_t));
  offset += sizeof(size_t);
  std::memcpy(&stats.forms, buffer.data() + offset, sizeof(size_t));
  offset += sizeof(size_t);

  // Extract links
  size_t link_count;
  std::memcpy(&link_count, buffer.data() + offset, sizeof(size_t));
  offset += sizeof(size_t);

  for (size_t i = 0; i < link_count; i++) {
    size_t linkLen;
    std::memcpy(&linkLen, buffer.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

    std::string link(buffer.data() + offset, buffer.data() + offset + linkLen);
    stats.links.push_back(utils::parseURL(link));
    offset += linkLen;
  }

  // Extract headings
  size_t heading_count;
  std::memcpy(&heading_count, buffer.data() + offset, sizeof(size_t));
  offset += sizeof(size_t);

  for (size_t i = 0; i < heading_count; i++) {
    size_t headingLen;
    std::memcpy(&headingLen, buffer.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

    size_t level;
    std::memcpy(&level, buffer.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

    std::string heading(buffer.data() + offset,
                        buffer.data() + offset + headingLen);
    stats.headings.emplace_back(level, heading);
    offset += headingLen;
  }

  return stats;
}

} // namespace serialization
