#include "outliers.hpp"
#include "threadpool.hpp"
#include <limits>
#include <ranges>
#include <utility>

using StationMonthlyMinmaxes =
    std::array<std::pair<Temperature, Temperature>, 12>;

std::pair<StationMonthlyAverages, StationMonthlyMinmaxes>
calculate_monthly_stats(const Station &station) {
  StationMonthlyAverages monthly_averages;

  StationMonthlyMinmaxes monthly_minmaxes;
  monthly_minmaxes.fill({std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()});

  Month current_month{station.measurements.front().month};
  size_t days = 0;
  Temperature running_total{0};

  for (const auto &measurement : station.measurements) {
    const auto year = measurement.year;
    const auto month = measurement.month;
    const auto value = measurement.value;

    if (month != current_month) {
      const auto average = running_total / days;

      auto &current_min_max = monthly_minmaxes[month - 1];
      current_min_max.first = std::min(current_min_max.first, average);
      current_min_max.second = std::max(current_min_max.second, average);

      monthly_averages[month - 1].push_back({year, average});

      current_month = month;
      days = 0;
      running_total = 0;
    }

    running_total += value;
    days++;
  }

  return {monthly_averages, monthly_minmaxes};
}

StationMonthlyAverages calculate_monthly_averages(const Station &station,
                                                  Outliers &outliers) {
  auto [averages, min_maxes] = calculate_monthly_stats(station);

  for (const auto [i, item] :
       std::views::zip(averages, min_maxes) | std::views::enumerate) {
    const auto [month, min_max] = item;

    const auto allowed_difference = 0.75f * (min_max.second - min_max.first);

    for (const auto [prev, next] : std::views::pairwise(month)) {
      if (next.first != prev.first + 1) {
        continue;
      }

      const auto diff = std::abs(next.second - prev.second);

      if (diff > allowed_difference) {
        outliers.emplace_back(station.id, i + 1, next.first, diff);
      }
    }
  }

  return averages;
}

std::pair<std::vector<StationMonthlyAverages>, Outliers>
SerialOutlierDetector::find_averages_and_outliers(
    const Stations &stations) const {
  Outliers outliers;
  const auto stations_averages =
      stations | std::views::transform([&outliers](const auto &station) {
        return calculate_monthly_averages(station, outliers);
      }) |
      std::ranges::to<std::vector>();

  return {stations_averages, outliers};
}

std::pair<std::vector<StationMonthlyAverages>, Outliers>
ParallelOutlierDetector::find_averages_and_outliers(
    const Stations &stations) const {
  auto futures = pool.transform(stations, [](const auto &station) {
    Outliers outliers;
    const auto monthly_averages = calculate_monthly_averages(station, outliers);
    return std::make_pair(monthly_averages, outliers);
  });

  std::vector<StationMonthlyAverages> stations_averages;
  stations_averages.reserve(stations.size());

  Outliers outliers;

  for (auto &future : futures) {
    const auto &[averages, station_outliers] = future.get();

    stations_averages.push_back(std::move(averages));

    outliers.insert(outliers.end(), station_outliers.begin(),
                    station_outliers.end());
  }

  return {stations_averages, outliers};
}
