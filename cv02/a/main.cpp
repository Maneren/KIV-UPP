#include <cstdint>
#include <future>
#include <iostream>
#include <ranges>
#include <thread>
#include <vector>

constexpr uint64_t Number_Of_Numbers = 100'000'000;

// Collatz conjecture
// https://en.wikipedia.org/wiki/Collatz_conjecture
//
// Tohle je pro nas jen priklad problemu, ktery nema predem znamy pocet kroku a
// tedy muze trvat kratkou i dlouhou dobu.
uint64_t collatz(uint64_t n) {
  uint64_t steps = 0;
  while (n != 1) {

    if (n % 2 == 0) {
      n /= 2;
    } else {
      n = 3 * n + 1;
    }
    steps++;
  }

  return steps;
}

int main(int argc, char **argv) {

  // Vas ukol:
  // 1) implementujte mereni casu pro cely vypocet
  // 2) zmerte, jak dlouho u vas trva vypocet seriove, tj. ve stavu, ve kterem
  // je ted. 3) pokuste se alespon drobne optimalizovat vypocet tak, aby trval
  // co nejmene casu i kdyz je seriovy 4) pokuste se vypocet paralelizovat a
  // zmerit, jak dlouho trva vypocet v paralelnim rezimu

  // NOTE: samozrejme, ze na to existuji efektivni zpusoby; dnes je ale cilem,
  // abyste si vyzkouseli paralelizovat "naivne", tedy tak, jak si myslite, ze
  // by to mohlo jit s vyuzitim prostredku, ktere znate. V lepsim pripade
  // zjistite, ze jste to mysleli spravne.
  //
  // Nekteri z Vas, kteri jiz maji zkusenosti s paralelnim programovanim budou
  // zde urcite ve vyhode. Pokuste se na reseni prijit doopravdy sami. Pokud
  // budete mit pocit, ze jste zasekli, nebojte se zeptat. Ale zkuste to
  // nejdrive sami. V kazdem pripade i vy zkusenejsi, zkuste pouzit jen to, co
  // doted zname (tj. std::thread, join, ...)

  // Hinty:
  // - nehledejte moznost optimalizace ve funkci collatz, nebo alespon ne hned
  // zkraje
  // - pouzijte std::thread pro vytvoreni vlaken; vytvorte staticky pocet
  // vlaken, idealne tolik, kolik mate CPU jader
  // - zmerit cas vypoctu muzete pomoci std::chrono::high_resolution_clock a
  // zpusobu, jaky jsme si ukazovali na prvnim cviceni
  // - alokace a dealokace pameti trva nejakou dobu; cim vetsi mnozstvi pameti,
  // tim delsi je prace s ni (zalezi, ale zpravidla to tak je)
  // - vzpomente si, ze na ukonceni vlakna muzete cekat pomoci metody join()

  // VAROVANI: vystup po optimalizaci a paralelizaci musi byt identicky! Tj.
  // porad chcete zjistit, ktere cislo ma nejvetsi pocet kroku a jaky je
  // prumerny pocet kroku. jedinou dovolenou odchylkou ve vystupu je cislo, pro
  // ktere byl zaznamenan maximalni pocet kroku (ale pocet kroku musi byt
  // stejny)

  const auto threads_available = std::thread::hardware_concurrency();
  std::vector<std::thread> threads{};

  std::vector<std::future<std::vector<uint64_t>>> futures =
      std::views::iota(1u, Number_Of_Numbers) |
      std::views::chunk(Number_Of_Numbers / threads_available + 1) |
      std::views::transform([&](const auto &numbers) {
        const auto task =
            std::make_shared<std::packaged_task<std::vector<uint64_t>()>>(
                [numbers] {
                  return numbers | std::views::transform(collatz) |
                         std::ranges::to<std::vector>();
                });

        threads.emplace_back([task]() { (*task)(); });

        return task->get_future();
      }) |
      std::ranges::to<std::vector>();

  uint64_t max = 0;
  uint64_t avg = 0;
  uint64_t maxIndex = 0;
  for (const auto [i, steps_future] : futures | std::views::enumerate) {
    for (const auto step : steps_future.get()) {
      if (step > max) {
        max = step;
        maxIndex = i;
      }
      avg += step;
    }
  }

  std::cout << "Nejvetsi nalezeny pocet kroku je " << max << " pro cislo "
            << maxIndex + 1 << std::endl;
  std::cout << "Prumerny pocet kroku je " << avg / Number_Of_Numbers
            << std::endl;

  for (auto &thread : threads) {
    thread.join();
  }

  return 0;
}

// Base:
//
// Nejvetsi nalezeny pocet kroku je 949 pro cislo 63728127
// Prumerny pocet kroku je 179
//
// ================
// CPU     99%
// user    17,132
// system  0,156
// total   17,330
// memory  1029MB
// ================

// Ranges:
//
// Nejvetsi nalezeny pocet kroku je 949 pro cislo 63728127
// Prumerny pocet kroku je 179
//
// ================
// CPU     99%
// user    16,842
// system  0,050
// total   16,902
// memory  766MB
// ================

// Threads:
//
// Nejvetsi nalezeny pocet kroku je 949 pro cislo 6
// Prumerny pocet kroku je 179
//
// ================
// CPU     745%
// user    23,824
// system  0,204
// total   3,221
// memory  709MB
// ================
