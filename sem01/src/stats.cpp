#include "stats.hpp"
#include "threadpool.hpp"
#include <limits>
#include <ranges>

std::pair<StationMonthlyAverages, StationMonthlyMinmaxes>
calculate_monthly_stats(const Station &station) {
  StationMonthlyAverages monthly_averages;

  StationMonthlyMinmaxes monthly_minmaxes;
  monthly_minmaxes.fill({std::numeric_limits<Temperature>::infinity(),
                         -std::numeric_limits<Temperature>::infinity()});

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

std::vector<StationMonthlyStats>
SerialStats::monthly_stats(const Stations &stations) const {
  return stations | std::views::transform(calculate_monthly_stats) |
         std::ranges::to<std::vector>();
}

std::vector<StationMonthlyStats>
ParallelStats::monthly_stats(const Stations &stations) const {
  return threadpool::pool.transform(stations, calculate_monthly_stats) |
         std::views::transform([](std::future<StationMonthlyStats> &f) mutable {
           return f.get();
         }) |
         std::ranges::to<std::vector>();
}
