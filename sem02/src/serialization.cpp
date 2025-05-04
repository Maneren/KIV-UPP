#include <cstring>
#include <random>
#include <ranges>
#include <vector>

#include "data.h"
#include "serialization.h"

namespace serialization {
// Convert HtmlStats to a message that can be sent over MPI
std::vector<char> serializeHtmlStats(const html::Stats &stats) {
  // Calculate total size upfront
  size_t total_size =
      sizeof(size_t) +                            // path size
      stats.path.string().size() +                // path
      2 * sizeof(size_t) +                        // images and forms counts
      sizeof(size_t) +                            // links count
      stats.links.size() * sizeof(size_t) +       // links sizes
      sizeof(size_t) +                            // headings count
      stats.headings.size() * 2 * sizeof(size_t); // headings sizes and levels

  // stringified links
  std::vector<std::string> links;

  for (const auto &url : stats.links) {
    links.emplace_back(url.toString());
    total_size += links.back().size();
  }

  for (const auto &[level, heading] : stats.headings) {
    total_size += heading.size();
  }

  std::vector<char> buffer(total_size);

  char *ptr = buffer.data();
  const auto write = [&ptr](const void *src, size_t n) {
    std::memcpy(ptr, src, n);
    ptr += n;
  };

  // Add path
  const auto path = stats.path.string();
  const size_t path_length = path.size();

  write(&path_length, sizeof(size_t));
  write(path.data(), path.size());

  // Add counts
  write(&stats.images, sizeof(size_t));
  write(&stats.forms, sizeof(size_t));

  // Add links
  // link_count, link_1_len, link_1, link_2_len, link_2, ...
  const size_t link_count = stats.links.size();
  write(&link_count, sizeof(size_t));

  for (const auto &link : links) {
    const auto linkLen = link.size();

    write(&linkLen, sizeof(size_t));
    write(link.data(), linkLen);
  }

  // Add headings
  // heading_count, heading_1_len, heading_1, heading_2_len, heading_2, ...
  size_t heading_count = stats.headings.size();
  write(&heading_count, sizeof(size_t));

  for (const auto &[level, heading] : stats.headings) {
    const auto length = heading.size();

    write(&length, sizeof(size_t));
    write(&level, sizeof(level));
    write(heading.data(), length);
  }

  return buffer;
}

// Deserialize message back to HtmlStats
html::Stats deserializeHtmlStats(const std::vector<char> &buffer) {
  html::Stats stats;

  char const *ptr = buffer.data();
  const auto read = [&ptr](void *dest, size_t n) {
    std::memcpy(dest, ptr, n);
    ptr += n;
  };

  // Extract path
  size_t path_len;
  read(&path_len, sizeof(size_t));

  stats.path = std::string(ptr, ptr + path_len);
  ptr += path_len;

  // Extract counts
  read(&stats.images, sizeof(size_t));
  read(&stats.forms, sizeof(size_t));

  // Extract links
  size_t link_count;
  read(&link_count, sizeof(size_t));

  for (size_t i = 0; i < link_count; i++) {
    size_t length;
    read(&length, sizeof(size_t));

    std::string link(ptr, ptr + length);
    ptr += length;

    stats.links.push_back(utils::parseURL(link));
  }

  // Extract headings
  size_t heading_count;
  read(&heading_count, sizeof(size_t));

  for (size_t i = 0; i < heading_count; i++) {
    size_t length;
    read(&length, sizeof(size_t));

    size_t level;
    read(&level, sizeof(size_t));

    std::string heading(ptr, ptr + length);
    ptr += length;

    stats.headings.emplace_back(level, heading);
  }

  return stats;
}

std::vector<char> serializeSiteGraph(const SiteGraph &graph) {
  auto total_size = sizeof(size_t) +                          // nodes count
                    graph.nodes.size() * sizeof(size_t) +     // nodes sizes
                    sizeof(size_t) +                          // edges count
                    graph.edges.size() * 2 * sizeof(size_t) + // edges sizes
                    sizeof(size_t) +                          // stats count
                    graph.stats.size() * sizeof(size_t);      // stats sizes

  for (const auto &node : graph.nodes) {
    total_size += node.size();
  }

  for (const auto &edge : graph.edges) {
    total_size += edge.first.size() + edge.second.size();
  }

  const auto serialized_stats =
      graph.stats | std::views::transform([&total_size](const auto &stats) {
        const auto serialized = serializeHtmlStats(stats);
        total_size += serialized.size();
        return std::move(serialized);
      }) |
      std::ranges::to<std::vector>();

  std::vector<char> buffer(total_size);

  char *ptr = buffer.data();
  const auto write = [&ptr](const void *src, size_t n) {
    std::memcpy(ptr, src, n);
    ptr += n;
  };

  // Write nodes
  const size_t nodes_count = graph.nodes.size();
  write(&nodes_count, sizeof(size_t));

  for (const auto &node : graph.nodes) {
    const auto length = node.size();
    write(&length, sizeof(size_t));
    write(node.data(), length);
  }

  // Write edges
  const size_t edges_count = graph.edges.size();
  write(&edges_count, sizeof(size_t));

  for (const auto &edge : graph.edges) {
    const auto first_length = edge.first.size();
    write(&first_length, sizeof(size_t));
    write(edge.first.data(), first_length);

    const auto second_length = edge.second.size();
    write(&second_length, sizeof(size_t));
    write(edge.second.data(), second_length);
  }

  // Write stats
  const size_t stats_count = graph.stats.size();
  write(&stats_count, sizeof(size_t));

  for (const auto &stat : serialized_stats) {
    const auto stats_length = stat.size();
    write(&stats_length, sizeof(size_t));
    write(stat.data(), stats_length);
  }

  return buffer;
}

SiteGraph deserializeSiteGraph(const std::vector<char> &buffer) {
  SiteGraph graph;

  char const *ptr = buffer.data();
  const auto read = [&ptr](void *dest, size_t n) {
    std::memcpy(dest, ptr, n);
    ptr += n;
  };

  // Extract nodes
  size_t nodes_count;
  read(&nodes_count, sizeof(size_t));

  for (size_t i = 0; i < nodes_count; i++) {
    size_t length;
    read(&length, sizeof(size_t));

    graph.nodes.emplace_back(ptr, ptr + length);
    ptr += length;
  }

  // Extract edges
  size_t edges_count;
  read(&edges_count, sizeof(size_t));

  for (size_t i = 0; i < edges_count; i++) {
    size_t first_length;
    read(&first_length, sizeof(size_t));

    std::string first(ptr, ptr + first_length);
    ptr += first_length;

    size_t second_length;
    read(&second_length, sizeof(size_t));

    std::string second(ptr, ptr + second_length);
    ptr += second_length;

    graph.edges.emplace_back(std::move(first), std::move(second));
  }

  // std::subtract_with_carry_engine<size_t, 32, 2, 4> rng;

  // Extract stats
  size_t stats_count;
  read(&stats_count, sizeof(size_t));

  for (size_t i = 0; i < stats_count; i++) {
    size_t stats_length;
    read(&stats_length, sizeof(size_t));

    std::vector<char> stats_buffer(ptr, ptr + stats_length);
    ptr += stats_length;

    graph.stats.emplace_back(deserializeHtmlStats(stats_buffer));
  }

  return graph;
}

} // namespace serialization
