#include "data.hpp"
#include <filesystem>

std::string read_file(const std::filesystem::path &input_filepath);

Stations parse_stations(const std::string &file_string);

void fill_measurements(Stations &stations, const std::string &file_string,
                       bool parallel);
