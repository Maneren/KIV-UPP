#pragma once

#include <filesystem>
#include <format>
#include <thread>

enum ProcessingMode { Serial, Parallel };

template <> struct std::formatter<ProcessingMode> {
  constexpr auto parse(std::format_parse_context const &ctx) const {
    return ctx.begin();
  }
  template <typename FormatContext>
  auto format(const ProcessingMode &p, FormatContext &ctx) const {
    if (p == ProcessingMode::Serial) {
      return std::format_to(ctx.out(), "serial");
    } else {
      return std::format_to(ctx.out(), "parallel ({} threads)",
                            std::thread::hardware_concurrency());
    }
  }
};

class Config {
public:
  Config() = default;
  Config(const int argc, const char *const argv[]);

  ProcessingMode mode() const { return mMode; }
  const std::filesystem::path &stations_file() const { return mStationsFile; }
  const std::filesystem::path &measurements_file() const {
    return mMeasurementsFile;
  }

private:
  ProcessingMode mMode;
  std::filesystem::path mStationsFile;
  std::filesystem::path mMeasurementsFile;

  friend std::formatter<Config>;
};

template <> struct std::formatter<Config> {
  constexpr auto parse(std::format_parse_context const &ctx) const {
    return ctx.begin();
  }
  template <typename FormatContext>
  auto format(const Config &config, FormatContext &ctx) const {
    return std::format_to(
        ctx.out(), "Config:\n\tmode: {}\n\tstations: {}\n\tmeasurements: {}\n",
        config.mMode, config.mStationsFile.string(),
        config.mMeasurementsFile.string());
  }
};
