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
  // auto svg_lines = pool.transform(
  //     std::views::zip(stations, averages), [this, month](const auto &item) {
  //       const auto [station, averages] = item;
  //       const auto month_averages = averages[month] | std::views::values;
  //
  //       const auto average = std::ranges::fold_left(
  //                                month_averages, Temperature{0}, std::plus{})
  //                                /
  //                            month_averages.size();
  //
  //       return render_station(station, average);
  //     });
  //
  // std::string svg = std::string{HEADER};
  //
  // for (auto &future : svg_lines) {
  //   svg += future.get();
  // }

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
  auto minmaxes = pool.transform(averages, [](const auto &station_averages) {
    return std::ranges::minmax(station_averages | std::views::join |
                               std::views::values);
  });

  for (auto &future : minmaxes) {
    auto [min, max] = future.get();
    mMinmax.min = std::min(mMinmax.min, min);
    mMinmax.max = std::max(mMinmax.max, max);
  }

  std::array<std::string, 12> svgs;
  std::array<std::future<void>, 12> futures;

  for (auto i = 0; i < 12; i++) {
    futures[i] = pool.spawn_with_future(
        [&, this, i] { svgs[i] = render_month(stations, averages, i); });
  }

  for (auto &future : futures) {
    future.get();
  }

  return svgs;
}
