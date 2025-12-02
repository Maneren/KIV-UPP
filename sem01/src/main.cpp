#include "config.hpp"
#include "data.hpp"
#include "outliers.hpp"
#include "parsing.hpp"
#include "preprocessor.hpp"
#include "renderer.hpp"
#include "stats.hpp"
#include "threadpool.hpp"
#include "utils.hpp"
#include <cstddef>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <vector>

inline constexpr bool PERF_TEST = (
#ifdef PERF_TEST_MACRO
    true
#else
    false
#endif
);

constexpr size_t TEST_RUNS = 100;

int main(int argc, char *argv[]) {
  Config config;
  try {
    config = Config(argc, argv);
  } catch (std::invalid_argument &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  std::cout << std::format("{}\n", config);

  const auto start = std::chrono::high_resolution_clock::now();

  auto stations = read_stations(config.stations_file());

  fill_measurements(stations, config.measurements_file(),
                    config.mode() == Parallel);

  const auto elapsed = std::chrono::high_resolution_clock::now() - start;

  const auto measurements = std::ranges::fold_left(
      stations | std::views::transform([](const auto &station) {
        return station.measurements.size();
      }),
      0, std::plus<>{});

  std::cout << std::format(
      "Loaded data for {} stations and {} measurements in {} ms. "
      "Processing...\n",
      stations.size(), measurements,
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());

  const auto processing_start = std::chrono::high_resolution_clock::now();

  const auto preprocessor =
      choose_by_mode<Preprocessor, SerialPreprocessor, ParallelPreprocessor>(
          config.mode());

  Stations stations_copy;
  if constexpr (PERF_TEST) {
    stations_copy = stations;
  }

  preprocessor->preprocess_data(stations);
  stations.shrink_to_fit();

  if constexpr (PERF_TEST) {
    size_t total = 0;
    for (size_t i = 0; i < TEST_RUNS; i++) {
      auto stations_test = stations_copy;
      const auto start = std::chrono::high_resolution_clock::now();

      preprocessor->preprocess_data(stations_test);
      stations_test.shrink_to_fit();

      total += std::chrono::duration_cast<std::chrono::microseconds>(
                   (std::chrono::high_resolution_clock::now() - start))
                   .count();
    }

    std::cout << std::format("Preprocessed {} stations in {} μs\n",
                             stations.size(), total / TEST_RUNS);
  }

  const auto stats_calculator =
      choose_by_mode<Stats, SerialStats, ParallelStats>(config.mode());

  const auto stats = stats_calculator->monthly_stats(stations);

  if constexpr (PERF_TEST) {
    size_t total = 0;
    for (size_t i = 0; i < TEST_RUNS; i++) {
      const auto start = std::chrono::high_resolution_clock::now();

      const auto stats_test = stats_calculator->monthly_stats(stations);

      total += std::chrono::duration_cast<std::chrono::microseconds>(
                   (std::chrono::high_resolution_clock::now() - start))
                   .count();
    }

    std::cout << std::format("Calculated stats for {} stations in {} μs\n",
                             stations.size(), total / TEST_RUNS);
  }

  const auto detector = std::make_unique<SerialOutlierDetector>();

  const auto stats_ptr =
      std::make_shared<std::vector<StationMonthlyStats>>(stats);

  if (config.mode() == ProcessingMode::Serial) {
    std::ofstream outlier_file("output/vykyvy.csv");
    outlier_file << OUTLIER_FILE_HEADER << std::endl;
    detector->find_outliers(stations, stats, outlier_file);
  } else {
    threadpool::pool.spawn([&stations, stats_ptr, &detector] {
      std::ofstream outlier_file("output/vykyvy.csv");
      outlier_file << OUTLIER_FILE_HEADER << std::endl;
      detector->find_outliers(stations, *stats_ptr, outlier_file);
    });
  }

  if constexpr (PERF_TEST) {
    size_t total = 0;
    size_t outlier_count = 0;
    for (size_t i = 0; i < TEST_RUNS; i++) {
      const auto start = std::chrono::high_resolution_clock::now();

      std::ofstream outlier_file("output/vykyvy.csv");
      outlier_file << OUTLIER_FILE_HEADER << std::endl;
      outlier_count =
          detector->find_outliers(stations, *stats_ptr, outlier_file);

      total += std::chrono::duration_cast<std::chrono::microseconds>(
                   (std::chrono::high_resolution_clock::now() - start))
                   .count();
    }

    std::cout << std::format("Detected {} outliers in {} μs\n", outlier_count,
                             total / TEST_RUNS);
  }

  const auto renderer =
      choose_by_mode<Renderer, SerialRenderer, ParallelRenderer>(config.mode());

  renderer->render_months(stations, *stats_ptr);

  if constexpr (PERF_TEST) {
    size_t total = 0;
    for (size_t i = 0; i < TEST_RUNS; i++) {
      auto start = std::chrono::high_resolution_clock::now();

      renderer->render_months(stations, *stats_ptr);

      total += std::chrono::duration_cast<std::chrono::microseconds>(
                   (std::chrono::high_resolution_clock::now() - start))
                   .count();
    }

    std::cout << std::format("Rendered SVG files in {} μs\n",
                             total / TEST_RUNS);
  }

  threadpool::pool.join();

  if constexpr (!PERF_TEST) {
    const auto elapsed =
        std::chrono::high_resolution_clock::now() - processing_start;
    std::cout << std::format(
        "Processed data in {} μs\n",
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
  }

  return 0;
}
