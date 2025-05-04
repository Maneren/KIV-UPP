#pragma once
#include "html.h"
#include <vector>

namespace serialization {

// Convert HtmlStats to a message that can be sent over MPI
std::vector<char> serializeHtmlStats(const html::Stats &stats);

html::Stats deserializeHtmlStats(const std::vector<char> &buffer);

} // namespace serialization
