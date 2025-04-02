#include "config.hpp"
#include "data.hpp"
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
#include <iterator>
#include <memory>
#include <ostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief Converts a string to a number.
 *
 * Simpler and faster version of stoul optimized for raw speed with no error
 * or bounds checking.
 */
size_t str_to_number(auto str) {
  size_t result = 0;
  for (const char c : str) {
    result *= 10;
    result += c - '0';
  }
  return result;
}

/**
 * @brief Converts a string to a double.
 *
 * Simpler and faster version of stod optimized for raw speed with no error
 * or bounds checking.
 */
double str_to_double(auto str) {
  double result = 0.;
  bool dot = false;
  size_t power = 10;

  double signum = 1.;
  if (str[0] == '-') {
    signum = -1.;
    str = str | std::views::drop(1);
  }

  for (const char c : str) {
    if (c == '.') {
      dot = true;
      power = 10;
      continue;
    }

    if (!std::isdigit(c))
      break;

    double digit = c - '0';

    if (!dot) {
      result *= 10;
      result += digit;
    } else {
      result += digit / power;
      power *= 10;
    }
  }

  return result * signum;
}

Stations read_stations(const std::filesystem::path &input_filepath) {
  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  Stations stations;

  std::ostringstream oss;
  oss << file.rdbuf();
  std::string file_string = oss.str();

  for (const auto &file_line :
       std::views::split(file_string, '\n') | std::views::drop(1)) {
    if (file_line.empty()) {
      continue;
    }

    auto tokens = std::views::split(file_line, ';');
    auto iterator = tokens.begin();

    auto id_str = iterator++;
    if (id_str == tokens.end()) {
      throw std::runtime_error("Failed to read id field.");
    }
    size_t id = str_to_number(*id_str);

    auto name_str = iterator++;
    if (name_str == tokens.end()) {
      throw std::runtime_error("Failed to read name field.");
    }
    std::string name = *name_str | std::ranges::to<std::string>();

    auto longitude_str = iterator++;
    if (longitude_str == tokens.end()) {
      throw std::runtime_error("Failed to read longitude field.");
    }
    float longitude = str_to_double(*longitude_str);

    auto latitude_str = iterator++;
    if (latitude_str == tokens.end()) {
      throw std::runtime_error("Failed to read latitude field.");
    }
    float latitude = str_to_double(*latitude_str);

    stations.emplace_back(id, name, std::make_pair(latitude, longitude));
  }

  return stations;
}

void fill_measurements(Stations &stations,
                       const std::filesystem::path &input_filepath) {
  auto size = std::filesystem::file_size(input_filepath);
  std::string file_string(size, '\0');

  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  file.read(file_string.data(), size);

  for (const auto &line :
       std::views::split(file_string, '\n') | std::views::drop(1)) {
    if (line.empty()) {
      continue;
    }

    auto tokens = std::views::split(line, ';');
    auto iterator = tokens.begin();

    auto id_str = iterator++;
    if (id_str == tokens.end()) {
      throw std::runtime_error("Failed to read id field.");
    }
    size_t id = str_to_number(*id_str);

    auto ordinal_str = iterator++;
    if (ordinal_str == tokens.end()) {
      throw std::runtime_error("Failed to read ordinal field.");
    }
    size_t ordinal = str_to_number(*ordinal_str);

    auto year_str = iterator++;
    if (year_str == tokens.end()) {
      throw std::runtime_error("Failed to read year field.");
    }
    Year year = str_to_number(*year_str);

    auto month_str = iterator++;
    if (month_str == tokens.end()) {
      throw std::runtime_error("Failed to read month field.");
    }
    Month month = str_to_number(*month_str);

    auto day_str = iterator++;
    if (day_str == tokens.end()) {
      throw std::runtime_error("Failed to read day field.");
    }
    Day day = str_to_number(*day_str);

    auto value_str = iterator++;
    if (value_str == tokens.end()) {
      throw std::runtime_error("Failed to read value field.");
    }
    Temperature value = str_to_double(*value_str);

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

  std::cout << std::format("{}\n", config);

  const auto start = std::chrono::high_resolution_clock::now();

  auto stations = read_stations(config.stations_file());

  fill_measurements(stations, config.measurements_file());

  const auto elapsed = std::chrono::high_resolution_clock::now() - start;

  auto measurements = std::ranges::fold_left(
      stations | std::views::transform(
                     [](auto &station) { return station.measurements.size(); }),
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
    pool.spawn([&stations, stats_ptr, &detector] {
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

  pool.join();

  if constexpr (!PERF_TEST) {
    const auto elapsed =
        std::chrono::high_resolution_clock::now() - processing_start;
    std::cout << std::format(
        "Processed data in {} μs\n",
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
  }

  return 0;
}
