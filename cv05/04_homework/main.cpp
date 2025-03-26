#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <print>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// velikost obrazku MNIST datove sady
constexpr int ImageWidth = 28;
constexpr int ImageHeight = 28;

// predpocitane vahy pro klasifikaci; zjisteny z trenovaci sady
std::array<double, 10> avgrs = {1.65594, 1.00886, 1.27979, 1.19485, 1.42585,
                                1.19124, 1.39466, 1.14478, 1.48488, 1.32505};
std::array<double, 10> avgcs = {1.62732, 1.08876, 1.75115, 2.07454, 1.30706,
                                1.86389, 1.67646, 1.45069, 1.95342, 1.70459};

// nacteni BMP souboru ze souboru; bereme klasicky BMP soubor s 8 bity na pixel
std::vector<unsigned char> Load_BMP(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    std::cout << "Soubor " << filename << " se nepodarilo otevrit!"
              << std::endl;
    return {};
  }

  // nacteni BMP do matice (cernobily obrazek)
  unsigned char header[54];
  file.read(reinterpret_cast<char *>(header), 54);
  int width = header[18] + (header[19] << 8);
  int height = header[22] + (header[23] << 8);

  // preskocime offset na data
  file.seekg(header[10] + (header[11] << 8));

  const int size = width * height;
  std::vector<unsigned char> data(size);
  file.read(reinterpret_cast<char *>(data.data()), size);

  return data;
}

// otoceni obrazku - v BMP jsou kodovane "obracene"
void Transform(std::vector<unsigned char> &vec) {

  for (size_t i = 0; i < ImageHeight / 2; i++) {
    for (size_t j = 0; j < ImageWidth; j++) {
      // otocit vertikalne
      std::swap(vec[i * ImageWidth + j],
                vec[(ImageHeight - i - 1) * ImageWidth + j]);
    }
  }
}

// binarizace obrazku - zprahujeme na cernou a bilou, tj. true nebo false
std::vector<std::vector<bool>> Binarize(const std::vector<unsigned char> &vec) {
  std::vector<std::vector<bool>> result(ImageHeight,
                                        std::vector<bool>(ImageWidth));
  for (size_t i = 0; i < ImageHeight; i++) {
    for (size_t j = 0; j < ImageWidth; j++) {
      result[i][j] = (vec[i * ImageWidth + j] > 50);
    }
  }
  return result;
}

// jednoduchy klasifikator; druhy nejstupidnejsi hned po nahodnem hadani
int Classify(const std::vector<std::vector<bool>> &data) {

  // spocitame neprerusene sekvence jednicek v radcich a sloupcich
  std::vector<int> row_counts(ImageHeight);
  for (int i = 0; i < ImageHeight; i++) {
    int count = 0;
    for (int j = 0; j < ImageWidth; j++) {
      if (data[i][j]) {
        count++;
      } else {
        if (count > 0) {
          row_counts[i]++;
          count = 0;
        }
      }
    }
    if (count > 0) {
      row_counts[i]++;
    }
  }

  std::vector<int> col_counts(ImageWidth);
  for (int j = 0; j < ImageWidth; j++) {
    int count = 0;
    for (int i = 0; i < ImageHeight; i++) {
      if (data[i][j]) {
        count++;
      } else {
        if (count > 0) {
          col_counts[j]++;
          count = 0;
        }
      }
    }
    if (count > 0) {
      col_counts[j]++;
    }
  }

  // zprumerujeme nenulove hodnoty pro radky a sloupce
  double avg_row = 0;
  double avg_col = 0;
  int row_count = 0;
  int col_count = 0;
  for (int i = 0; i < ImageHeight; i++) {
    if (row_counts[i] > 0) {
      avg_row += row_counts[i];
      row_count++;
    }
  }
  for (int j = 0; j < ImageWidth; j++) {
    if (col_counts[j] > 0) {
      avg_col += col_counts[j];
      col_count++;
    }
  }
  avg_row /= row_count;
  avg_col /= col_count;

  // spocitame vzdalenost od predtrenovanych vah
  double dists[10];
  for (int i = 0; i < 10; i++) {
    dists[i] = std::sqrt((avg_row - avgrs[i]) * (avg_row - avgrs[i]) +
                         (avg_col - avgcs[i]) * (avg_col - avgcs[i]));
  }

  // nalezneme nejblizsi vahy, tj. nejblizsi cislo
  int min_index = 0;
  double min_dist = dists[0];
  for (int i = 1; i < 10; i++) {
    if (dists[i] < min_dist) {
      min_dist = dists[i];
      min_index = i;
    }
  }

  return min_index;
}

