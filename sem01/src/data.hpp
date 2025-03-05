#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using Year = short;
using Month = short;
using Day = short;

using Temperature = double;

struct Measurement {
  std::size_t ordinal;
  Year year;
  Month month;
  Day day;
  Temperature value;
};

struct Station {
  size_t id;
  std::string name;
  std::pair<double, double> location;
  std::vector<Measurement> measurements;
};

using Stations = std::vector<Station>;

struct Outlier {
  size_t station_id;
  Month month;
  Year year;
  Temperature difference;
};

using Outliers = std::vector<Outlier>;

using MonthlyAverage = std::pair<Year, Temperature>;
using MonthlyAverages = std::vector<MonthlyAverage>;
using StationMonthlyAverages = std::array<MonthlyAverages, 12>;
using StationMonthlyMinmaxes =
    std::array<std::ranges::minmax_result<Temperature>, 12>;
using StationMonthlyStats =
    std::pair<StationMonthlyAverages, StationMonthlyMinmaxes>;
