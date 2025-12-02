#include "parsing.hpp"
#include "threadpool.hpp"
#include <fstream>
#include <iostream>
#include <mutex>
#include <ranges>

using std::operator""sv;

/**
 * @brief Converts a string to a number.
 *
 * Simpler and faster version of stoul optimized for raw speed with no error
 * or bounds checking.
 */
size_t str_to_number(std::string_view &str) {
  size_t result = 0;
  const auto *ptr = str.data();
  const auto *end = str.end();
  // disable loop unrolling, since the numbers are usually only a few digits
  // long
#pragma clang loop unroll(disable)
  while (ptr < end) [[likely]] {
    result *= 10;
    result += *ptr++ - '0';

    if (*ptr == ';') [[unlikely]] {
      str = {ptr + 1, end};
      return result;
    }
  }
  return result;
}
// for stations
size_t str_to_number(auto str) {
  size_t result = 0;
  // disable loop unrolling, since the numbers are usually only a few digits
  // long
#pragma clang loop unroll(disable)
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
  const char *ptr = str.data();
  const char *end = str.end();

  bool has_signum = str[0] == '-';
  ptr += has_signum;

  int_fast64_t acc = 0;

#pragma clang loop unroll(disable)
  while (ptr < end && *ptr != '.') {
    acc *= 10;
    acc += *ptr - '0';
    ptr++;
  }

  double whole = has_signum ? -acc : acc;

  if (ptr == end) [[unlikely]] {
    return whole;
  }

  uint_fast32_t decimals = 0;
  uint_fast32_t power = 10;

#pragma clang loop unroll(disable)
  while (++ptr < end) {
    decimals *= 10;
    decimals += *ptr++ - '0';
    power *= 10;
  }

  double fractional = static_cast<double>(decimals) / power;
  double result = whole + (has_signum ? -fractional : fractional);

  return result;
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
       std::views::split(file_string, "\r\n"sv) | std::views::drop(1)) {
    if (file_line.empty()) {
      continue;
    }

    auto tokens = std::views::split(file_line, ';');
    auto iterator = tokens.begin();

    auto id_str = iterator++;
    if (id_str == tokens.end()) {
      throw std::runtime_error("Failed to read id field.");
    }
    size_t id = str_to_number(std::string_view(*id_str));

    auto name_str = iterator++;
    if (name_str == tokens.end()) {
      throw std::runtime_error("Failed to read name field.");
    }
    std::string name = *name_str | std::ranges::to<std::string>();

    auto longitude_str = iterator++;
    if (longitude_str == tokens.end()) {
      throw std::runtime_error("Failed to read longitude field.");
    }
    float longitude = str_to_double(std::string_view(*longitude_str));

    auto latitude_str = iterator++;
    if (latitude_str == tokens.end()) {
      throw std::runtime_error("Failed to read latitude field.");
    }
    float latitude = str_to_double(std::string_view(*latitude_str));

    stations.emplace_back(id, name, std::make_pair(latitude, longitude));
  }

  return stations;
}

void parse_line(std::string_view line, const auto &callback) {
  size_t id = str_to_number(line);
  size_t ordinal = str_to_number(line);
  Year year = str_to_number(line);
  Month month = str_to_number(line);
  Day day = str_to_number(line);
  Temperature value = str_to_double(line);

  callback(id, ordinal, year, month, day, value);
}

void process_measurements_serial(Stations &stations, std::string_view content) {
  for (const auto line : std::views::split(content, "\r\n"sv)) {
    if (!line.empty()) {
      const auto callback = [&](const size_t id, const size_t ordinal,
                                const Year year, const Month month,
                                const Day day, const Temperature value) {
        stations[id - 1].measurements.emplace_back(ordinal, year, month, day,
                                                   value);
      };
      parse_line(std::string_view(line), callback);
    }
  }
}

void process_measurements_parallel(Stations &stations,
                                   std::string_view content) {
  // split the range into 2 MiB parts
  constexpr size_t N = 1024 * 1024 * 2;

  std::vector<std::mutex> mutexes(stations.size());

  // parse each chunk in a separate thread
  const auto process_chunk = [&content, &stations, &mutexes](const size_t i) {
    auto chunk = content.substr(i);

    if (i > 0) {
      size_t newline_index = chunk.find("\r\n"sv);
      chunk = chunk.substr(newline_index + 2);
    }

    if (chunk.size() >= N) {
      size_t newline_index = chunk.substr(N).find("\r\n"sv);
      if (newline_index != std::string::npos) {
        chunk = chunk.substr(0, N + newline_index);
      }
    }

    size_t last_id = std::numeric_limits<size_t>::max();

    const auto callback = [&stations, &mutexes,
                           &last_id](const size_t id, const size_t ordinal,
                                     const Year year, const Month month,
                                     const Day day, const Temperature value) {
      // hold the lock for the current station until we hit another
      // one This exploits the fact that the data is sorted by
      // station id
      if (last_id != id) {
        if (last_id != std::numeric_limits<size_t>::max()) {
          mutexes[last_id - 1].unlock();
        }
        mutexes[id - 1].lock();
        last_id = id;
      }

      auto &measurements = stations[id - 1].measurements;
      measurements.emplace_back(ordinal, year, month, day, value);
    };

    do {
      decltype(chunk) next_chunk;
      size_t newline_index = chunk.find("\r\n"sv);
      if (newline_index != std::string::npos) {
        next_chunk = chunk.substr(newline_index + 2);
        chunk = chunk.substr(0, newline_index);
      }
      parse_line(chunk, callback);
      chunk = next_chunk;
    } while (!chunk.empty());

    if (last_id != std::numeric_limits<size_t>::max()) {
      mutexes[last_id - 1].unlock();
    }
  };

  auto offsets = std::views::iota(0uz, content.size()) | std::views::stride(N);
  threadpool::pool.for_each(std::move(offsets), process_chunk);
}

void fill_measurements(Stations &stations,
                       const std::filesystem::path &input_filepath,
                       bool parallel) {

  auto size = std::filesystem::file_size(input_filepath);
  std::string file_string(size, '\0');

  std::ifstream file(input_filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file.");
  }

  file.read(file_string.data(), size);

  if (file.fail()) {
    throw std::runtime_error("Failed to read file.");
  }

  // skip header
  size_t newline_index = file_string.find('\n');

  std::string_view file_string_view =
      std::string_view(file_string).substr(newline_index + 1);

  if (parallel) {
    process_measurements_parallel(stations, file_string_view);
  } else {
    process_measurements_serial(stations, file_string_view);
  }
}
