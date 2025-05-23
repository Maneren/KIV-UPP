/**
 * Kostra druhe semestralni prace z predmetu KIV/UPP
 * Soubory a hlavicku upravujte dle sveho uvazeni a nutnosti
 */

#include <algorithm>
#ifdef USE_SSL
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "../dep/cpp-httplib/httplib.h"

#include "utils.h"
#include <fstream>

namespace utils {

std::string readWholeFile(const std::string &path) {
  // otevreni souboru
  std::ifstream ifs(path);
  if (!ifs) {
    return "";
  }

  // nacteni obsahu souboru
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));
  return content;
}

std::string downloadHTML(const std::string &url) {
  std::string scheme;
  std::string rest;

  // extrahovani schematu a zbytku URL
  if (url.substr(0, 7) == "http://") {
    scheme = "http";
    rest = url.substr(7);
  } else if (url.substr(0, 8) == "https://") {
    scheme = "https";
    rest = url.substr(8);
  } else {
    return ""; // nezname schema
  }

  const size_t pos = rest.find("/");
  const std::string domain = rest.substr(0, pos);
  const std::string path = rest.substr(pos);

  // stahne obsah stranky - pouzije SSL klienta, pokud je pozadovana podpora SSL
#ifdef USE_SSL
  httplib::SSLClient cli(domain.c_str());
  cli.enable_server_certificate_verification(false);
  cli.enable_server_hostname_verification(false);
#else
  httplib::Client cli(domain.c_str());
#endif

  cli.set_follow_location(true);
  auto res = cli.Get(path.c_str());

  if (!res || res->status != 200) {
    std::cerr << "Chyba: " << res->status << std::endl;
    return "";
  }

  return res->body;
}

std::string URL::toString() const {
  return scheme + (scheme.empty() ? "" : "://") + domain + path.string();
}

std::ostream &operator<<(std::ostream &os, const URL &url) {
  return os << url.toString();
}

URL parseURL(const std::string &url) {
  // https://github.com/yhirose/cpp-httplib/issues/453
  // Regex breakdown:
  // ^(?:(https?):)? - Optional scheme (http or https)
  // (?://([^:/?#]*)(?::(\d+))?)? - Optional domain and port
  // ([^?#]*(?:\?[^#]*)?) - Path and optional query
  // (?:#.*)?$ - Optional fragment
  const static std::regex URL_REGEX(
      R"(^(?:(https?):)?(?://([^:/?#]*)(?::(\d+))?)?([^?#]*)(?:\?[^#]*)?(?:#.*)?)");

  std::smatch match;
  if (!std::regex_match(url, match, URL_REGEX)) {
    throw std::invalid_argument("Invalid URL");
  }

  std::string scheme = match[1];
  std::string domain = match[2];

  if (std::string port = match[3]; !port.empty()) {
    domain += ":" + port;
  }

  std::string path = match[4];

  std::filesystem::path fs_path{path.empty() ? "/" : path};

  return {scheme, domain, fs_path};
}

std::string safeURL(const std::string &url) {
  const auto parsed = parseURL(url);
  std::string result = parsed.domain + parsed.path.string();

  // Allow alphanumeric characters, hyphens, and underscores
  const auto isAllowed = [](char c) {
    return std::isalnum(c) || c == '-' || c == '_';
  };

  // Replace all characters with `_`
  std::transform(result.begin(), result.end(), result.begin(),
                 [&](char c) { return isAllowed(c) ? c : '_'; });

  // Remove trailing
  result.erase(result.find_last_not_of('_') + 1);

  return result;
}

std::string strip(const std::string &s) {
  static const std::regex whitespace(R"(^\s+|\s+$)");
  return std::regex_replace(s, whitespace, "");
}

bool path_is_inside(const std::filesystem::path &path,
                    const std::filesystem::path &base) {
  try {
    const auto relative = std::filesystem::relative(path, base);
    return !relative.empty() && relative.string().find("..") != 0;
  } catch (const std::filesystem::filesystem_error &) {
    // Handle cases where relative() throws, e.g., paths are on different root
    // drives
    std::cerr << "Error: " << std::strerror(errno) << std::endl;
    return false;
  }
}

} // namespace utils
