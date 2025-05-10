#include <algorithm>
#include <chrono>
#include <execution>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

constexpr size_t NumbersCount = 100'000'000;
// kolikrat se ma provest experiment (a mereni)
constexpr size_t RunCount = 5;

// funkce pro mereni - ponechme bez vetsiho komentare, je stejna jako u
// predchozich cviceni jen jednou spusti na prazdno, a pak spusti nekolikrat
// mereni po sobe a zmeri prumerny cas
void Do_Measure(const std::string &name, void (*fnc)()) {
  std::cout << "Measurement: " << name << std::endl;

  std::cout << "Dry run ... ";
  fnc();
  std::cout << "OK" << std::endl;

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

void fill_vector(std::vector<double> &vec, size_t num) {
  // vygenerujeme cisla podle nejakeho navodu, at se nemusime moc zaobirat rezii
  // generatoru pseudonahodnych cisel
  vec.resize(num);
  std::for_each(std::execution::par_unseq, vec.begin(), vec.end(),
                [&](double &x) {
                  size_t idx = std::distance(&(*vec.begin()), &x);
                  x = ((12301 + idx * 125) % 9200) / 1.125 - 1500.0;
                });
}

void test_find_min_idx_serial() {
  std::vector<double> numbers;
  fill_vector(numbers, NumbersCount);

  size_t min_idx = 0;
  double min_val = numbers[0];

  for (size_t i = 0; i < numbers.size(); i++) {
    if (numbers[i] < min_val) {
      min_val = numbers[i];
      min_idx = i;
    }
  }

  std::cout << "numbers[" << min_idx << "] = " << min_val << " ";
}

void test_find_min_idx_parallel() {
  std::vector<double> numbers;
  fill_vector(numbers, NumbersCount);

  std::vector<size_t> cpus(std::thread::hardware_concurrency());
  std::iota(cpus.begin(), cpus.end(), 0);

  // nezavisle promenne
  std::vector<size_t> min_idxs(cpus.size());

  // vlastne substituce za for, co vytvori N vlaken a spusti v nich to, co je
  // uvnitr nasledujici lambdy
  std::for_each(std::execution::par_unseq, cpus.begin(), cpus.end(),
                [&](size_t x) {
                  const size_t start = x * numbers.size() / cpus.size();
                  const size_t end = (x + 1) * numbers.size() / cpus.size();

                  min_idxs[x] = 0;

                  for (size_t i = start; i < end; i++) {
                    if (numbers[i] < numbers[min_idxs[x]]) {
                      min_idxs[x] = i;
                    }
                  }
                });

  size_t global_min_idx = 0;

  for (size_t i = 0; i < cpus.size(); i++) {
    if (numbers[min_idxs[i]] < numbers[global_min_idx]) {
      global_min_idx = min_idxs[i];
    }
  }

  std::cout << "numbers[" << global_min_idx << "] = " << numbers[global_min_idx]
            << " ";
}

void test_find_min_reduce() {
  std::vector<double> numbers;
  fill_vector(numbers, NumbersCount);

  std::vector<size_t> indexes(NumbersCount);
  std::iota(indexes.begin(), indexes.end(), 0);

  auto min_idx =
      std::reduce(std::execution::par_unseq, indexes.begin(), indexes.end(),
                  static_cast<size_t>(0), [&](size_t a, size_t b) -> size_t {
                    return numbers[a] < numbers[b] ? a : b;
                  });

  std::cout << "numbers[" << min_idx << "] = " << numbers[min_idx] << " ";
}

int main(int argc, char **argv) {
  Do_Measure("Find min index (serial)", &test_find_min_idx_serial);
  Do_Measure("Find min index (parallel)", &test_find_min_idx_parallel);
  Do_Measure("Find min index (reduce)", &test_find_min_reduce);

  return 0;
}
