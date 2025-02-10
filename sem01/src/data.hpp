#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using Year = short;
using Month = short;
using Day = short;

using Temperature = float;

struct Measurement {
  std::size_t ordinal;
  Year year;
  Month month;
  Day day;
  Temperature value;
};

class Station {
public:
  std::string name;
  std::pair<double, double> location;
  std::vector<Measurement> measurements;
};

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
