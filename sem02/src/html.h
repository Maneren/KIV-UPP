#pragma once

#include "utils.h"
#include <string>
#include <vector>

namespace html {

struct Stats {
  std::filesystem::path path;
  size_t images;
  size_t forms;
  std::vector<utils::URL> links;
  std::vector<std::pair<size_t, std::string>> headings;
};

std::ostream &operator<<(std::ostream &os, const Stats &stats);

Stats parse(const utils::URL &url);

} // namespace html
