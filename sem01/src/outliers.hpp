#pragma once

#include "data.hpp"

constexpr std::string_view OUTLIER_FILE_HEADER = "id;mesic;rok;rozdil";

class OutlierDetector {
public:
  virtual ~OutlierDetector() = default;

  virtual size_t find_outliers(const Stations &stations,
                               const std::vector<StationMonthlyStats> &stats,
                               std::ostream &outliers) const = 0;
};

class SerialOutlierDetector : public OutlierDetector {
public:
  size_t find_outliers(const Stations &stations,
                       const std::vector<StationMonthlyStats> &stats,
                       std::ostream &outliers) const override;
};

class ParallelOutlierDetector : public OutlierDetector {
public:
  size_t find_outliers(const Stations &stations,
                       const std::vector<StationMonthlyStats> &stats,
                       std::ostream &outliers) const override;
};
