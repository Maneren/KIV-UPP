#include "renderer.hpp"
#include "threadpool.hpp"
#include "utils.hpp"
#include <algorithm>
#include <format>
#include <fstream>

Renderer::Renderer() {
  std::ifstream file("czmap.svg");

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open SVG template file.");
  }

  // read the file to HEADER
  HEADER = std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

std::string Renderer::HEADER;

std::string Renderer::render_station(const Station &station,
                                     const Temperature temperature) const {
  const auto [upper_left_lat, upper_left_lon] = UPPER_LEFT_CORNER;
  const auto [lower_right_lat, lower_right_lon] = LOWER_RIGHT_CORNER;

  const auto y = map_range(upper_left_lat, lower_right_lat, 0, MAP_HEIGHT,
                           station.location.first);
  const auto x = map_range(upper_left_lon, lower_right_lon, 0, MAP_WIDTH,
                           station.location.second);

  const auto value = map_range(mMinmax.min, mMinmax.max, -1, 1, temperature);

  constexpr Color BLUE{0, 0, 255, 255};
  constexpr Color YELLOW{255, 255, 0, 255};
  constexpr Color RED{255, 0, 0, 255};

  const auto color = lerpColor3(BLUE, YELLOW, RED, value);

  return std::format(TEMPLATE, y, x, color.r, color.g, color.b);
};

void Renderer::render_month_to_file(
    const Stations &stations,
    const std::vector<StationMonthlyAverages> &averages, const size_t month,
    const std::string &file_name) const {
  std::ofstream svg_file(file_name);
  svg_file << HEADER;

  for (const auto [station, averages] : std::views::zip(stations, averages)) {
    const auto month_averages = averages[month] | std::views::values;

    const auto average =
        std::ranges::fold_left(month_averages, Temperature{0}, std::plus{}) /
        month_averages.size();

    svg_file << render_station(station, average);
  }

  svg_file << FOOTER;
}

constexpr std::array<std::string_view, 12> MONTHS = {
    "leden",    "unor",  "brezen", "duben", "kveten",   "cerven",
    "cervenec", "srpen", "zari",   "rijen", "listopad", "prosinec"};

void SerialRenderer::render_months(
    const Stations &stations,
    const std::vector<StationMonthlyAverages> &averages) {
  mMinmax = std::ranges::minmax(averages | std::views::join |
                                std::views::transform([](auto &item) {
                                  return item | std::views::values;
                                }) |
                                std::views::join);

  for (const auto [i, month] : MONTHS | std::views::enumerate) {
    render_month_to_file(stations, averages, i,
                         std::format("output/{}.svg", month));
  }
}

void ParallelRenderer::render_months(
    const Stations &stations,
    const std::vector<StationMonthlyAverages> &averages) {
  mMinmax = std::ranges::minmax(averages | std::views::join |
                                std::views::transform([](auto &item) {
                                  return item | std::views::values;
                                }) |
                                std::views::join);

  pool.for_each(MONTHS | std::views::enumerate, [&, this](const auto &item) {
    const auto [i, month] = item;
    render_month_to_file(stations, averages, i,
                         std::format("output/{}.svg", month));
  });
}
