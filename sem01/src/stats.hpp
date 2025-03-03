#include "data.hpp"

class Stats {
public:
  virtual ~Stats() = default;

  virtual std::vector<StationMonthlyStats>
  monthly_stats(const Stations &stations) const = 0;
};

class SerialStats : public Stats {
public:
  std::vector<StationMonthlyStats>
  monthly_stats(const Stations &stations) const override;
};

class ParallelStats : public Stats {
public:
  std::vector<StationMonthlyStats>
  monthly_stats(const Stations &stations) const override;
};
