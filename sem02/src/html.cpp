#include <ostream>
#include <regex>

#include "html.h"
#include "utils.h"

namespace html {

std::ostream &operator<<(std::ostream &os, const Stats &stats) {
  os << "IMAGES " << stats.images << std::endl;
  os << "LINKS " << stats.links.size() << std::endl;
  os << "FORMS " << stats.forms << std::endl;
  for (const auto &heading : stats.headings) {
    os << std::string(heading.first, '-') << " " << heading.second << std::endl;
  }

  return os;
}

Stats parse(const utils::URL &url) {
  const static std::regex img_regex(R"(<img\b)");
  const static std::regex form_regex(R"(<form\b)");
  const static std::regex link_regex(R"(<a\b[^>]+href="([^"]+)\")");
  const static std::regex heading_regex(R"(<h([1-6])>(.*?)</h\1>)");

  const auto html = utils::downloadHTML(url.toString());

  const size_t images =
      std::distance(std::sregex_iterator(html.begin(), html.end(), img_regex),
                    std::sregex_iterator());

  const size_t forms =
      std::distance(std::sregex_iterator(html.begin(), html.end(), form_regex),
                    std::sregex_iterator());

  std::vector<utils::URL> links;
  for (std::sregex_iterator it(html.begin(), html.end(), link_regex), end_it;
       it != end_it; ++it) {
    std::smatch match = *it;
    links.push_back(utils::parseURL(match[1]));
  }

  std::vector<std::pair<size_t, std::string>> headings;
  for (std::sregex_iterator it(html.begin(), html.end(), heading_regex), end_it;
       it != end_it; ++it) {
    std::smatch match = *it;
    headings.emplace_back(std::stol(match[1]), match[2]);
  }

  return {url.path.string(), images, forms, links, headings};
}

} // namespace html
