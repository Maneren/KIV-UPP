#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using Year = short;
using Month = unsigned char;
using Day = unsigned char;

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
