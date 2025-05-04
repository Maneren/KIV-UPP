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
#include <mpi.h>
#include <ostream>
#include <queue>
#include <ranges>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "data.h"
#include "html.h"
#include "serialization.h"
#include "server.h"
#include "utils.h"

namespace ranges = std::ranges;
namespace views = std::views;
namespace chrono = std::chrono;

namespace MPIConfig {
static int rank, total;
static int farmers, workers;
} // namespace MPIConfig

enum Tags { URL_TAG, TERMINATE_TAG, STATS_TAG, SUMMARY_TAG };

void distribute_work(std::queue<utils::URL> &queue,
                     std::set<std::string> &visited, size_t &available_workers,
                     size_t &active_workers, int &current_worker,
                     MPI_Comm comm) {
  while (!queue.empty() && available_workers > 0) {
    utils::URL url = queue.front();
    queue.pop();

    const auto path = url.path.string();

    if (visited.find(path) != visited.end()) {
      continue;
    } else {
      visited.insert(path);
    }

    const auto url_string = url.toString();

    std::cout << "Farmer " << MPIConfig::rank << " sending " << url_string
              << " to worker " << current_worker << std::endl;

    MPI_Send(url_string.data(), url_string.size(), MPI_CHAR, current_worker,
             URL_TAG, comm);

    available_workers--;
    active_workers++;

    if (++current_worker > MPIConfig::workers)
      current_worker = 1;
  }
}

