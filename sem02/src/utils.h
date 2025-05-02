/**
 * Kostra druhe semestralni prace z predmetu KIV/UPP
 * Soubory a hlavicku upravujte dle sveho uvazeni a nutnosti
 */

#pragma once

#include <string>

namespace utils {
// precte cely soubor do retezce
// path - cesta k souboru
// vraci obsah souboru nebo prazdny retezec v pripade chyby
std::string readWholeFile(const std::string &path);

// stahne HTML kod stranky z dane URL
// url - adresa stranky
// vraci obsah stranky nebo prazdny retezec v pripade chyby
std::string downloadHTML(const std::string &url);

// struktura reprezentujici URL
struct URL {
  std::string string;

  std::string scheme;
  std::string domain;
  std::string path;

  std::string toString() const;
};

// prevede URL do struktury
URL parseURL(const std::string &url);

// prevede URL do bezpecne formy
// url - nebezpeceny URL
// vraci string, kde jsou nebezpecene znaky nahrazeny podtrzitky
std::string safeURL(const std::string &url);
} // namespace utils
