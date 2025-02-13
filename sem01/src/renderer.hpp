#pragma once

#include "colors.hpp"
#include "data.hpp"
#include <algorithm>
#include <cmath>

constexpr float MAP_WIDTH = 1412.f;
constexpr float MAP_HEIGHT = 809.f;

class Renderer {
public:
  Renderer();
  virtual ~Renderer() = default;

  std::string render_station(const Station &station,
                             const Temperature temperature) const;

  void render_month_to_file(const Stations &stations,
                            const std::vector<StationMonthlyAverages> &averages,
                            const size_t month,
                            const std::string &file_name) const;

  virtual void
  render_months(const Stations &stations,
                const std::vector<StationMonthlyAverages> &averages) = 0;

protected:
  std::ranges::minmax_result<Temperature> mMinmax;
  static std::string HEADER;

  static constexpr std::string_view TEMPLATE =
      "<circle cy=\"{}\" cx=\"{}\" r=\"12\" fill=\"rgb({},{},{})\" />\n";
  static constexpr std::string_view FOOTER = "</svg>";

  static constexpr std::pair<double, double> UPPER_LEFT_CORNER{
      51.03806105663445, 12.102209054269062};
  static constexpr std::pair<double, double> LOWER_RIGHT_CORNER{
      48.521003814763994, 18.866923511078615};
};

class SerialRenderer final : public Renderer {
public:
  void
  render_months(const Stations &stations,
                const std::vector<StationMonthlyAverages> &averages) override;
};

class ParallelRenderer final : public Renderer {
public:
  void
  render_months(const Stations &stations,
                const std::vector<StationMonthlyAverages> &averages) override;
};
