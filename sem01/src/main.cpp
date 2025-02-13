#include "config.hpp"
#include "data.hpp"
#include "months.hpp"
#include "preprocessor.hpp"
#include "renderer.hpp"
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

Stations read_stations(const std::filesystem::path &input_filepath) {
  Stations stations;

  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  std::string line, field_buffer;

  // Skip the header line
  std::getline(file, line);

  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }

    auto line_stream = std::istringstream{line};

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read id field.");
    }
    size_t id = std::stoi(field_buffer);

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

    stations.emplace_back(name, std::make_pair(latitude, longitude));
  }

  return stations;
}

void fill_measurements(Stations &stations,
                       const std::filesystem::path &input_filepath) {
  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  std::string line;
  std::string field_buffer;

  // Skip the header line
  std::getline(file, line);

  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }

    auto line_stream = std::istringstream{line};

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read id field.");
    }
    std::size_t id = std::stoi(field_buffer);

    if (id - 1 > stations.size())
      throw std::runtime_error("Invalid station ID.");

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read ordinal field.");
    }
    std::size_t ordinal = std::stoi(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read year field.");
    }
    Year year = std::stoi(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read month field.");
    }
    Month month = std::stoi(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read day field.");
    }
    Day day = std::stoi(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read value field.");
    }
    Temperature value = std::stof(field_buffer);

    stations[id - 1].measurements.emplace_back(ordinal, year, month, day,
                                               value);
  }
}

int main(int argc, char *argv[]) {
  Config config;
  try {
    config = Config(argc, argv);
  } catch (std::invalid_argument &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  std::cout << std::format("{}", config);

  auto start = std::chrono::high_resolution_clock::now();

  auto stations = read_stations(config.stations_file());

  fill_measurements(stations, config.measurements_file());

  auto elapsed = std::chrono::high_resolution_clock::now() - start;
  std::cout
      << "Elapsed time: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
      << "ms" << std::endl;

  std::unique_ptr<Preprocessor> preprocessor;
  if (config.mode() == ProcessingMode::Serial) {
    preprocessor = std::make_unique<SerialPreprocessor>();
  } else {
    preprocessor = std::make_unique<ParallelPreprocessor>();
  }

  std::cout << "Stations: " << stations.size() << std::endl;

  start = std::chrono::high_resolution_clock::now();

  preprocessor->preprocess_data(stations);
  stations.shrink_to_fit();

  elapsed = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Preprocessed stations: " << stations.size() << std::endl;
  std::cout
      << "Elapsed time: "
      << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
      << "μs" << std::endl;

  std::unique_ptr<OutlierDetector> detector;
  if (config.mode() == ProcessingMode::Serial) {
    detector = std::make_unique<SerialOutlierDetector>();
  } else {
    detector = std::make_unique<ParallelOutlierDetector>();
  }

  start = std::chrono::high_resolution_clock::now();

  auto [averages, outliers] = detector->average_and_find_outliers(stations);

  elapsed = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Outliers: " << outliers.size() << std::endl;
  std::cout
      << "Elapsed time: "
      << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
      << "μs" << std::endl;

  {
    std::ofstream outlier_file("output/vykyvy.csv");
    outlier_file << "id;mesic;rok;rozdil" << std::endl;
    for (const auto &outlier : outliers) {
      outlier_file << outlier.station_id << ";" << outlier.month << ";"
                   << outlier.year << ";" << outlier.difference << std::endl;
    }
  }

  std::unique_ptr<Renderer> renderer;
  if (config.mode() == ProcessingMode::Serial) {
    renderer = std::make_unique<SerialRenderer>();
  } else {
    renderer = std::make_unique<ParallelRenderer>();
  }

  start = std::chrono::high_resolution_clock::now();

  renderer->render_months(stations, averages);

  elapsed = std::chrono::high_resolution_clock::now() - start;
  std::cout
      << "Elapsed time: "
      << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count()
      << "μs" << std::endl;

  return 0;
}
