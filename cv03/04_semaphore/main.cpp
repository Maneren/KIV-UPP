#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "semaphore.h"

extern void ukol();

constexpr size_t Thread_Count = 4;
std::vector<size_t> Print_Counter;

Semaphore sem_tiskarny(3);

void printer_vlakno(int vlaknoId) {
  while (true) {
    // pokusime se ziskat pristup k tiskarne
    sem_tiskarny.P(1);

    // tiskneme
    Print_Counter[vlaknoId]++;

    // Pro ladeni muzete odkomentovat vypis:
    // std::string out = "[" + std::to_string(vlaknoId) + "] Tisknu... ";
    // std::cout << out << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // uvolnime tiskarnu
    sem_tiskarny.V(1);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

int main(int argc, char **argv) {
  for (int i = 0; i < 30; i++)
    ukol();
  std::this_thread::sleep_for(std::chrono::seconds(1));

  Print_Counter.resize(Thread_Count);

  std::vector<std::unique_ptr<std::thread>> threads(Thread_Count);
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i] = std::make_unique<std::thread>(&printer_vlakno, i);
    Print_Counter[i] = 0;
  }

  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    for (size_t i = 0; i < Print_Counter.size(); i++) {
      std::cout << "Vlakno " << i << " vytisklo " << Print_Counter[i]
                << " stran." << std::endl;
    }
    std::cout << "Zbyva " << sem_tiskarny.Get() << " mist v tiskarne."
              << std::endl;
    assert(sem_tiskarny.Get() == 0);
  }

  for (size_t i = 0; i < threads.size(); i++) {
    threads[i]->join();
  }

  return 0;
}
