#include "databaze.h"
#include <numeric>

Database::Database() {
  mValues.resize(Velikost_Databaze);
  std::iota(mValues.begin(), mValues.end(), 0);
}

int Database::read(int index) const { return mValues[index]; }

void Database::write(int index, int value) { mValues[index] = value; }
