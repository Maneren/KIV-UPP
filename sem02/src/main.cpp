/**
 * Kostra druhe semestralni prace z predmetu KIV/UPP
 * Soubory a hlavicku upravujte dle sveho uvazeni a nutnosti
 */

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <list>
#include <mpi.h>
#include <ostream>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "server.h"
#include "utils.h"

namespace ranges = std::ranges;
namespace views = std::views;
namespace chrono = std::chrono;

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
  const static std::regex img_regex(R"(<img\b)");
  const static std::regex form_regex(R"(<form\b)");
  const static std::regex link_regex(R"(<a\b[^>]+href="([^>"]+)\")");
  const static std::regex heading_regex(R"(<h([1-6])>(.*?)</h\1>)");

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

static int rank, total;
static size_t farmers, workers;

void process(const std::vector<std::string> &URLs, std::string &vystup) {
  std::stringstream oss;

  oss << "Zadali jste: <ul>";

  for (const auto &url : URLs) {
    oss << "<li>" << url << "</li>";
  }

  oss << "</ul>";

  vystup = oss.str();

  const static std::regex whitespace(R"(^\s+|\s+$)");

  const auto clean_urls = URLs | views::transform([](const auto &url) {
                            return std::regex_replace(url, whitespace, "");
                          }) |
                          ranges::to<std::vector>();

  const auto folders =
      clean_urls | views::transform([](const auto &url) {
        return std::format("./results/{:%Y_%m_%d_%H_%M}_{}",
                           chrono::utc_clock::now(), utils::safeURL(url));
      }) |
      ranges::to<std::vector>();

  static size_t farmer = 1;
  for (const auto &[url, folder] : views::zip(clean_urls, folders)) {

    if (url.size() >= 1024)
      throw std::runtime_error("URL too long");

    std::cout << "Sending " << url << " (" << url.size() << " bytes) to farmer "
              << farmer << " out of " << farmers << std::endl;

    const auto now = chrono::utc_clock::now();

    MPI_Send(url.data(), url.size(), MPI_CHAR, farmer++, 0, MPI_COMM_WORLD);
    if (farmer > farmers) {
      farmer = 1;
    }

    std::filesystem::create_directories(folder);

    std::ofstream logfile(folder + "/log.txt");
    logfile << std::format("{:%Y-%m-%d %H:%M:%S}", now) << std::endl;
  }

  // Receive results from all farmers
  for (const auto &[url, folder] : views::zip(clean_urls, folders)) {
    std::string buffer(1024 * 1024, '\0');
    MPI_Recv(buffer.data(), buffer.size(), MPI_CHAR, MPI_ANY_SOURCE, 1,
             MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (size_t pos = buffer.find_last_not_of('\0'); pos != std::string::npos) {
      buffer.resize(pos + 1);
    }

    std::filesystem::create_directories(folder);

    std::cout << "Received " << buffer.size() << " bytes for " << url
              << std::endl;
    std::ofstream file(folder + "/map.txt");
    file << buffer;

    const auto now = chrono::utc_clock::now();
    std::ofstream logfile(folder + "/log.txt");
    logfile << std::format("{:%Y-%m-%d %H:%M:%S}", now) << std::endl;
    logfile << "OK" << std::endl;
  }
}

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cerr << "Invalid arguments! " << "Expected 4, but got " << argc
              << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Starting MPI..." << std::endl;
  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &total);

  farmers = std::atol(argv[2]);
  workers = std::atol(argv[4]);

  std::cout << "Rank " << rank << "/" << total << " online" << std::endl;

  if (rank == 0) {
    std::cout << "Farmers: " << farmers << std::endl;
    std::cout << "Workers: " << workers << std::endl;
  }

  const auto expected = 1 + farmers + farmers * workers;

  if (expected != total) {
    std::cerr << "Invalid number of processes! Provided " << total
              << ", but requested " << expected << std::endl;
    return EXIT_FAILURE;
  }

  if (rank == 0) {
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

    MPI_Finalize();

    return status;
  } else if (rank > 0 && rank <= farmers) { // Farmer nodes
    std::string url(1024, '\0');
    MPI_Recv(url.data(), url.size(), MPI_CHAR, 0, 0, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    if (size_t pos = url.find_last_not_of('\0'); pos != std::string::npos) {
      url.resize(pos + 1);
    }

    const auto graph = map_site(utils::parseURL(url));

    std::stringstream oss;
    oss << graph;
    std::string summary = oss.str();

    std::cout << "Summary for " << url << " has " << summary.size() << " bytes"
              << std::endl;

    MPI_Send(summary.data(), summary.size(), MPI_CHAR, 0, 1, MPI_COMM_WORLD);
  }

  MPI_Finalize();
}
