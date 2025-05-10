#include <algorithm>
#include <chrono>
#include <execution>
#include <iostream>
#include <random>
#include <vector>

constexpr size_t NumbersCount = 10'000'000;
// kolikrat se ma provest experiment (a mereni)
constexpr size_t RunCount = 5;

// funkce pro mereni - ponechme bez vetsiho komentare, je stejna jako u
// predchozich cviceni jen jednou spusti na prazdno, a pak spusti nekolikrat
// mereni po sobe a zmeri prumerny cas
void Do_Measure(const std::string &name, void (*fnc)()) {
  std::cout << "Measurement: " << name << std::endl;

  std::cout << "Dry run ...";
  fnc();
  std::cout << " OK" << std::endl;

  unsigned long long tm = 0;

  for (size_t i = 0; i < RunCount; i++) {
    std::cout << "Run " << i << " ... ";

    auto st = std::chrono::steady_clock::now();

    fnc();

    auto end = std::chrono::steady_clock::now();
    tm +=
        std::chrono::duration_cast<std::chrono::milliseconds>(end - st).count();

    std::cout << "OK" << std::endl;
  }
  tm /= RunCount;

  std::cout << "Average time: " << tm << "ms" << std::endl << std::endl;
}

/*
 * Na nasledujicich algoritmech si muzete vyzkouset paralelizaci s PSTL a C++17
 *
 * V podstate staci jako prvni parametr do vybranych metod pridat
 * std::execution::par_unseq, paralelizace se pak zaridi "skoro sama"
 */

// spolecna funkce - naplni vektor nahodnymi cisly
void fill_vector(std::vector<double> &vec, size_t num) {
  thread_local std::default_random_engine reng(456);
  std::uniform_real_distribution<double> rdist(-100.0, 100.0);

  vec.resize(num);
  std::for_each(std::execution::par_unseq, vec.begin(), vec.end(),
                [&](double &trg) { trg = rdist(reng); });
}

// naplni vektor a transformuje hodnoty
void test_fill_transform() {
  std::vector<double> a;
  fill_vector(a, NumbersCount);

  std::for_each(std::execution::par_unseq, a.begin(), a.end(),
                [](double &num) { num *= 2; });
}

// naplni vektor, zkopiruje ho, seradi a transformuje hodnoty
void test_fill_copy_sort_transform() {
  std::vector<double> a;
  fill_vector(a, NumbersCount);

  std::vector<double> b(NumbersCount);
  std::copy(std::execution::par_unseq, a.begin(), a.end(), b.begin());

  std::sort(std::execution::par_unseq, b.begin(), b.end());

  std::transform(std::execution::par_unseq, b.begin(), b.end(), b.begin(),
                 [](const double e) { return e / 2.0; });
}

int main(int argc, char **argv) {

  Do_Measure("Test fill vector", &test_fill_transform);
  Do_Measure("Test fill,copy,sort,transform vector",
             &test_fill_copy_sort_transform);

  return 0;
}
