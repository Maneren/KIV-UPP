#include "preprocessor.hpp"
#include "threadpool.hpp"
#include <algorithm>
#include <ranges>

bool valid_station(const Station &station) {
  const auto [first_year, last_year] = std::ranges::minmax(
      station.measurements | std::views::transform([](auto &measurement) {
        return measurement.year;
      }));

  const auto time_span = last_year - first_year;

  return time_span >= 5 && station.measurements.size() / time_span >= 300;
}

template <typename T>
inline void swap_remove(std::vector<T> &vector, const size_t index) {
  vector[index] = std::move(vector.back());
  vector.pop_back();
}

void SerialPreprocessor::preprocess_data(Stations &stations) const {
  for (auto [i, station] :
       stations | std::views::enumerate | std::views::reverse) {
    if (!valid_station(station)) {
      swap_remove(stations, i);
    }
  }
}

void ParallelPreprocessor::preprocess_data(Stations &stations) const {
  for (auto [i, future] : pool.transform(stations, valid_station) |
                              std::views::enumerate | std::views::reverse) {
    if (!future.get()) {
      swap_remove(stations, i);
    }
  }
}