int main(int argc, char **argv) {

  if (!std::filesystem::is_directory("mnist")) {
    std::cout << "Slozka mnist neexistuje!" << std::endl;
    return 1;
  }

  /*
   * Ukazkovy priklad na pipelined multi-threading (PMT)
   * V soucasnem stavu je program single-threaded, tj. zpracovava obrazky
   * postupne Vasim ukolem je zpracovat obrazky paralelne, tj. vytvorit vlakna,
   * ktera budou zpracovavat obrazky v pipeline Napr.: zatimco jedno vlakno
   * klasifikuje, druhe muze nacitat dalsi obrazek
   *
   * Pokuste se maximalizovat pipelining, tj. vidite 4 vypocetni faze, takze
   * vytvorte 4 vlakna, kde kazde bude cekat na dokonceni predchozi faze a
   * jakmile bude dokoncena, spusti svou praci
   */

  auto tp_start = std::chrono::steady_clock::now();

  int total = 0;
  int correct = 0;

  std::deque<std::pair<int, std::vector<unsigned char>>> loaded_images;
  std::deque<std::pair<int, std::vector<unsigned char>>> transformed_images;
  std::deque<std::pair<int, std::vector<std::vector<bool>>>> binarized_images;

  std::mutex m_loaded_images;
  std::mutex m_transformed_images;
  std::mutex m_binarized_images;

  std::condition_variable cv_loaded_images;
  std::condition_variable cv_transformed_images;
  std::condition_variable cv_binarized_images;

  // for (int num = 0; num <= 9; num++) {
  //   for (auto d :
  //        std::filesystem::directory_iterator("mnist/" + std::to_string(num)))
  //        {
  //     // jen bmp soubory
  //     if (d.path().extension() != ".bmp") {
  //       continue;
  //     }
  //
  //     // 1) nacist obrazek
  //     auto data = Load_BMP(d.path().string());
  //
  //     // 2) transformovat jej
  //     Transform(data);
  //
  //     // 3) zprahovat do binarniho vektoru
  //     const auto binarized = Binarize(data);
  //
  //     // 4) klasifikovat
  //     const int label = Classify(binarized);
  //
  //     // 5) porovnat s ocekavanym cislem
  //     if (label == num) {
  //       correct++;
  //     }
  //     total++;
  //   }
  // }

  std::atomic<bool> loading_done{false};
  std::atomic<bool> transforming_done{false};
  std::atomic<bool> binarizing_done{false};

  std::array threads{
      std::thread{[&]() {
        for (int num = 0; num <= 9; num++) {
          for (auto d : std::filesystem::directory_iterator(
                   "mnist/" + std::to_string(num))) {
            // jen bmp soubory
            if (d.path().extension() != ".bmp") {
              continue;
            }
            const auto data = Load_BMP(d.path().string());
            {
              std::lock_guard<std::mutex> lock(m_loaded_images);
              loaded_images.emplace_back(num, std::move(data));
            }
            cv_loaded_images.notify_one();
          }
        }
        loading_done.store(true);
        cv_loaded_images.notify_all();
      }},
      std::thread{[&] {
        while (true) {
          int num;
          std::vector<unsigned char> data;
          {
            std::unique_lock<std::mutex> lock(m_loaded_images);

            cv_loaded_images.wait(
                lock, [&] { return !loaded_images.empty() || loading_done; });

            if (loaded_images.empty() && loading_done) {
              transforming_done.store(true);
              cv_transformed_images.notify_all();
              break;
            }

            const auto &[num_, data_] = loaded_images.front();
            num = num_;
            data = std::move(data_);
            loaded_images.pop_front();
          }
          Transform(data);
          {
            std::lock_guard<std::mutex> lock(m_transformed_images);
            transformed_images.emplace_back(num, std::move(data));
          }
          cv_transformed_images.notify_one();
        }
      }},
      std::thread{[&] {
        while (true) {
          int num;
          std::vector<unsigned char> data;
          {
            std::unique_lock<std::mutex> lock(m_transformed_images);

            cv_transformed_images.wait(lock, [&] {
              return !transformed_images.empty() || transforming_done;
            });

            if (transformed_images.empty() && transforming_done) {
              binarizing_done.store(true);
              cv_binarized_images.notify_all();
              break;
            }

            const auto &[num_, data_] = transformed_images.front();
            num = num_;
            data = std::move(data_);
            transformed_images.pop_front();
          }
          const auto binarized = Binarize(data);
          {
            std::lock_guard<std::mutex> lock(m_binarized_images);
            binarized_images.emplace_back(num, std::move(binarized));
          }
          cv_binarized_images.notify_one();
        }
      }},
      std::thread{[&] {
        while (true) {
          int num;
          std::vector<std::vector<bool>> binarized;
          {
            std::unique_lock<std::mutex> lock(m_binarized_images);

            cv_binarized_images.wait(lock, [&] {
              return !binarized_images.empty() || binarizing_done;
            });

            if (binarized_images.empty() && binarizing_done) {
              break;
            }

            const auto &[num_, binarized_] = binarized_images.front();
            num = num_;
            binarized = std::move(binarized_);
            binarized_images.pop_front();
          }
          const int label = Classify(binarized);

          if (label == num) {
            correct++;
          }
          total++;
        }
      }}};

  for (auto &t : threads) {
    t.join();
  }

  std::cout << "Celkem: " << total << ", spravne: " << correct << std::endl;

  std::cout << "Presnost: " << static_cast<double>(correct) / total * 100.0
            << "%" << std::endl;

  auto tp_end = std::chrono::steady_clock::now();

  std::cout << "Doba zpracovani: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(tp_end -
                                                                     tp_start)
                   .count()
            << "ms" << std::endl;

  return 0;
}
