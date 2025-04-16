#include <iostream>
#include <random>
#include <vector>

constexpr size_t NumberCount =
    10'000'000; // tuto konstantu si kdyztak zmente dle sve konfigurace PC

int main(int argc, char **argv) {

  double prumer = 0;
  int max = std::numeric_limits<int>::min();
  int min = std::numeric_limits<int>::max();
  int pocetSudych = 0;
  double standardniOdchylka = 0;

  /*
   * Vas ukol:
   * 1) vygenerujte paralelne pole ruznych celych cisel o velikosti NumberCount
   *    - generovani nemusi byt uplne nahodne, jak se vam bude chtit; podstatou
   * je, aby cisla byla ruzna
   *    - dbejte na spravnou deklaraci typu promenne, apod
   *
   * 2) spocitejte/najdete paralelne:
   *    - aritmeticky prumer
   *    - maximum
   *    - minimum
   *    - pocet sudych cisel
   *    ! Kazdy ukol bude zpracovavany nanejvys jednim vlaknem (tj. nebude jeden
   * cyklus, ktery bude delat vse) ! nejde o datovy paralelismus, ale o
   * task-level paralelismus
   *
   * 3) spocitejte standardni odchylku (NOTE: je potreba mit aritmeticky prumer)
   *    - zde uz je zadouci pouzit datovy paralelismus
   *
   * *) mezi kazdymi kroky vytisknete zpravu, ze se zpracovava prave tenhle krok
   */

  std::vector<int> Numbers(NumberCount);

  std::mt19937_64 rng;
  std::normal_distribution<> dist(500000, 10000);

  std::cout << "Filling the vector..." << std::endl;

  // fill the vector with random numbers
#pragma omp parallel for schedule(dynamic, 10000) shared(rng)
  for (int i = 0; i < NumberCount; i++) {
    Numbers[i] = dist(rng);
  }

  std::cout << "Calculating avg, min, max..." << std::endl;

#pragma omp sections
  {
#pragma omp section
    {
      long sum = 0;

      for (const auto num : Numbers)
        sum += num;

      prumer = static_cast<double>(sum) / NumberCount;
    }

#pragma omp section
    {
      for (const auto num : Numbers) {
        if (num % 2 == 0)
          pocetSudych++;
      }
    }

#pragma omp section
    {
      for (const auto num : Numbers) {
        if (num < min)
          min = num;
      }
    }

#pragma omp section
    {
      for (const auto num : Numbers) {
        if (num > max)
          max = num;
      }
    }
  }

  std::cout << "Calculating standard deviation..." << std::endl;

#pragma omp parallel for reduction(+ : standardniOdchylka)
  for (const auto num : Numbers) {
    const auto diff = static_cast<double>(num - prumer);
    standardniOdchylka += diff * diff;
  }

  standardniOdchylka = std::sqrt(standardniOdchylka / NumberCount);

  std::cout << "Aritmeticky prumer:  " << prumer << std::endl;
  std::cout << "Standardni odchylka: " << standardniOdchylka << std::endl;
  std::cout << "Maximum:             " << max << std::endl;
  std::cout << "Minimum:             " << min << std::endl;
  std::cout << "Pocet sudych cisel:  " << pocetSudych << std::endl;

  return 0;
}
