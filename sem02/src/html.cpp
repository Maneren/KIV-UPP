#include <ostream>
#include <regex>

#include "html.h"
#include "utils.h"

namespace html {

std::ostream &operator<<(std::ostream &os, const Stats &stats) {
  os << stats.path << std::endl;
  os << "IMAGES " << stats.images << std::endl;
  os << "LINKS " << stats.links.size() << std::endl;
  os << "FORMS " << stats.forms << std::endl;
  for (const auto &heading : stats.headings) {
    os << std::string(heading.first, '-') << " " << heading.second << std::endl;
  }

  return os;
}

Stats parse(const utils::URL &url) {
  // Regular expressions to match HTML elements
  const static std::regex img_regex(R"(<img\b)");
  const static std::regex form_regex(R"(<form\b)");
  const static std::regex link_regex(R"(<a\b[^>]+href="([^"]+)\")");
  const static std::regex heading_regex(R"(<h([1-6])>(.*?)</h\1>)");

  const auto html = utils::downloadHTML(url.toString());

  // Count the number of <img> tags
  const size_t images =
      std::distance(std::sregex_iterator(html.begin(), html.end(), img_regex),
                    std::sregex_iterator());

  // Count the number of <form> tags
  const size_t forms =
      std::distance(std::sregex_iterator(html.begin(), html.end(), form_regex),
                    std::sregex_iterator());

  // Extract and filter links from <a> tags
  std::vector<utils::URL> links;
  for (std::sregex_iterator it(html.begin(), html.end(), link_regex), end_it;
       it != end_it; ++it) {
    std::smatch match = *it;
    const auto link_url = utils::parseURL(match[1]);

    if (link_url.path.empty()) {
      continue;
    }

    // Skip links with different schemes or domains (missing are considered as
    // same)
    if ((!link_url.scheme.empty() && link_url.scheme != url.scheme) ||
        (!link_url.domain.empty() && link_url.domain != url.domain)) {
      continue;
    }

    links.push_back(link_url);
  }

  // Extract headings and their levels
  std::vector<std::pair<unsigned char, std::string>> headings;
  for (std::sregex_iterator it(html.begin(), html.end(), heading_regex), end_it;
       it != end_it; ++it) {
    std::smatch match = *it;
    headings.emplace_back(std::stol(match[1]), match[2]);
  }

  return {url.path.string(), images, forms, links, headings};
}

} // namespace html
