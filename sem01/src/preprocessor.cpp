#include "preprocessor.hpp"
#include "threadpool.hpp"
#include <algorithm>
#include <ranges>
#include <utility>

bool valid_station(const Station &station) {
  const auto [first_year, last_year] = std::ranges::minmax(
      station.measurements | std::views::transform([](auto &measurement) {
        return measurement.year;
      }));

  const auto time_span = last_year - first_year;

  return time_span >= 5 && station.measurements.size() / time_span >= 300;
}

std::vector<Station> SerialPreprocessor::preprocess_data(
    const std::vector<Station> &stations) const {
  return stations | std::views::filter(valid_station) |
         std::ranges::to<std::vector>();
}

std::vector<Station> ParallelPreprocessor::preprocess_data(
    const std::vector<Station> &stations) const {
  auto futures = pool.transform(stations, [](const Station &station) {
    return std::make_pair(valid_station(station), station);
  });

  std::vector<Station> new_stations{};
  for (auto &future : futures) {
    auto [valid, station] = future.get();

    if (valid) {
      new_stations.push_back(std::move(station));
    }
  }

  return new_stations;
}
