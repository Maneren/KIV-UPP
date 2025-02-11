#pragma once

#include "colors.hpp"
#include "data.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <mutex>
#include <ranges>

constexpr float MAP_WIDTH = 1412.f;
constexpr float MAP_HEIGHT = 809.f;

class Renderer {
public:
  Renderer();
  virtual ~Renderer() = default;

  std::string render_station(const Station &station,
                             const Temperature temperature) const {
    const auto [upper_left_lat, upper_left_lon] = UPPER_LEFT_CORNER;
    const auto [lower_right_lat, lower_right_lon] = LOWER_RIGHT_CORNER;

    const auto y = map_range(upper_left_lat, lower_right_lat, 0, MAP_HEIGHT,
                             station.location.first);
    const auto x = map_range(upper_left_lon, lower_right_lon, 0, MAP_WIDTH,
                             station.location.second);

    const auto value = map_range(mMinmax.min, mMinmax.max, -1, 1, temperature);

    const auto color = lerpColor3(Color{0, 0, 255}, Color{255, 255, 0},
                                  Color{255, 0, 0}, value);

    return std::format(TEMPLATE, y, x, color.r, color.g, color.b);
  };

  virtual std::string
  render_month(const Stations &stations,
               const std::vector<StationMonthlyAverages> &averages,
               const size_t month) const = 0;

  virtual std::array<std::string, 12>
  render_months(const Stations &stations,
                const std::vector<StationMonthlyAverages> &averages) = 0;

protected:
  std::ranges::minmax_result<Temperature> mMinmax;
  static std::string HEADER;

  static constexpr std::string_view TEMPLATE =
      "<circle cy=\"{}\" cx=\"{}\" r=\"20\" fill=\"rgb({},{},{})\" />\n";
  static constexpr std::string_view FOOTER = "</svg>";

  static constexpr std::pair<double, double> UPPER_LEFT_CORNER{
      51.03806105663445, 12.102209054269062};
  static constexpr std::pair<double, double> LOWER_RIGHT_CORNER{
      48.521003814763994, 18.866923511078615};
};

class SerialRenderer final : public Renderer {
public:
  std::string render_month(const Stations &stations,
                           const std::vector<StationMonthlyAverages> &averages,
                           const size_t month) const override;

  std::array<std::string, 12>
  render_months(const Stations &stations,
                const std::vector<StationMonthlyAverages> &averages) override;
};

class ParallelRenderer final : public Renderer {
public:
  std::string render_month(const Stations &stations,
                           const std::vector<StationMonthlyAverages> &averages,
                           const size_t month) const override;

  std::array<std::string, 12>
  render_months(const Stations &stations,
                const std::vector<StationMonthlyAverages> &averages) override;

private:
  std::mutex mMutex;
  std::vector<std::string> mSvgBuffer;
};
