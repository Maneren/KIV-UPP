#include "config.hpp"
#include "data.hpp"
#include "months.hpp"
#include "preprocessor.hpp"
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

std::vector<Station>
read_stations(const std::filesystem::path &input_filepath) {
  std::vector<Station> stations;

  std::ifstream input_file(input_filepath);
  if (!input_file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  std::string field_buffer;
  auto last_id = -1;

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }

    if (last_id == -1) {
      last_id++;
      continue;
    }

    auto line_stream = std::istringstream{line};

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read id field.");
    }
    std::size_t id = std::stoi(field_buffer);

    if (id - last_id > 1)
      throw std::runtime_error("Skipped station index.");
    else
      last_id = id;

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read name field.");
    }
    std::string name = std::move(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read latitude field.");
    }
    float latitude = std::stof(field_buffer);

    if (!std::getline(line_stream, field_buffer, ';')) {
      throw std::runtime_error("Failed to read longitude field.");
    }
    float longitude = std::stof(field_buffer);

    stations.emplace_back(name, std::make_pair(latitude, longitude));
  }

  return stations;
}

void fill_measurements(std::vector<Station> &stations,
                       const std::filesystem::path &input_filepath) {
  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  std::string line;
  std::string field_buffer;
  auto last_ord = -1;
  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }

    if (last_ord == -1) {
      last_ord++;
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

    if (ordinal - stations[id - 1].measurements.size() > 1)
      throw std::runtime_error("Skipped measurement.");
    else
      last_ord = ordinal;

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

  auto stations = read_stations(config.stations_file());

  fill_measurements(stations, config.measurements_file());

  Preprocessor *preprocessor;
  if (config.mode() == ProcessingMode::Serial) {
    preprocessor = new SerialPreprocessor();
  } else {
    preprocessor = new ParallelPreprocessor();
  }

  std::cout << "Stations: " << stations.size() << std::endl;

  auto start = std::chrono::high_resolution_clock::now();

  stations = preprocessor->preprocess_data(stations);

  auto elapsed = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Preprocessed stations: " << stations.size() << std::endl;
  std::cout
      << "Elapsed time: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
      << "ms" << std::endl;

  OutlierDetector *detector;
  if (config.mode() == ProcessingMode::Serial) {
    detector = new SerialOutlierDetector();
  } else {
    detector = new ParallelOutlierDetector();
  }

  start = std::chrono::high_resolution_clock::now();

  auto [averages, outliers] = detector->average_and_find_outliers(stations);

  elapsed = std::chrono::high_resolution_clock::now() - start;
  std::cout << "Outliers: " << outliers.size() << std::endl;
  std::cout
      << "Elapsed time: "
      << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
      << "ms" << std::endl;

  {
    std::ofstream outlier_file("vykyvy.csv");
    outlier_file << "id;mesic;rok;rozdil" << std::endl;
    for (const auto &outlier : outliers) {
      outlier_file << outlier.station_id << ";" << outlier.month << ";"
                   << outlier.year << ";" << outlier.difference << std::endl;
    }
  }

  return 0;
}
