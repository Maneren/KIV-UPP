#pragma once

#include "data.hpp"
#include <vector>

class Preprocessor {
public:
  virtual std::vector<Station>
  preprocess_data(const std::vector<Station> &) const = 0;
};

class SerialPreprocessor : public Preprocessor {
public:
  std::vector<Station>
  preprocess_data(const std::vector<Station> &) const override;
};

class ParallelPreprocessor : public Preprocessor {
public:
  std::vector<Station>
  preprocess_data(const std::vector<Station> &) const override;
};
