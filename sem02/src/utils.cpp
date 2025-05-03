/**
 * Kostra druhe semestralni prace z predmetu KIV/UPP
 * Soubory a hlavicku upravujte dle sveho uvazeni a nutnosti
 */

#include <vector>
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

std::vector<std::string> split(const std::string &s, char delim) {
  if (s.empty())
    return {};

  std::vector<std::string> elems;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

// https://github.com/yhirose/cpp-httplib/issues/453
const static std::regex URL_REGEX(
    R"(^(?:(https?):)?(?://([^:/?#]*)(?::(\d+))?)?([^?#]*(?:\?[^#]*)?)(?:#.*)?)");

URL parseURL(const std::string &url) {
  std::smatch match;
  if (!std::regex_match(url, match, URL_REGEX)) {
    throw std::invalid_argument("Invalid URL");
  }

  std::string scheme = match[1];
  std::string domain = match[2];
  std::string path = match[4];

  std::filesystem::path fs_path{path.empty() ? "/" : path};

  return {scheme, domain, fs_path};
}

std::string safeURL(const std::string &url) {
  const auto parsed = parseURL(url);
  std::string result = parsed.domain + parsed.pathToString();

  const std::unordered_set<char> whitelist{
      'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
      'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_'};

  for (char &c : result)
    if (whitelist.find(c) == whitelist.end())
      c = '_';

  return result;
}

} // namespace utils
