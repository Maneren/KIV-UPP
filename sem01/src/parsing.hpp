#include "data.hpp"
#include <filesystem>

Stations read_stations(const std::filesystem::path &input_filepath);

void fill_measurements(Stations &stations,
                       const std::filesystem::path &input_filepath,
                       bool parallel);
