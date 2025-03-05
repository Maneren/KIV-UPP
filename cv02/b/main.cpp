#include <algorithm>
#include <chrono>
#include <execution>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

// jake kody v souboru urcite nekde jsou?
std::set<std::string> knownCodes{"9585:3179", "6731:6331", "5214:8140:8081"};

// desifruje vstupni retezec pomoci klice
std::string decrypt(const std::string &input, const std::string &key) {

  const size_t inputSize = input.size();
  std::string output(inputSize, ' ');

  for (size_t i = 0; i < inputSize; i++) {
    char c = input[i];
    if (c >= 'a' && c <= 'j') {
      output[i] = key[c - 'a'];
    } else {
      output[i] = c;
    }
  }

  return output;
}

// zjisti, zda vstupni retezec obsahuje vsechny zname kody
bool containsAllKnownCodes(const std::string &input) {
  size_t count = 0;
  for (const std::string &code : knownCodes) {
    if (input.find(code) == std::string::npos) {
      return false;
    }
  }
  return true;
}

int main(int argc, char **argv) {

  // nacteme soubor do retezce, abychom to pak meli snazsi
  std::ifstream file("encrypted.txt");
  std::string encrypted((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // Pokus o prolomeni sifry zacina zde //
  ////////////////////////////////////////////////////////////////////////////////////////////////

  std::string decryptMap = "0123456789"; // nekde zacit musime

  size_t counter = 0; // pocitadlo vyzkousenych permutaci

  // pro mereni casu
  auto start = std::chrono::high_resolution_clock::now();

  // zkusime vsechny permutace
  do {
    std::string decrypted = decrypt(encrypted, decryptMap);
    bool fits = containsAllKnownCodes(decrypted);
    if (fits) {
      break;
    }

    // pro potreby vypisu:
    counter++;
    if (counter % 100000 == 0) {
      // kolik casu uplynulo doted?
      auto end = std::chrono::high_resolution_clock::now();
      auto elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

      std::cout << "Vyzkouseno " << counter << " permutaci z 3628800 ("
                << (100 * counter / 3628800) << " %) za " << elapsed.count()
                << " ms" << std::endl;
    }
  } while (std::next_permutation(
      decryptMap.begin(), decryptMap.end())); // vygeneruje dalsi permutaci

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // Tady uz jen vypiseme vysledky a ukoncime program //
  ////////////////////////////////////////////////////////////////////////////////////////////////

  std::cout << "Nalezena desifrovaci mapa: " << decryptMap << std::endl;

  std::map<char, char> translationMap;
  for (size_t i = 0; i < 10; i++) {
    translationMap[decryptMap[i]] = 'a' + i;
  }

  std::cout << "Mapa desifrovani:" << std::endl;
  for (const auto &[key, value] : translationMap) {
    std::cout << value << " -> " << key << std::endl;
  }

  return 0;
}

// P.S.: spravna desifrovaci mapa je 3251649087

// ================
// CPU     99%
// user    19,575
// system  0,003
// total   19,595
// memory  8MB
// ================
