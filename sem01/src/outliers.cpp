#include "outliers.hpp"
#include "threadpool.hpp"
#include <ostream>
#include <ranges>

std::string format_outlier(const Outlier &outlier) {
  return std::format("{};{};{};{}\n", outlier.station_id, outlier.month,
                     outlier.year, outlier.difference);
}

void output_outliers(const Station &station, const StationMonthlyStats &stats,
                     const std::function<void(const Outlier &)> &save_outlier) {
  for (const auto [i, item] :
       std::views::zip(stats.first, stats.second) | std::views::enumerate) {
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
}

size_t SerialOutlierDetector::find_outliers(
    const Stations &stations, const std::vector<StationMonthlyStats> &stats,
    std::ostream &outliers) const {

  size_t outliers_count = 0;
  const auto save_outlier = [&outliers, &outliers_count](const auto &outlier) {
    outliers_count++;
    outliers << format_outlier(outlier);
  };

  for (const auto &[station, station_stats] :
       std::views::zip(stations, stats)) {
    output_outliers(station, station_stats, save_outlier);
  }

  return outliers_count;
}

size_t ParallelOutlierDetector::find_outliers(
    const Stations &stations, const std::vector<StationMonthlyStats> &stats,
    std::ostream &outliers) const {

  size_t outliers_count = 0;
  std::mutex mutex;
  const auto save_outlier = [&mutex, &outliers,
                             &outliers_count](const auto &outlier) mutable {
    const auto formatted = format_outlier(outlier);
    std::lock_guard lock{mutex};
    outliers_count++;
    outliers << formatted;
  };

  const auto process_station = [&save_outlier](const auto &item) {
    const auto [station, station_stats] = item;
    output_outliers(station, station_stats, save_outlier);
  };

  pool.for_each(std::views::zip(stations, stats), process_station);

  return outliers_count;
}
