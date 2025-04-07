#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

constexpr size_t Worker_Count = 16;
constexpr size_t Numbers_Count = 1'000'000'000;

std::vector<int> numbers;

class LockableInt {
private:
  int mVal;
  std::mutex mMutex;

public:
  LockableInt() : mVal(0) {}

  LockableInt &operator+=(const int rhs) {
    std::unique_lock<std::mutex> lck(mMutex);

    mVal += rhs;
    return *this;
  }

  operator int() const { return mVal; }
};

// (1) nezamykano, nechraneno - vysledek bude spatny, ale nejrychleji (to je
// zradne!) int sum; (2) nezamykano, nechraneno, volatilni pamet - vysledek bude
// spatny, o neco pomalejsi, ale stale docela rychly
// volatile int sum;
// (3) zamykano - vysledek bude spravny, ale vypocet bude trvat dele
// LockableInt sum;
// (4) nezamykano, ale zajistena atomicita (CAS, LL/SC, ...) - spravny vysledek;
// proc je ale casto pomalejsi nez (3)?
std::atomic<int> sum;

void Summator(size_t start, size_t end) {
  // pochopitelne lepsi reseni je zavest lokalni soucet a ten az pozdeji pricist
  // pro potreby demonstrace ponechme takto

  for (size_t i = start; i < end; i++) {
    // sum += numbers[i];
    sum.fetch_add(numbers[i], std::memory_order_consume);
  }
}

int main(int argc, char **argv) {
  numbers.resize(Numbers_Count);

  std::default_random_engine reng(1234);
  std::uniform_int_distribution<int> rdist(1, 10);

  for (size_t i = 0; i < Numbers_Count; i++)
    numbers[i] = rdist(reng);

  auto tp_start = std::chrono::steady_clock::now();

  std::vector<std::unique_ptr<std::thread>> workers;

  size_t step = Numbers_Count / Worker_Count;
  for (size_t i = 0; i < Worker_Count; i++)
    workers.push_back(
        std::make_unique<std::thread>(&Summator, i * step, (i + 1) * step));

  for (size_t i = 0; i < Worker_Count; i++) {
    if (workers[i]->joinable())
      workers[i]->join();
  }

  auto tp_end = std::chrono::steady_clock::now();

  std::cout << "Celkem:    "
            << std::chrono::duration_cast<std::chrono::milliseconds>(tp_end -
                                                                     tp_start)
                   .count()
            << "ms" << std::endl;
  std::cout << "Suma:      " << sum << std::endl;
  std::cout << "Ocekavano: 1205050551" << std::endl;

  return 0;
}
