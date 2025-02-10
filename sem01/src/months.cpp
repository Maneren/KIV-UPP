#include "months.hpp"
#include "threadpool.hpp"
#include <limits>
#include <ranges>
#include <utility>

constexpr std::array<int, 12> DAYS_IN_MONTHS{31, 28, 31, 30, 31, 30,
                                             31, 31, 30, 31, 30, 31};

using StationMonthlyMinmaxes =
    std::array<std::pair<Temperature, Temperature>, 12>;

std::pair<StationMonthlyAverages, StationMonthlyMinmaxes>
calculate_monthly_stats(const Station &station) {
  std::array<MonthlyAverages, 12> monthly_averages;

  StationMonthlyMinmaxes monthly_minmaxes;
  monthly_minmaxes.fill({std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()});

  Month current_month{station.measurements.front().month};
  size_t days = 0;
  Temperature running_total{0};

  for (auto &measurement : station.measurements) {
    auto year = measurement.year;
    auto month = measurement.month;
    auto value = measurement.value;

    if (current_month != month) {
      auto average = running_total / days;

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
                                                  const size_t station_id,
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
        outliers.emplace_back(station_id, i + 1, next.first, diff);
      }
    }
  }

  return averages;
}

std::pair<std::vector<StationMonthlyAverages>, Outliers>
SerialOutlierDetector::average_and_find_outliers(
    const Stations &stations) const {
  std::vector<StationMonthlyAverages> stations_averages;
  Outliers outliers;

  for (const auto &[id, station] : stations | std::views::enumerate) {
    stations_averages.push_back(
        calculate_monthly_averages(station, id, outliers));
  }

  return {stations_averages, outliers};
}

std::pair<std::vector<StationMonthlyAverages>, Outliers>
ParallelOutlierDetector::average_and_find_outliers(
    const Stations &stations) const {
  auto results = pool.transform(
      stations | std::ranges::views::enumerate, [&](const auto &item) {
        auto [id, station] = item;
        auto [averages, min_maxes] = calculate_monthly_stats(station);
        Outliers outliers;

        auto monthly_averages =
            calculate_monthly_averages(station, id, outliers);

        return std::make_pair(monthly_averages, outliers);
      });

  std::vector<StationMonthlyAverages> stations_averages;
  Outliers outliers;

  for (auto &result : results) {
    const auto [averages, station_outliers] = result.get();

    stations_averages.push_back(averages);

    outliers.insert(outliers.end(), station_outliers.begin(),
                    station_outliers.end());
  }

  return {stations_averages, outliers};
}
