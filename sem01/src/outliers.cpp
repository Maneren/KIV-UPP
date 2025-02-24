#include "outliers.hpp"
#include "threadpool.hpp"
#include <algorithm>
#include <limits>
#include <ranges>
#include <utility>

using StationMonthlyMinmaxes =
    std::array<std::ranges::minmax_result<Temperature>, 12>;

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
    const auto &month = measurement.month;

    if (month != current_month) {
      const auto average = running_total / days;

      auto &current_min_max = monthly_minmaxes[month - 1];
      current_min_max.min = std::min(current_min_max.min, average);
      current_min_max.max = std::max(current_min_max.max, average);

      monthly_averages[month - 1].push_back({measurement.year, average});

      current_month = month;
      days = 0;
      running_total = 0;
    }

    running_total += measurement.value;
    days++;
  }

  return {monthly_averages, monthly_minmaxes};
}

std::string format_outlier(const Outlier &outlier) {
  return std::format("{};{};{};{}\n", outlier.station_id, outlier.month,
                     outlier.year, outlier.difference);
}

StationMonthlyAverages calculate_monthly_averages(
    const Station &station,
    const std::function<void(const Outlier &)> &save_outlier) {
  const auto [averages, min_maxes] = calculate_monthly_stats(station);

  for (const auto [i, item] :
       std::views::zip(averages, min_maxes) | std::views::enumerate) {
    const auto [month, min_max] = item;

    const auto allowed_difference = 0.75f * (min_max.max - min_max.min);

    for (const auto [prev, next] : std::views::pairwise(month)) {
      if (next.first != prev.first + 1) {
        continue;
      }

      const auto diff = std::abs(next.second - prev.second);

      if (diff > allowed_difference) {
        save_outlier({station.id, static_cast<Month>(i + 1), next.first, diff});
      }
    }
  }

  return averages;
}

std::pair<std::vector<StationMonthlyAverages>, size_t>
SerialOutlierDetector::find_averages_and_outliers(
    const Stations &stations, std::ostream &outliers) const {

  size_t outliers_count = 0;
  const std::function<void(const Outlier &)> save_outlier =
      [&outliers, &outliers_count](const Outlier &outlier) {
        outliers_count++;
        outliers << format_outlier(outlier);
      };

  const auto stations_averages =
      stations | std::views::transform([save_outlier](const auto &station) {
        return calculate_monthly_averages(station, save_outlier);
      }) |
      std::ranges::to<std::vector>();

  return {stations_averages, outliers_count};
}

std::pair<std::vector<StationMonthlyAverages>, size_t>
ParallelOutlierDetector::find_averages_and_outliers(
    const Stations &stations, std::ostream &outliers) const {

  size_t outliers_count = 0;
  std::mutex mutex;
  const auto save_outlier = [&mutex, &outliers,
                             &outliers_count](const Outlier &outlier) mutable {
    const auto formatted = format_outlier(outlier);
    std::lock_guard lock{mutex};
    outliers_count++;
    outliers << formatted;
  };

  const auto process_station = [&save_outlier](const auto &station) {
    return calculate_monthly_averages(station, save_outlier);
  };

  const auto stations_averages =
      pool.transform(stations, process_station) |
      std::views::transform([](std::future<StationMonthlyAverages> &future) {
        return future.get();
      }) |
      std::ranges::to<std::vector>();

  return {stations_averages, outliers_count};
}
