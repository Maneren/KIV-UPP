#include <cstring>
#include <ranges>
#include <vector>

#include "data.h"
#include "serialization.h"

namespace serialization {
// Convert HtmlStats to a message that can be sent over MPI
std::vector<char> serializeHtmlStats(const html::Stats &stats) {
  // Calculate total size upfront
  auto total_size =
      sizeof(size_t) +                      // path size
      stats.path.string().size() +          // path
      2 * sizeof(size_t) +                  // images and forms counts
      sizeof(size_t) +                      // scheme size
      sizeof(size_t) +                      // domain size
      sizeof(size_t) +                      // links count
      stats.links.size() * sizeof(size_t) + // links sizes
      sizeof(size_t) +                      // headings count
      stats.headings.size() *
          (sizeof(size_t) + sizeof(unsigned char)); // headings sizes and levels

  // send scheme and domain only once, rather than for each link
  std::string scheme;
  std::string domain;

  // stringified link paths
  std::vector<std::string> links;

  for (const auto &url : stats.links) {
    if (scheme.empty()) {
      scheme = url.scheme;
    } else if (url.scheme != scheme) {
      throw std::invalid_argument(
          "All urls in HtmlStats must have the same scheme");
    }

    if (domain.empty()) {
      domain = url.domain;
    } else if (url.domain != domain) {
      throw std::invalid_argument(
          "All urls in HtmlStats must have the same domain");
    }

    links.emplace_back(url.path.string());
    total_size += links.back().size();
  }

  total_size += domain.size() + scheme.size();

  for (const auto &[_, heading] : stats.headings) {
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
  const auto path_length = path.size();

  write(&path_length, sizeof(path_length));
  write(path.data(), path.size());

  // Add counts
  write(&stats.images, sizeof(stats.images));
  write(&stats.forms, sizeof(stats.forms));

  // Add scheme and domain
  const auto scheme_length = scheme.size();
  const auto domain_length = domain.size();
  write(&scheme_length, sizeof(scheme_length));
  write(scheme.data(), scheme_length);
  write(&domain_length, sizeof(domain_length));
  write(domain.data(), domain_length);

  // Add links
  // link_count, link_1_len, link_1, link_2_len, link_2, ...
  const auto link_count = stats.links.size();
  write(&link_count, sizeof(link_count));

  for (const auto &link : links) {
    const auto length = link.size();

    write(&length, sizeof(length));
    write(link.data(), length);
  }

  // Add headings
  // heading_count, heading_1_len, heading_1, heading_2_len, heading_2, ...
  const auto heading_count = stats.headings.size();
  write(&heading_count, sizeof(heading_count));

  for (const auto &[level, heading] : stats.headings) {
    const auto length = heading.size();

    write(&length, sizeof(length));
    write(&level, sizeof(level));
    write(heading.data(), length);
  }

  return buffer;
}

// Deserialize message back to HtmlStats
html::Stats deserializeHtmlStats(const std::vector<char> &buffer) {
  html::Stats stats;

  char const *ptr = buffer.data();
  const auto read = [&ptr]<typename T>(T *dest) {
    std::memcpy(dest, ptr, sizeof(T));
    ptr += sizeof(T);
  };

  // Extract path
  size_t path_len;
  read(&path_len);

  stats.path = std::string(ptr, ptr + path_len);
  ptr += path_len;

  // Extract counts
  read(&stats.images);
  read(&stats.forms);

  // Extract scheme and domain
  size_t scheme_len;
  read(&scheme_len);

  std::string scheme(ptr, ptr + scheme_len);
  ptr += scheme_len;

  size_t domain_len;
  read(&domain_len);

  std::string domain(ptr, ptr + domain_len);
  ptr += domain_len;

  // Extract links
  size_t link_count;
  read(&link_count);

  for (size_t i = 0; i < link_count; i++) {
    size_t length;
    read(&length);

    std::string link(ptr, ptr + length);
    ptr += length;

    stats.links.push_back(utils::parseURL(link));

    auto &url = stats.links.back();
    url.scheme = scheme;
    url.domain = domain;
  }

  // Extract headings
  size_t heading_count;
  read(&heading_count);

  for (size_t i = 0; i < heading_count; i++) {
    size_t length;
    read(&length);

    unsigned char level;
    read(&level);

    std::string heading(ptr, ptr + length);
    ptr += length;

    stats.headings.emplace_back(level, heading);
  }

  return stats;
}

std::vector<char> serializeSiteGraph(const SiteGraph &graph) {
  namespace views = std::views;
  namespace ranges = std::ranges;

  auto total_size = sizeof(size_t) +                          // nodes count
                    graph.nodes.size() * sizeof(size_t) +     // nodes sizes
                    sizeof(size_t) +                          // edges count
                    graph.edges.size() * 2 * sizeof(size_t) + // edges sizes
                    sizeof(size_t) +                          // stats count
                    graph.stats.size() * sizeof(size_t);      // stats sizes

  for (const auto &node : graph.nodes) {
    total_size += node.size();
  }

  const auto serialized_stats = graph.stats |
                                views::transform(serializeHtmlStats) |
                                ranges::to<std::vector>();

  for (const auto &stat : serialized_stats) {
    total_size += stat.size();
  }

  std::vector<char> buffer(total_size);

  char *ptr = buffer.data();
  const auto write = [&ptr](const void *src, size_t n) {
    std::memcpy(ptr, src, n);
    ptr += n;
  };

  // Write nodes
  const auto nodes_count = graph.nodes.size();
  write(&nodes_count, sizeof(nodes_count));

  for (const auto &node : graph.nodes) {
    const auto length = node.size();
    write(&length, sizeof(length));
    write(node.data(), length);
  }

  // Write edges as pairs of indices into nodes vector to save space
  const auto edges_count = graph.edges.size();
  write(&edges_count, sizeof(edges_count));

  for (const auto &[first, second] : graph.edges) {
    const auto first_index =
        std::distance(graph.nodes.begin(), ranges::find(graph.nodes, first));

    const auto second_index =
        std::distance(graph.nodes.begin(), ranges::find(graph.nodes, second));

    write(&first_index, sizeof(first_index));
    write(&second_index, sizeof(second_index));
  }

  // Write stats
  const auto stats_count = graph.stats.size();
  write(&stats_count, sizeof(stats_count));

  for (const auto &stat : serialized_stats) {
    const auto stats_length = stat.size();
    write(&stats_length, sizeof(stats_length));
    write(stat.data(), stats_length);
  }

  return buffer;
}

SiteGraph deserializeSiteGraph(const std::vector<char> &buffer) {
  SiteGraph graph;

  char const *ptr = buffer.data();
  const auto read = [&ptr]<typename T>(T *dest) {
    std::memcpy(dest, ptr, sizeof(T));
    ptr += sizeof(T);
  };

  // Extract nodes
  size_t nodes_count;
  read(&nodes_count);

  for (size_t i = 0; i < nodes_count; i++) {
    size_t length;
    read(&length);

    graph.nodes.emplace_back(ptr, ptr + length);
    ptr += length;
  }

  // Extract edges
  size_t edges_count;
  read(&edges_count);

  for (size_t i = 0; i < edges_count; i++) {
    size_t first_index;
    read(&first_index);

    size_t second_index;
    read(&second_index);

    graph.edges.emplace_back(graph.nodes[first_index],
                             graph.nodes[second_index]);
  }

  // Extract stats
  size_t stats_count;
  read(&stats_count);

  for (size_t i = 0; i < stats_count; i++) {
    size_t length;
    read(&length);

    std::vector<char> buffer(ptr, ptr + length);
    ptr += length;

    graph.stats.push_back(deserializeHtmlStats(buffer));
  }

  return graph;
}

} // namespace serialization
