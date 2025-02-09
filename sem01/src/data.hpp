#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

struct Measurement {
  std::size_t ordinal;
  short year;
  std::byte month;
  std::byte day;
  float value;
};

class Station {
public:
  std::string name;
  std::pair<double, double> location;
  std::vector<Measurement> measurements;
};
