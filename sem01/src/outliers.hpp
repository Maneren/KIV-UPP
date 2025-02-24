#pragma once

#include "data.hpp"
#include <utility>

using StationStats = std::pair<StationMonthlyAverages, Outliers>;

class OutlierDetector {
public:
  virtual ~OutlierDetector() = default;
  virtual std::pair<std::vector<StationMonthlyAverages>, size_t>

  find_averages_and_outliers(const Stations &stations,
                             std::ostream &outliers) const = 0;
};

class SerialOutlierDetector : public OutlierDetector {
public:
  std::pair<std::vector<StationMonthlyAverages>, size_t>
  find_averages_and_outliers(const Stations &stations,
                             std::ostream &outliers) const override;
};

class ParallelOutlierDetector : public OutlierDetector {
public:
  std::pair<std::vector<StationMonthlyAverages>, size_t>
  find_averages_and_outliers(const Stations &stations,
                             std::ostream &outliers) const override;
};
