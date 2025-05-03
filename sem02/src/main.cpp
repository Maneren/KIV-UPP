/**
 * Kostra druhe semestralni prace z predmetu KIV/UPP
 * Soubory a hlavicku upravujte dle sveho uvazeni a nutnosti
 */

#include <algorithm>
#include <iostream>
#include <list>
#include <mpi.h>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "server.h"
#include "utils.h"

namespace views = std::ranges::views;
namespace ranges = std::ranges;

struct HtmlStats {
  std::string path;
  size_t images;
  size_t forms;
  std::vector<utils::URL> links;
  std::vector<std::pair<size_t, std::string>> headings;
};

HtmlStats parseHTML(const utils::URL &url) {
  const std::regex img_regex(R"(<img\b)");
  const std::regex form_regex(R"(<form\b)");
  const std::regex link_regex(R"(<a\b[^>]+href="([^>"]+)\")");
  const std::regex heading_regex(R"(<h([1-6])>(.*?)</h\1>)");

  const auto html = utils::downloadHTML(url.toString());

  size_t images =
      std::distance(std::sregex_iterator(html.begin(), html.end(), img_regex),
                    std::sregex_iterator());

  size_t forms =
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

  return {url.pathToString(), images, forms, links, headings};
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

    const auto path = url.pathToString();

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

      if (url.path.size() > 1) {
        link.path.insert(link.path.begin(), url.path.begin(),
                         url.path.end() - 1);
      }

      if (link.path == url.path)
        continue;

      queue.push_back(link);

      edge_set.emplace(path, link.pathToString());
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
      std::cout << "IMAGES " << stat.second.images << std::endl;
      std::cout << "LINKS " << stat.second.links.size() << std::endl;
      std::cout << "FORMS " << stat.second.forms << std::endl;
      for (const auto &heading : stat.second.headings) {
        for (size_t i = 0; i < heading.first; ++i) {
          std::cout << "-";
        }
        std::cout << " " << heading.second << std::endl;
      }
      std::cout << std::endl;
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
