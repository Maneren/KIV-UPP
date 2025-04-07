#pragma once

#include <vector>

constexpr size_t Velikost_Databaze = 100'000;

inline constexpr size_t Spravny_Soucet() {
  size_t sum = 0;
  for (size_t i = 0; i < Velikost_Databaze; i++) {
    sum += i;
  }
  return sum;
}

class Database {
private:
  std::vector<int> mValues;

public:
  Database();

  int read(int index) const;

  void write(int index, int value);
};
