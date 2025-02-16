#pragma once

#include "config.hpp"

inline float map_range(const float in_min, const float in_max,
                       const float out_min, const float out_max,
                       float in_value) {
  return (in_value - in_min) * (out_max - out_min) / (in_max - in_min) +
         out_min;
}

template <typename T, typename Serial, typename Parallel>
constexpr inline std::unique_ptr<T> choose_by_mode(ProcessingMode mode) {
  if (mode == ProcessingMode::Serial) {
    return std::make_unique<Serial>();
  } else {
    return std::make_unique<Parallel>();
  }
}
