#include <chrono>
#include <iostream>
#include <memory>
#include <omp.h>
#include <random>
#include <thread>
#include <vector>

// pocet cisel, se kterymi se bude pracovat
constexpr size_t NumbersCount = 100'000'000;
// kolikrat se ma provest experiment (a mereni)
constexpr size_t RunCount = 5;

std::vector<double> Numbers;

// funkce pro mereni - ponechme bez vetsiho komentare, je stejna jako u
// predchozich cviceni jen jednou spusti na prazdno, a pak spusti nekolikrat
// mereni po sobe a zmeri prumerny cas
void Do_Measure(const std::string &name, void (*fnc)()) {
  std::cout << "Measurement: " << name << std::endl;

  std::cout << "Dry run ... ";
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

    std::cout << " OK" << std::endl;
  }
  tm /= RunCount;

  std::cout << "Average time: " << tm << "ms" << std::endl << std::endl;
}

/*
 * Nasleduji funkce implementujici tri redukcni problemy:
 * 1) suma prvku
 * 2) hledani minima
 * 3) hledani maxima
 */

// seriova verze - asi neni treba komentare
void reduce_serial() {
  double sum = 0;
  double min = std::numeric_limits<double>::max();
  double max = -min;

  for (size_t i = 0; i < Numbers.size(); i++) {
    sum += Numbers[i];
    if (Numbers[i] < min)
      min = Numbers[i];
    if (Numbers[i] > max)
      max = Numbers[i];
  }

  std::cout << "(" << sum << ", " << min << ", " << max << ")";
}

// paralelni verze s vlakny
void reduce_threads() {
  double sum = 0;
  double min = std::numeric_limits<double>::max();
  double max = -min;

  std::vector<std::unique_ptr<std::thread>> workers;
  const size_t cpus = std::thread::hardware_concurrency();
  const size_t portion = Numbers.size() / cpus;

  // nezavisle promenne, kazde vlakno ma vlastni verzi
  std::vector<double> loc_sum;
  std::vector<double> loc_min;
  std::vector<double> loc_max;
  for (size_t i = 0; i < cpus; i++) {
    loc_sum.push_back(sum);
    loc_min.push_back(min);
    loc_max.push_back(max);
  }

  // vytvoreni workeru
  for (size_t w = 0; w < cpus; w++) {
    workers.push_back(std::make_unique<std::thread>(
        [&](size_t id, size_t begin, size_t end) {
          // kazdy worker si svou cast redukuje do sve nezavisle promenne
          for (size_t i = begin; i < end; i++) {
            loc_sum[id] += Numbers[i];
            if (Numbers[i] < loc_min[id])
              loc_min[id] = Numbers[i];
            if (Numbers[i] > loc_max[id])
              loc_max[id] = Numbers[i];
          }
        },
        w, w * portion, (w + 1) * portion));
  }

  // pockani na workery
  for (size_t w = 0; w < cpus; w++) {
    if (workers[w]->joinable())
      workers[w]->join();

    // vnejsi vlakno pak sebere vysledky a redukuje je do finalniho vysledku

    sum += loc_sum[w];
    if (loc_min[w] < min)
      min = loc_min[w];
    if (loc_max[w] > max)
      max = loc_max[w];
  }

  std::cout << "(" << sum << ", " << min << ", " << max << ")";
}

// paralelni verze s OpenMP - staticka delba prace
void reduce_openmp_static() {
  double sum = 0;
  double min = std::numeric_limits<double>::max();
  double max = -min;

  // od OpenMP 3 mame k dispozici redukcni operatory min a max, pak jde opet jen
  // o tentyz kod jako u seriove verze, jen dekorovany pragmou
#if _OPENMP > 200800
#pragma omp parallel for reduction(+ : sum) reduction(min : min)               \
    reduction(max : max) schedule(static)
  for (int i = 0; i < Numbers.size(); i++) {
    sum += Numbers[i];
    if (Numbers[i] < min)
      min = Numbers[i];
    if (Numbers[i] > max)
      max = Numbers[i];
  }
#else // pokud mame nizsi verzi OpenMP, musime se zaridit jinak - treba
      // privatnimi promennymi

  // sablona pro privatni promenne
  double loc_min = min, loc_max = max;

  // parallel blok, do ktereho preneseme lokalni verze promennych inicializovane
  // na startovni hodnoty
#pragma omp parallel shared(min, max) firstprivate(loc_min, loc_max)
  {
    // az uvnitr spustime for cyklus (pozor, ne parallel for, cyklus uz
    // paralelni je diky tomu, ze je v parallel bloku)
#pragma omp for reduction(+ : sum) schedule(static)
    for (int i = 0; i < Numbers.size(); i++) {
      sum += Numbers[i];
      if (Numbers[i] < loc_min)
        loc_min = Numbers[i];
      if (Numbers[i] > loc_max)
        loc_max = Numbers[i];
    }

    // v kriticke sekci redukujeme privatni promenne do sdilenych v kriticke
    // sekci
#pragma omp critical
    {
      if (loc_min < min)
        min = loc_min;
      if (loc_max > max)
        max = loc_max;
    }
  }
#endif

  std::cout << "(" << sum << ", " << min << ", " << max << ")";
}

// paralelni verze s OpenMP - dynamicky planovana - tohle se muze docela hodne
// vymstit, protoze redukcni operace nemusi byt tak primocare, jak se zda, a
// pokud bude pridelovana prace po jednotkach, dojde ke zbytecnemu znasobeni
// poctu operaci
// vyzkousejte ruznou granularitu pri pridelovani prace, treba se dostanete
// alespon na srovnatelny cas zpracovani
void reduce_openmp_dynamic() {
  double sum = 0;
  double min = std::numeric_limits<double>::max();
  double max = -min;

#if _OPENMP > 200800
#pragma omp parallel for reduction(+ : sum) reduction(min : min)               \
    reduction(max : max) schedule(dynamic)
  for (int i = 0; i < Numbers.size(); i++) {
    sum += Numbers[i];
    if (Numbers[i] < min)
      min = Numbers[i];
    if (Numbers[i] > max)
      max = Numbers[i];
  }
#else

  double loc_min = min, loc_max = max;

#pragma omp parallel shared(min, max) firstprivate(loc_min, loc_max)
  {
#pragma omp for reduction(+ : sum)                                             \
    schedule(dynamic) // vyzkousejte ruznou granularitu
    for (int i = 0; i < Numbers.size(); i++) {
      sum += Numbers[i];
      if (Numbers[i] < loc_min)
        loc_min = Numbers[i];
      if (Numbers[i] > loc_max)
        loc_max = Numbers[i];
    }

#pragma omp critical
    {
      if (loc_min < min)
        min = loc_min;
      if (loc_max > max)
        max = loc_max;
    }
  }
#endif

  std::cout << "(" << sum << ", " << min << ", " << max << ")";
}

int main(int argc, char **argv) {
  std::default_random_engine reng(123); // konstantni seed, aby byla cisla stale
                                        // stejna mezi spustenimi programu
  std::normal_distribution<double> dist(0, 100.0);

  Numbers.reserve(NumbersCount);
  for (size_t i = 0; i < NumbersCount; i++)
    Numbers.push_back(dist(reng));

  Do_Measure("Serial version", &reduce_serial);
  Do_Measure("Parallel version (threads)", &reduce_threads);
  Do_Measure("Parallel version (OpenMP, static)", &reduce_openmp_static);
  Do_Measure("Parallel version (OpenMP, dynamic)", &reduce_openmp_dynamic);

  return 0;
}
