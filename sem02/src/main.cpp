/**
 * Kostra druhe semestralni prace z predmetu KIV/UPP
 * Soubory a hlavicku upravujte dle sveho uvazeni a nutnosti
 */

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <ostream>
#include <queue>
#include <ranges>
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

// Time to sleep if no message is available
constexpr auto WAIT_TIME = std::chrono::milliseconds(10);

// Message tags
namespace Tag {
enum {
  // New url to process
  URL,
  // The node should terminate
  TERMINATE,
  // Sending back html::Stats
  STATS,
  // Sending back summary
  SUMMARY,
  // Sending back errors
  ERROR
};
} // namespace Tag

// Distribute as much work to workers as possible using the round-robin
// algorithm
//
// Returns true if any work was distributed, false otherwise
bool distribute_work(std::queue<utils::URL> &queue,
                     std::set<std::string> &visited, int &active_workers,
                     int &current_worker, const MPI_Comm &comm) {
  bool success = false;

  while (!queue.empty() && active_workers < MPIConfig::workers) {
    const utils::URL url = queue.front();
    queue.pop();

    const auto path = url.path.string();

    if (visited.find(path) != visited.end()) {
      continue;
    }

    visited.insert(path);
    success = true;

    const auto url_string = url.toString();

    std::cout << "Farmer " << MPIConfig::rank << " sending " << url_string
              << " to worker " << current_worker << std::endl;

    int result = MPI_Send(url_string.data(), url_string.size(), MPI_CHAR,
                          current_worker, Tag::URL, comm);

    if (result != MPI_SUCCESS) {
      throw std::runtime_error(
          std::format("Farmer {} failed to send url to worker {}",
                      MPIConfig::rank, current_worker));
    }

    active_workers++;

    if (++current_worker > MPIConfig::workers)
      current_worker = 1;
  }

  return success;
}

// Creates a graph of the site starting from the given URL
SiteGraph map_site(const utils::URL &start_url, MPI_Comm comm) {
  // ordered sets are used, because we would sort the resulting vectors anyways
  std::set<std::string> visited;
  std::set<std::pair<std::string, std::string>> edge_set;

  std::queue<utils::URL> queue;

  // comparison function for stats, so we can use std::set
  struct stats_less {
    bool operator()(const html::Stats &a, const html::Stats &b) const {
      return a.path < b.path;
    };
  };

  std::set<html::Stats, stats_less> stats_set;

  const auto &domain = start_url.domain;
  const auto &scheme = start_url.scheme;
  const auto &path = start_url.path;

  queue.push(start_url);

  int active_workers = 0;
  int current_worker = 1;

  std::vector<char> recv_buffer;

  // While we can distribute work to workers or we have any active workers
  while (
      distribute_work(queue, visited, active_workers, current_worker, comm) ||
      active_workers > 0) {

    // Wait for stats from a worker
    MPI_Status status;
    int message_size;

    MPI_Probe(MPI_ANY_SOURCE, Tag::STATS, comm, &status);
    MPI_Get_count(&status, MPI_CHAR, &message_size);

    recv_buffer.resize(message_size);

    int result =
        MPI_Recv(recv_buffer.data(), message_size, MPI_CHAR, status.MPI_SOURCE,
                 Tag::STATS, comm, MPI_STATUS_IGNORE);

    if (result != MPI_SUCCESS) {
      throw std::runtime_error(
          std::format("Farmer {} failed to receive stats from worker {}",
                      MPIConfig::rank, status.MPI_SOURCE));
    }

    auto stats = serialization::deserializeHtmlStats(recv_buffer);

    std::cout << "Farmer " << MPIConfig::rank << " received stats for "
              << stats.path << " (" << recv_buffer.size() << " bytes)"
              << std::endl;

    const auto &page_path = stats.path;
    const auto page_parent = page_path.parent_path();

    // Add new links to the queue
    for (auto &link : stats.links) {
      if (link.scheme.empty())
        link.scheme = scheme;

      if (link.domain.empty())
        link.domain = domain;
      else if (link.domain != domain)
        continue;

      // handle relative links
      link.path = (page_parent / link.path).lexically_normal();

      // don't crawl outside of the start url
      if (!utils::path_is_inside(link.path, path))
        continue;

      // ignore links to the same page
      if (link.path == page_path)
        continue;

      queue.push(link);

      edge_set.emplace(page_path, link.path.string());
    }

    stats_set.emplace(stats);

    active_workers--;
  }

  std::vector nodes(visited.begin(), visited.end());
  std::vector edges(edge_set.begin(), edge_set.end());
  std::vector stats(stats_set.begin(), stats_set.end());

  return {nodes, edges, stats};
}

