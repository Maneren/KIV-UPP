#pragma once

#include "html.h"
#include <ostream>
#include <vector>

struct SiteGraph {
  std::vector<std::string> nodes;
  std::vector<std::pair<std::string, std::string>> edges;
  std::vector<html::Stats> stats;
};

inline std::ostream &operator<<(std::ostream &os, const SiteGraph &graph) {
  for (const auto &node : graph.nodes) {
    os << "\"" << node << "\"\n";
  }
  for (const auto &edge : graph.edges) {
    os << "\"" << edge.first << "\" \"" << edge.second << "\"\n";
  }
  return os;
}
