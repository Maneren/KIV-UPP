#include "config.hpp"
#include "data.hpp"
#include "file_line.hpp"
#include "outliers.hpp"
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
#include <print>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

Stations read_stations(const std::filesystem::path &input_filepath) {
  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  Stations stations;
  std::string line, field_buffer;

  // Skip the header line
  std::getline(file, line);

  std::ranges::istream_view<FileLine> file_lines(file);

  for (const auto &file_line : file_lines) {
    if (file_line.line.empty()) {
      continue;
    }

    auto line_stream = std::istringstream{file_line.line};

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read id field.");
    }
    size_t id = std::stoul(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read name field.");
    }
    std::string name = std::move(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read longitude field.");
    }
    float longitude = std::stof(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read latitude field.");
    }
    float latitude = std::stof(field_buffer);

    stations.emplace_back(id, name, std::make_pair(latitude, longitude));
  }

  return stations;
}

void fill_measurements(Stations &stations,
                       const std::filesystem::path &input_filepath) {
  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  std::string line, field_buffer;

  // Skip the header line
  std::getline(file, line);

  std::ranges::istream_view<FileLine> file_lines(file);

  for (const auto &file_line : file_lines) {
    if (file_line.line.empty()) {
      continue;
    }

    auto line_stream = std::istringstream{file_line.line};

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read id field.");
    }
    std::size_t id = std::stoul(field_buffer);

    if (id - 1 >= stations.size())
      throw std::runtime_error("Invalid station ID.");

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read ordinal field.");
    }
    std::size_t ordinal = std::stoul(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read year field.");
    }
    Year year = std::stoul(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read month field.");
    }
    Month month = std::stoul(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read day field.");
    }
    Day day = std::stoul(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read value field.");
    }
    Temperature value = std::stof(field_buffer);

    stations[id - 1].measurements.emplace_back(ordinal, year, month, day,
                                               value);
  }
}

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

  std::println("{}", config);

  const auto start = std::chrono::high_resolution_clock::now();

  auto stations = read_stations(config.stations_file());

  fill_measurements(stations, config.measurements_file());

  const auto elapsed = std::chrono::high_resolution_clock::now() - start;

  auto measurements = std::ranges::fold_left(
      stations | std::views::transform(
                     [](auto &station) { return station.measurements.size(); }),
      0, std::plus<>{});

  std::println(
      "Loaded data for {} stations and {} measurements in {} ms. "
      "Processing...",
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

    std::println("Preprocessed {} stations in {} μs", stations.size(),
                 total / TEST_RUNS);
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

    std::println("Calculated stats for {} stations in {} μs", stations.size(),
                 total / TEST_RUNS);
  }

  const auto detector = std::make_unique<SerialOutlierDetector>();

  if (config.mode() == ProcessingMode::Serial) {
    std::ofstream outlier_file("output/vykyvy.csv");
    outlier_file << OUTLIER_FILE_HEADER << std::endl;
    detector->find_outliers(stations, stats, outlier_file);
  } else {
    pool.spawn([&stations, &stats, &detector] {
      std::ofstream outlier_file("output/vykyvy.csv");
      outlier_file << OUTLIER_FILE_HEADER << std::endl;
      detector->find_outliers(stations, stats, outlier_file);
    });
  }

  if constexpr (PERF_TEST) {
    size_t total = 0;
    size_t outlier_count = 0;
    for (size_t i = 0; i < TEST_RUNS; i++) {
      const auto start = std::chrono::high_resolution_clock::now();

      std::ofstream outlier_file("output/vykyvy.csv");
      outlier_file << OUTLIER_FILE_HEADER << std::endl;
      outlier_count = detector->find_outliers(stations, stats, outlier_file);

      total += std::chrono::duration_cast<std::chrono::microseconds>(
                   (std::chrono::high_resolution_clock::now() - start))
                   .count();
    }

    std::println("Detected {} outliers in {} μs", outlier_count,
                 total / TEST_RUNS);
  }

  const auto renderer =
      choose_by_mode<Renderer, SerialRenderer, ParallelRenderer>(config.mode());

  renderer->render_months(stations, stats);

  if constexpr (PERF_TEST) {
    size_t total = 0;
    for (size_t i = 0; i < TEST_RUNS; i++) {
      auto start = std::chrono::high_resolution_clock::now();

      renderer->render_months(stations, stats);

      total += std::chrono::duration_cast<std::chrono::microseconds>(
                   (std::chrono::high_resolution_clock::now() - start))
                   .count();
    }

    std::println("Rendered SVG files in {} μs", total / TEST_RUNS);
  }

  if constexpr (!PERF_TEST) {
    const auto elapsed =
        std::chrono::high_resolution_clock::now() - processing_start;
    std::println(
        "Processed data in {} μs",
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
  }

  return 0;
}
