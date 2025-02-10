#pragma once

#include "data.hpp"
#include <vector>

class Preprocessor {
public:
  virtual Stations
  preprocess_data(const Stations &) const = 0;
};

class SerialPreprocessor : public Preprocessor {
public:
  Stations
  preprocess_data(const Stations &) const override;
};

class ParallelPreprocessor : public Preprocessor {
public:
  Stations
  preprocess_data(const Stations &) const override;
};
