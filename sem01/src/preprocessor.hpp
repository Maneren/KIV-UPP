#pragma once

#include "data.hpp"

class Preprocessor {
public:
  virtual ~Preprocessor() = default;
  virtual void preprocess_data(Stations &) const = 0;
};

class SerialPreprocessor : public Preprocessor {
public:
  void preprocess_data(Stations &) const override;
};

class ParallelPreprocessor : public Preprocessor {
public:
  void preprocess_data(Stations &) const override;
};