SiteGraph map_site(const utils::URL &start_url, MPI_Comm comm) {
  std::set<std::string> visited;
  std::set<std::pair<std::string, std::string>> edge_set;

  std::queue<utils::URL> queue;

  std::vector<html::Stats> site_stats;

  const auto &domain = start_url.domain;
  const auto &scheme = start_url.scheme;

  queue.push(start_url);

  size_t active_workers = 0;
  size_t available_workers = MPIConfig::workers;
  int current_worker = 1;

  std::vector<char> recv_buffer;

  while (true) {
    // Distribute work to available workers
    distribute_work(queue, visited, available_workers, active_workers,
                    current_worker, comm);

    // If all work is done, break
    if (queue.empty() && active_workers == 0)
      break;

    MPI_Status status;
    int message_size;

    MPI_Probe(MPI_ANY_SOURCE, STATS_TAG, comm, &status);
    MPI_Get_count(&status, MPI_CHAR, &message_size);

    recv_buffer.resize(message_size);

    MPI_Recv(recv_buffer.data(), message_size, MPI_CHAR, status.MPI_SOURCE,
             STATS_TAG, comm, MPI_STATUS_IGNORE);

    auto stats = serialization::deserializeHtmlStats(recv_buffer);

    std::cout << "Farmer " << MPIConfig::rank << " received stats for "
              << stats.path << " (" << recv_buffer.size() << " bytes)"
              << std::endl;

    recv_buffer.clear();

    const auto &page_path = stats.path;

    // Add new links to the queue
    for (auto &link : stats.links) {
      if (link.scheme.empty())
        link.scheme = scheme;

      if (link.domain.empty())
        link.domain = domain;

      if (link.domain != domain)
        continue;

      link.path = (page_path.parent_path() / link.path).lexically_normal();

      if (link.path == page_path)
        continue;

      queue.push(link);

      edge_set.emplace(page_path, link.path.string());
    }

    site_stats.emplace_back(stats);

    available_workers++;
    active_workers--;
  }

  std::vector nodes(visited.begin(), visited.end());
  std::vector edges(edge_set.begin(), edge_set.end());

  ranges::sort(nodes);
  ranges::sort(edges);
  ranges::sort(site_stats,
               [](const auto &a, const auto &b) { return a.path < b.path; });

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

  const static std::regex whitespace(R"(^\s+|\s+$)");

  const auto clean_urls =
      URLs | views::transform(utils::strip) | ranges::to<std::vector>();

  const auto folders =
      clean_urls | views::transform([](const auto &url) {
        return std::format("./results/{:%Y_%m_%d_%H_%M}_{}",
                           chrono::utc_clock::now(), utils::safeURL(url));
      }) |
      ranges::to<std::vector>();

  static int farmer = 1;
  for (const auto &[url, folder] : views::zip(clean_urls, folders)) {

    std::cout << "Master is sending " << url << " (" << url.size()
              << " bytes) to farmer " << farmer << std::endl;

    const auto now = chrono::utc_clock::now();

    MPI_Send(url.data(), url.size(), MPI_CHAR, farmer, URL_TAG, MPI_COMM_WORLD);

    if (++farmer > MPIConfig::farmers) {
      farmer = 1;
    }

    std::filesystem::create_directories(folder);

    std::ofstream logfile(folder + "/log.txt");
    logfile << std::format("{:%Y-%m-%d %H:%M:%S}", now) << std::endl;
  }

  // Receive results from all farmers
  std::vector<char> buffer;
  for (const auto &[url, folder] : views::zip(clean_urls, folders)) {
    MPI_Status status;
    int message_size;

    MPI_Probe(MPI_ANY_SOURCE, SUMMARY_TAG, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_CHAR, &message_size);

    buffer.resize(message_size);
    MPI_Recv(buffer.data(), buffer.size(), MPI_CHAR, MPI_ANY_SOURCE,
             SUMMARY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    std::cout << "Master received graph of " << url << " (" << buffer.size()
              << " bytes)" << std::endl;

    const auto graph = serialization::deserializeSiteGraph(buffer);

    std::ofstream file(folder + "/map.txt");
    file << graph;

    std::ofstream stats_file(folder + "/contents.txt");
    for (const auto &stats : graph.stats) {
      stats_file << stats << std::endl;
    }

    const auto now = chrono::utc_clock::now();
    std::ofstream logfile(folder + "/log.txt", std::ofstream::app);
    logfile << std::format("{:%Y-%m-%d %H:%M:%S}", now) << std::endl;
    logfile << "OK" << std::endl;
  }
}

void farmer(MPI_Comm &worker_comm) {
  int color = MPIConfig::rank;
  MPI_Comm_split(MPI_COMM_WORLD, color, 0, &worker_comm);

  std::cout << "Farmer " << MPIConfig::rank << " initialized with color "
            << color << std::endl;

  std::string url;
  while (true) {
    MPI_Status status;
    int message_size;
    int message_available = 0;

    MPI_Iprobe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &message_available, &status);
    if (!message_available) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    if (status.MPI_TAG == TERMINATE_TAG) {
      MPI_Recv(nullptr, 0, MPI_CHAR, 0, TERMINATE_TAG, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);

      for (int i = 1; i < MPIConfig::workers; i++) {
        MPI_Send(nullptr, 0, MPI_INT, i, TERMINATE_TAG, worker_comm);
      }

      break;
    }

    MPI_Get_count(&status, MPI_CHAR, &message_size);

    url.resize(message_size);
    MPI_Recv(url.data(), url.size(), MPI_CHAR, 0, URL_TAG, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    if (size_t pos = url.find_last_not_of('\0'); pos != std::string::npos) {
      url.resize(pos + 1);
    }

    std::cout << "Farmer " << MPIConfig::rank << " is processing " << url
              << std::endl;

    const auto graph = map_site(utils::parseURL(url), worker_comm);

    const auto serialized = serialization::serializeSiteGraph(graph);

    std::cout << "Farmer " << MPIConfig::rank << " serialized graph for " << url
              << " (" << serialized.size() << " bytes)" << std::endl;

    MPI_Send(serialized.data(), serialized.size(), MPI_CHAR, 0, SUMMARY_TAG,
             MPI_COMM_WORLD);

    url.clear();
  }
}

void worker(MPI_Comm &farmer_comm) {
  int color =
      (MPIConfig::rank - 1 - MPIConfig::farmers) / MPIConfig::farmers + 1;
  MPI_Comm_split(MPI_COMM_WORLD, color, MPIConfig::rank, &farmer_comm);

  int comm_rank;
  MPI_Comm_rank(farmer_comm, &comm_rank);

  std::cout << "Worker " << MPIConfig::rank << " initialized with color "
            << color << " and rank " << comm_rank << std::endl;

  std::string buffer;
  while (true) {
    MPI_Status status;
    int message_available = 0;

    MPI_Iprobe(0, MPI_ANY_TAG, farmer_comm, &message_available, &status);
    if (!message_available) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    if (status.MPI_TAG == TERMINATE_TAG) {
      MPI_Recv(nullptr, 0, MPI_CHAR, 0, TERMINATE_TAG, farmer_comm,
               MPI_STATUS_IGNORE);
      std::cout << "Worker " << MPIConfig::rank
                << " received a termination signal and is shutting down"
                << std::endl;
      break;
    }

    int message_size;
    MPI_Get_count(&status, MPI_CHAR, &message_size);

    buffer.resize(message_size);
    MPI_Recv(buffer.data(), message_size, MPI_CHAR, 0, URL_TAG, farmer_comm,
             MPI_STATUS_IGNORE);

    std::cout << "Worker " << MPIConfig::rank << " received " << buffer << " ("
              << buffer.size() << " bytes)" << std::endl;

    const auto url = utils::parseURL(buffer);

    const auto stats = html::parse(url);

    // Send results back to farmer
    const auto serialized = serialization::serializeHtmlStats(stats);

    std::cout << "Worker " << MPIConfig::rank << " parsed stats for " << buffer
              << " (" << serialized.size() << " bytes)" << std::endl;

    MPI_Send(serialized.data(), serialized.size(), MPI_CHAR, 0, STATS_TAG,
             farmer_comm);

    buffer.clear();
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

  MPI_Comm_rank(MPI_COMM_WORLD, &MPIConfig::rank);
  MPI_Comm_size(MPI_COMM_WORLD, &MPIConfig::total);

  MPIConfig::farmers = std::atol(argv[2]);
  MPIConfig::workers = std::atol(argv[4]);

  std::cout << "Rank " << MPIConfig::rank << "/" << MPIConfig::total
            << " online" << std::endl;

  if (MPIConfig::farmers == 0 || MPIConfig::workers == 0) {
    std::cerr << "Number of farmers and workers can't be zero!" << std::endl;
    return EXIT_FAILURE;
  }

  MPI_Barrier(MPI_COMM_WORLD);

  if (MPIConfig::rank == 0) {
    std::cout << "Farmers: " << MPIConfig::farmers << std::endl;
    std::cout << "Workers: " << MPIConfig::workers << " per farmer ("
              << MPIConfig::farmers * MPIConfig::workers << " in total)"
              << std::endl;
  }

  const auto expected =
      1 + MPIConfig::farmers + MPIConfig::farmers * MPIConfig::workers;

  if (expected != MPIConfig::total) {
    std::cerr << "Invalid number of processes! Provided " << MPIConfig::total
              << ", but requested " << expected << std::endl;
    return EXIT_FAILURE;
  }

  MPI_Comm farmer_worker_comm;

  if (MPIConfig::rank == 0) {
    // only to synchronize, we don't use this in master
    MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &farmer_worker_comm);

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

    // Send termination signal to all workers
    for (int i = 1; i < MPIConfig::total; i++) {
      MPI_Send(nullptr, 0, MPI_INT, i, TERMINATE_TAG, MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return status;
  } else if (MPIConfig::rank <= MPIConfig::farmers) { // Farmer nodes
    farmer(farmer_worker_comm);
  } else {
    worker(farmer_worker_comm);
  }

  MPI_Comm_free(&farmer_worker_comm);

  std::cout << "Process " << MPIConfig::rank << " finished" << std::endl;

  MPI_Finalize();

  return EXIT_SUCCESS;
}
