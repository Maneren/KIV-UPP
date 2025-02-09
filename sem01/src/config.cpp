#include "config.hpp"
#include <format>
#include <string>

Config::Config(const int argc, const char *const argv[]) {
  const auto usage_message = [&argv]() {
    return std::format("Usage: {} --serial|parallel <input_file>",
                       std::string(argv[0]));
  };

  if (argc < 3) {
    throw std::invalid_argument(usage_message());
  }

  auto mode_arg = std::string(argv[1]);
  if (mode_arg == "--serial") {
    mMode = ProcessingMode::Serial;
  } else if (mode_arg == "--parallel") {
    mMode = ProcessingMode::Parallel;
  } else {
    throw std::invalid_argument(usage_message());
  }

  auto stations_file_arg = std::filesystem::path(argv[2]);

  if (!std::filesystem::exists(stations_file_arg)) {
    throw std::invalid_argument("Stations file does not exist");
  }

  mStationsFile = stations_file_arg;

  auto measurements_file_arg = std::filesystem::path(argv[3]);

  if (!std::filesystem::exists(measurements_file_arg)) {
    throw std::invalid_argument("Measurements file does not exist");
  }

  mMeasurementsFile = measurements_file_arg;
}