void process(const std::vector<std::string> &URLs, std::string &vystup) {
  const auto clean_urls =
      URLs | views::transform(utils::strip) |
      views::filter([](const auto &url) { return !url.empty(); }) |
      ranges::to<std::vector>();

  std::stringstream oss;

  oss << "Zadali jste: <ul>";

  for (const auto &url : clean_urls) {
    oss << "<li>" << url << "</li>";
  }

  oss << "</ul>";

  vystup = oss.str();

  const auto folders =
      clean_urls | views::transform([](const auto &url) {
        return std::format("./results/{:%Y_%m_%d_%H_%M}_{}",
                           chrono::utc_clock::now(), utils::safeURL(url));
      }) |
      ranges::to<std::vector>();

  // Send URLs to farmers
  static int farmer = 1;
  for (const auto &[url, folder] : views::zip(clean_urls, folders)) {
    std::cout << "Master is sending " << url << " (" << url.size()
              << " bytes) to farmer " << farmer << std::endl;

    const auto now = chrono::utc_clock::now();
    std::ofstream logfile(folder + "/log.txt");

    int result = MPI_Send(url.data(), url.size(), MPI_CHAR, farmer, Tag::URL,
                          MPI_COMM_WORLD);

    if (result != MPI_SUCCESS) {
      logfile << "ERROR:‌ Master failed to send url" << farmer << std::endl;
    }

    if (++farmer > MPIConfig::farmers) {
      farmer = 1;
    }

    std::filesystem::create_directories(folder);

    logfile << std::format("{:%Y-%m-%d %H:%M:%S}", now) << std::endl;
  }

  // Receive results from all farmers
  std::vector<char> buffer;
  for (const auto &[url, folder] : views::zip(clean_urls, folders)) {
    std::ofstream logfile(folder + "/log.txt", std::ofstream::app);

    MPI_Status status;
    int message_size;

    MPI_Probe(MPI_ANY_SOURCE, Tag::SUMMARY, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_CHAR, &message_size);

    if (status.MPI_TAG == Tag::ERROR) {
      std::string error(message_size, '\0');
      MPI_Recv(error.data(), error.size(), MPI_CHAR, status.MPI_SOURCE,
               Tag::ERROR, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      std::cerr << "Master received error from farmer " << status.MPI_SOURCE
                << ": " << error;
      logfile << "ERROR:‌ " << error << std::endl;
      continue;
    }

    buffer.resize(message_size);
    int result =
        MPI_Recv(buffer.data(), buffer.size(), MPI_CHAR, MPI_ANY_SOURCE,
                 Tag::SUMMARY, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (result != MPI_SUCCESS) {
      std::cerr << "Master failed to receive graph from farmer "
                << status.MPI_SOURCE << std::endl;

      logfile << "ERROR:‌ Failed to receive graph" << std::endl;
      continue;
    }

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
    logfile << std::format("{:%Y-%m-%d %H:%M:%S}", now) << std::endl;
    logfile << "OK" << std::endl;
  }
}

void terminate_all() {
  // Send termination signal to all nodes in the world
  for (int i = 1; i < MPIConfig::total; i++) {
    MPI_Send(nullptr, 0, MPI_INT, i, Tag::TERMINATE, MPI_COMM_WORLD);
  }
}

int master(MPI_Comm &farmer_worker_comm) {
  // only to synchronize, we don't use this in master
  MPI_Comm_split(MPI_COMM_WORLD, 0, 0, &farmer_worker_comm);

  // inicializace serveru
  CServer svr;
  if (!svr.Init("./data", "0.0.0.0", 8001)) {
    std::cerr << "Nelze inicializovat server!" << std::endl;
    return EXIT_FAILURE;
  }

  // registrace callbacku pro zpracovani odeslanych URL
  svr.RegisterFormCallback(process);

  // spusteni serveru
  const auto status = svr.Run() ? EXIT_SUCCESS : EXIT_FAILURE;

  terminate_all();

  return status;
}

void farmer(MPI_Comm &worker_comm) {
  // initialize farmer
  int color = MPIConfig::rank;
  MPI_Comm_split(MPI_COMM_WORLD, color, 0, &worker_comm);

  std::cout << "Farmer " << MPIConfig::rank << " initialized with color "
            << color << std::endl;

  // Receive URLs from master until we get a termination signal
  std::string url;
  while (true) {
    MPI_Status status;
    int message_size;
    int message_available = 0;

    MPI_Iprobe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &message_available, &status);
    if (!message_available) {
      std::this_thread::sleep_for(WAIT_TIME);
      continue;
    }

    if (status.MPI_TAG == Tag::TERMINATE) {
      MPI_Recv(nullptr, 0, MPI_CHAR, 0, Tag::TERMINATE, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);

      for (int i = 1; i < MPIConfig::workers; i++) {
        MPI_Send(nullptr, 0, MPI_INT, i, Tag::TERMINATE, worker_comm);
      }

      break;
    }

    MPI_Get_count(&status, MPI_CHAR, &message_size);

    url.resize(message_size);
    int result = MPI_Recv(url.data(), url.size(), MPI_CHAR, 0, Tag::URL,
                          MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if (result != MPI_SUCCESS) {
      std::string error = std::format(
          "Farmer {} failed to receive URL from master", MPIConfig::rank);
      MPI_Send(error.data(), error.size(), MPI_CHAR, 0, Tag::ERROR,
               MPI_COMM_WORLD);
      continue;
    }

    std::cout << "Farmer " << MPIConfig::rank << " is processing " << url
              << std::endl;

    SiteGraph graph;
    try {
      graph = map_site(utils::parseURL(url), worker_comm);
    } catch (const std::exception &e) {
      std::string error = e.what();
      MPI_Send(error.data(), error.size(), MPI_CHAR, 0, Tag::ERROR,
               MPI_COMM_WORLD);
      continue;
    }

    const auto serialized = serialization::serializeSiteGraph(graph);

    std::cout << "Farmer " << MPIConfig::rank << " serialized graph for " << url
              << " (" << serialized.size() << " bytes)" << std::endl;

    MPI_Send(serialized.data(), serialized.size(), MPI_CHAR, 0, Tag::SUMMARY,
             MPI_COMM_WORLD);
  }
}

void worker(MPI_Comm &farmer_comm) {
  // initialize worker - color matches its respective farmer
  int color =
      (MPIConfig::rank - 1 - MPIConfig::farmers) / MPIConfig::workers + 1;
  MPI_Comm_split(MPI_COMM_WORLD, color, MPIConfig::rank, &farmer_comm);

  int comm_rank;
  MPI_Comm_rank(farmer_comm, &comm_rank);

  std::cout << "Worker " << MPIConfig::rank << " initialized with color "
            << color << " and rank " << comm_rank << std::endl;

  // receive URLs from master
  std::string buffer;
  while (true) {
    MPI_Status status;
    int message_available = 0;

    MPI_Iprobe(0, MPI_ANY_TAG, farmer_comm, &message_available, &status);
    if (!message_available) {
      std::this_thread::sleep_for(WAIT_TIME);
      continue;
    }

    if (status.MPI_TAG == Tag::TERMINATE) {
      MPI_Recv(nullptr, 0, MPI_CHAR, 0, Tag::TERMINATE, farmer_comm,
               MPI_STATUS_IGNORE);
      std::cout << "Worker " << MPIConfig::rank
                << " received a termination signal and is shutting down"
                << std::endl;
      break;
    }

    int message_size;
    MPI_Get_count(&status, MPI_CHAR, &message_size);

    buffer.resize(message_size);
    int result = MPI_Recv(buffer.data(), message_size, MPI_CHAR, 0, Tag::URL,
                          farmer_comm, MPI_STATUS_IGNORE);

    if (result != MPI_SUCCESS) {
      std::string error = std::format(
          "Worker {} failed to receive URL from master", MPIConfig::rank);
      MPI_Send(error.data(), error.size(), MPI_CHAR, 0, Tag::ERROR,
               farmer_comm);
      continue;
    }

    std::cout << "Worker " << MPIConfig::rank << " received " << buffer << " ("
              << buffer.size() << " bytes)" << std::endl;

    // parse received URL
    const auto url = utils::parseURL(buffer);

    const auto stats = html::parse(url);

    // Send results back to farmer
    const auto serialized = serialization::serializeHtmlStats(stats);

    std::cout << "Worker " << MPIConfig::rank << " parsed stats for " << buffer
              << " (" << serialized.size() << " bytes)" << std::endl;
    MPI_Send(serialized.data(), serialized.size(), MPI_CHAR, 0, Tag::STATS,
             farmer_comm);
  }
}

int main(int argc, char **argv) {
  std::cout << "Starting MPI..." << std::endl;
  MPI_Init(&argc, &argv);

  MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

  MPI_Comm_rank(MPI_COMM_WORLD, &MPIConfig::rank);
  MPI_Comm_size(MPI_COMM_WORLD, &MPIConfig::total);

  if (argc != 5) {
    std::cerr << "Invalid number of arguments! Expected 4, but got " << argc
              << std::endl;
    return EXIT_FAILURE;
  }

  MPIConfig::farmers = std::atol(argv[2]);
  MPIConfig::workers = std::atol(argv[4]);

  std::cout << "Rank " << MPIConfig::rank << "/" << MPIConfig::total
            << " online" << std::endl;

  if (MPIConfig::farmers == 0 || MPIConfig::workers == 0) {
    std::cerr << "Number of farmers and workers can't be zero!" << std::endl;
    return EXIT_FAILURE;
  }

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

  // Farmer-worker communicator
  // Color 0 is reserved for master and not used otherwise
  // Color 1 is farmer 1 and his workers
  // Color 2 is farmer 2 and his workers
  // etc.
  MPI_Comm farmer_worker_comm;

  if (MPIConfig::rank == 0) {
    const auto status = master(farmer_worker_comm);

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
