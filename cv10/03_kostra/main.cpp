#include <algorithm>
#include <array>
#include <atomic>
#include <execution>
#include <iostream>
#include <random>
#include <vector>

constexpr size_t NumbersCount = 100'000'000;

void fill_vector(std::vector<double> &vec, size_t num) {
  vec.resize(num);

  // generovani nahodnych cisel gaussovskym rozdelenim
  std::default_random_engine reng(456);
  std::normal_distribution<double> rdist(100.0, 3.0);
  // NOTE: generovat cisla chceme doopravdy seriove - generovani cisel jednim
  // generatorem neni thread-safe
  std::for_each(std::execution::seq, vec.begin(), vec.end(),
                [&](double &trg) { trg = rdist(reng); });
}

int main(int argc, char **argv) {
  std::vector<double> numbers;
  fill_vector(numbers, NumbersCount);

  /*
   * Itinerar algoritmu:
   *  - std::reduce - redukcni operace
   *  - std::transform - zmena prvku ve vektoru
   *  - std::for_each - prochazeni prvku vektoru
   *  - std::sort - serazeni prvku vektoru
   *  - std::minmax_element - najde minimum a maximum vektoru
   */

  /*
   * Vas ukol (vse paralelne):
   *  1) najdete minimum a maximum ve vektoru
   *  2) skalujte vsechny prvky vektoru na 0-1 (tj. minimalni prvek bude
   * odpovidat cislu 0, maximalni prvek cislu 1, vsechny ostatni budou v
   * rozmezi) 3) seradte vektor vzestupne
   */

  const auto [min, max] = std::minmax_element(std::execution::par_unseq,
                                              numbers.begin(), numbers.end());

  std::for_each(
      std::execution::par_unseq, numbers.begin(), numbers.end(),
      [min = *min, max = (*max - *min)](double &n) { n = (n - min) / max; });

  std::sort(std::execution::par_unseq, numbers.begin(), numbers.end());

  // Vytiskneme prvnich 10 prvku pro kontrolu - mely by to byt same nuly
  std::cout << "Prvnich 10 prvku: ";
  for (size_t i = 0; i < 10 && i < numbers.size(); ++i) {
    std::cout << numbers[i] << " ";
  }
  // a poslednich 10 prvku pro kontrolu - mely by to byt same jednotky
  std::cout << std::endl << "Poslednich 10 prvku: ";
  for (size_t i = numbers.size() - 10; i < numbers.size(); ++i) {
    std::cout << numbers[i] << " ";
  }
  std::cout << std::endl;

  /*
   * Bonusovy ukol na doma: vykreslete histogram hodnot, cisla normalizovana na
   * rozsah 0-1 "roztridte" do binu po 0.05 (tj. prvni budou cisla od 0 do 0.05,
   * dalsi od 0.05 do 0.1 a tak dale) V nejvetsim binu (tj. nejvice hodnot)
   * vypisete 20 hvezdicek, ostatni proporcne zmensite Priklad vystupu:
   *
   * **
   * ****
   * ******
   * *********
   * *************
   * ****************
   * ******************
   * *******************
   * ********************
   * *******************
   * ******************
   * ****************
   * ************
   * *********
   * *****
   * **
   * *
   *
   */

  constexpr size_t BIN_COUNT = 100 / 5;
  std::array<std::atomic_size_t, BIN_COUNT> bins;

  std::for_each(
      std::execution::par_unseq, numbers.begin(), numbers.end(),
      [&bins](const auto n) {
        bins[std::min(static_cast<size_t>(n * 20), BIN_COUNT - 1)].fetch_add(
            1, std::memory_order_relaxed);
      });

  const auto [minc, maxc] = std::minmax_element(bins.begin(), bins.end());

  std::for_each(std::execution::par_unseq, bins.begin(), bins.end(),
                [minc = minc->load(), maxc = (maxc->load() - minc->load())](
                    auto &n) { n.store((n.load() - minc) * 20 / maxc); });

  for (const auto &count : bins) {
    for (size_t i = 0; i < count.load(); i++) {
      std::cout << "*";
    }
    std::cout << std::endl;
  }

  return 0;
}
