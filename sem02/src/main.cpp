/**
 * Kostra druhe semestralni prace z predmetu KIV/UPP
 * Soubory a hlavicku upravujte dle sveho uvazeni a nutnosti
 */

#include <algorithm>
#include <iostream>
#include <list>
#include <mpi.h>
#include <ostream>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "server.h"
#include "utils.h"

namespace ranges = std::ranges;

struct HtmlStats {
  std::string path;
  size_t images;
  size_t forms;
  std::vector<utils::URL> links;
  std::vector<std::pair<size_t, std::string>> headings;
};

std::ostream &operator<<(std::ostream &os, const HtmlStats &stats) {
  std::cout << "IMAGES " << stats.images << std::endl;
  std::cout << "LINKS " << stats.links.size() << std::endl;
  std::cout << "FORMS " << stats.forms << std::endl;
  for (const auto &heading : stats.headings) {
    for (size_t i = 0; i < heading.first; ++i) {
      std::cout << "-";
    }
    std::cout << " " << heading.second << std::endl;
  }

  return os;
}

HtmlStats parseHTML(const utils::URL &url) {
  const std::regex img_regex(R"(<img\b)");
  const std::regex form_regex(R"(<form\b)");
  const std::regex link_regex(R"(<a\b[^>]+href="([^>"]+)\")");
  const std::regex heading_regex(R"(<h([1-6])>(.*?)</h\1>)");

  std::cout << "Parsing " << url.toString() << std::endl;
  const auto html = utils::downloadHTML(url.toString());

  const size_t images =
      std::distance(std::sregex_iterator(html.begin(), html.end(), img_regex),
                    std::sregex_iterator());

  const size_t forms =
      std::distance(std::sregex_iterator(html.begin(), html.end(), form_regex),
                    std::sregex_iterator());

  std::vector<utils::URL> links;
  for (std::sregex_iterator it(html.begin(), html.end(), link_regex), end_it;
       it != end_it; ++it) {
    std::smatch match = *it;
    links.push_back(utils::parseURL(match[1]));
  }

  std::vector<std::pair<size_t, std::string>> headings;
  for (std::sregex_iterator it(html.begin(), html.end(), heading_regex), end_it;
       it != end_it; ++it) {
    std::smatch match = *it;
    headings.emplace_back(std::stoi(match[1]), match[2]);
  }

  return {url.path.string(), images, forms, links, headings};
}

struct SiteGraph {
  std::vector<std::string> nodes;
  std::vector<std::pair<std::string, std::string>> edges;
  std::vector<std::pair<std::string, HtmlStats>> stats;
};

std::ostream &operator<<(std::ostream &os, const SiteGraph &graph) {
  for (const auto &node : graph.nodes) {
    os << "\"" << node << "\"\n";
  }
  for (const auto &edge : graph.edges) {
    os << "\"" << edge.first << "\" \"" << edge.second << "\"\n";
  }
  return os;
}

SiteGraph map_site(const utils::URL &start_url) {
  std::set<std::string> visited;
  std::set<std::pair<std::string, std::string>> edge_set;

  std::list<utils::URL> queue;

  std::vector<std::pair<std::string, HtmlStats>> site_stats;

  queue.push_back(start_url);

  while (!queue.empty()) {
    const auto url = queue.front();
    queue.pop_front();

    const auto path = url.path.string();

    if (visited.find(path) != visited.end()) {
      continue;
    } else {
      visited.insert(path);
    }

    auto stats = parseHTML(url);

    for (auto &link : stats.links) {
      if (link.scheme.empty())
        link.scheme = url.scheme;

      if (link.domain.empty())
        link.domain = url.domain;

      if (link.domain != url.domain)
        continue;

      link.path = (url.path.parent_path() / link.path).lexically_normal();

      if (link.path == url.path)
        continue;

      queue.push_back(link);

      edge_set.emplace(path, link.path.string());
    }

    site_stats.push_back(std::make_pair(path, stats));
  }

  std::vector nodes(visited.begin(), visited.end());
  std::vector edges(edge_set.begin(), edge_set.end());

  ranges::sort(nodes);
  ranges::sort(edges);
  ranges::sort(site_stats,
               [](const auto &a, const auto &b) { return a.first < b.first; });

  return {nodes, edges, site_stats};
}

void process(const std::vector<std::string> &URLs, std::string &vystup) {
  std::stringstream oss;

  oss << "Zadali jste: <ul>";

  for (const auto &url : URLs) {
    oss << "<li>" << url << "</li>";
  }

  oss << "</ul>";

  vystup = oss.str();

  for (const auto &url : URLs) {
    const auto graph = map_site(utils::parseURL(url));

    std::cout << graph << std::endl;

    for (const auto &stat : graph.stats) {
      std::cout << "\"" << stat.first << "\"" << std::endl;
      std::cout << stat.second << std::endl;
    }
  }
}

int main(int argc, char **argv) {
  // inicializace serveru
  CServer svr;
  if (!svr.Init("./data", "localhost", 8001)) {
    std::cerr << "Nelze inicializovat server!" << std::endl;
    return EXIT_FAILURE;
  }

  // registrace callbacku pro zpracovani odeslanych URL
  svr.RegisterFormCallback(process);

  // spusteni serveru
  const auto status = svr.Run() ? EXIT_SUCCESS : EXIT_FAILURE;

  return status;
}
