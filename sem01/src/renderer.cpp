#include "renderer.hpp"
#include "threadpool.hpp"
#include <numeric>

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

std::string SerialRenderer::render_month(
    const Stations &stations,
    const std::vector<StationMonthlyAverages> &averages,
    const size_t month) const {
  std::string svg = std::string{HEADER};

  for (const auto [station, averages] : std::views::zip(stations, averages)) {
    const auto month_averages = averages[month] | std::views::values;

    const auto average =
        std::ranges::fold_left(month_averages, Temperature{0}, std::plus{}) /
        month_averages.size();

    svg += render_station(station, average);
  }

  svg += FOOTER;

  return svg;
}

std::array<std::string, 12> SerialRenderer::render_months(
    const Stations &stations,
    const std::vector<StationMonthlyAverages> &averages) {
  mMinmax = std::ranges::minmax(averages | std::views::join |
                                std::views::transform([](auto &item) {
                                  return item | std::views::values;
                                }) |
                                std::views::join);

  std::array<std::string, 12> svgs;

  for (auto i = 0; i < 12; i++) {
    svgs[i] = render_month(stations, averages, i);
  }

  return svgs;
}

std::string ParallelRenderer::render_month(
    const Stations &stations,
    const std::vector<StationMonthlyAverages> &averages,
    const size_t month) const {
  std::string svg = std::string{HEADER};

  for (const auto [station, averages] : std::views::zip(stations, averages)) {
    const auto month_averages = averages[month] | std::views::values;

    const auto average =
        std::ranges::fold_left(month_averages, Temperature{0}, std::plus{}) /
        month_averages.size();

    svg += render_station(station, average);
  }

  svg += FOOTER;

  return svg;
}

std::array<std::string, 12> ParallelRenderer::render_months(
    const Stations &stations,
    const std::vector<StationMonthlyAverages> &averages) {
  mMinmax = std::ranges::minmax(averages | std::views::join |
                                std::views::transform([](auto &item) {
                                  return item | std::views::values;
                                }) |
                                std::views::join);

  std::array<std::string, 12> svgs;

  pool.for_each(std::views::iota(0, 12), [&, this](const int &i) {
    svgs[i] = render_month(stations, averages, i);
  });

  return svgs;
}
