#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// kvuli omp_get_thread_num()
#include <omp.h>

void barrier_test() {

#pragma omp parallel
  {

    for (int i = 0; i < 10; i++) {
      std::string msg = "[" + std::to_string(omp_get_thread_num()) +
                        "] XXXXXXXXXX " + std::to_string(i) + " ... \n";

      std::cout << msg;
      std::cout.flush();
      std::this_thread::sleep_for(
          std::chrono::milliseconds(500 + omp_get_thread_num() * 50));
    }

// odkomentujeme, pokud chceme vlakna sesynchronizovat pred prechodem do
// dalsi casti
#pragma omp barrier

// jedno z vlaken muze vypsat, ze se synchronizujeme - zkusime to udelat s
// barierou vyse i bez ni
#pragma omp single
    std::cout << "[" + std::to_string(omp_get_thread_num()) +
                     "] SYNCHRONIZACE ! \n";

    for (int i = 0; i < 10; i++) {
      std::string msg = "[" + std::to_string(omp_get_thread_num()) +
                        "] |||||||||| " + std::to_string(i) + " ... \n";

      std::cout << msg;
      std::cout.flush();
      std::this_thread::sleep_for(
          std::chrono::milliseconds(500 + omp_get_thread_num() * 50));
    }
  }
}

void parallel_sections() {
#pragma omp parallel sections
  {
    // kazde vlakno provede jednu sekci

#pragma omp section
    {
      std::cout << "[" + std::to_string(omp_get_thread_num()) + "] sekce 1 \n";
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
#pragma omp section
    {
      std::cout << "[" + std::to_string(omp_get_thread_num()) + "] sekce 2 \n";
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
#pragma omp section
    {
      std::cout << "[" + std::to_string(omp_get_thread_num()) + "] sekce 3 \n";
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
#pragma omp section
    {
      std::cout << "[" + std::to_string(omp_get_thread_num()) + "] sekce 4 \n";
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  }
}

int main(int argc, char **argv) {

  // nastavme pocet vlaken na 8, abychom ve vystupu nemeli zbytecne neporadek
  omp_set_num_threads(8);

  barrier_test();
  // parallel_sections();

  return 0;
}
