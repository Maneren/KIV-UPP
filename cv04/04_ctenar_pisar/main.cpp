#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <vector>

// ERROR, suma = 4999936624
// Pocet chyb: 23995
// Prumerny cas ctenaru: 224656 us
// Prumerny cas pisaru: 847071 us
//
//
// OK
// Pocet chyb: 0
// Prumerny cas ctenaru: 4757975 us
// Prumerny cas pisaru: 4993915 us

// OK
// Pocet chyb: 0
// Prumerny cas ctenaru: 4881078 us
// Prumerny cas pisaru: 5182901 us

// mutex:
//   OK
//   Pocet chyb: 0
//   Prumerny cas ctenaru: 39015 us
//   Prumerny cas pisaru: 99344 us
//
// shared_mutex:
//   OK
//   Pocet chyb: 0
//   Prumerny cas ctenaru: 32821 us
//   Prumerny cas pisaru: 98576 us

#include "databaze.h"

constexpr size_t Pocet_Ctenaru = 24;
constexpr size_t Pocet_Pisaru = 4;

constexpr size_t Pocet_Operaci = 100;

std::atomic<size_t> Pocet_Chyb = 0;

std::array<size_t, Pocet_Ctenaru> Casy_Ctenari;
std::array<size_t, Pocet_Pisaru> Casy_Pisari;

std::shared_mutex mutex;

/*
 * Vlaknova funkce ctenare - cte z databaze, overuje, ze soucet hodnot je stale
 * 120
 */
void ctenar(size_t idx, Database &db) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(10, 50);

  Casy_Ctenari[idx] = 0;

  for (size_t i = 0; i < Pocet_Operaci; i++) {
    // pockame nahodnou dobu
    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

    // zmerime cas cteni
    auto start = std::chrono::high_resolution_clock::now();

    // precte vsechny hodnoty a secte je
    size_t sum = 0;
    {
      std::shared_lock lock(mutex);
      std::cout << "ctenar " << idx << std::endl;
      for (size_t i = 0; i < Velikost_Databaze; i++) {
        sum += db.read(i);
      }
      std::cout << "ctenar " << idx << " out" << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    Casy_Ctenari[idx] +=
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    // zkontroluje, ze soucet vsech hodnot je Spravny_Soucet()
    if (sum != Spravny_Soucet()) {
      Pocet_Chyb++;
    }
  }
}

/*
 * Vlaknova funkce pisare - zapisuje do databaze, zvetsuje a hned zmensuje
 * kazdou hodnotu o 1
 */
void pisar(size_t idx, Database &db) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(10, 50);

  Casy_Pisari[idx] = 0;

  for (size_t i = 0; i < Pocet_Operaci; i++) {
    // pockame nahodnou dobu
    std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));

    // zmerime cas zapisu
    auto start = std::chrono::high_resolution_clock::now();

    {
      std::unique_lock lock(mutex);
      std::cout << "pisar " << idx << std::endl;

      // zvetsi kazdou hodnotu v databazi o 1
      for (int i = 0; i < Velikost_Databaze; i++) {
        db.write(i, db.read(i) + 1);
      }

      // zmensi kazdou hodnotu v databazi o 1
      for (int i = 0; i < Velikost_Databaze; i++) {
        db.write(i, db.read(i) - 1);
      }

      std::cout << "pisar " << idx << " out" << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    Casy_Pisari[idx] +=
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    // = tohle znamena, ze po zasahu pisare se obsah DB nezmeni (v idealnim
    // pripade)
  }
}

int main(int argc, char **argv) {

  Database db;

  // spusti vlakna ctenaru a pisaru
  std::vector<std::unique_ptr<std::thread>> threads;
  for (size_t i = 0; i < Pocet_Ctenaru; i++) {
    threads.push_back(std::make_unique<std::thread>(ctenar, i, std::ref(db)));
  }
  for (size_t i = 0; i < Pocet_Pisaru; i++) {
    threads.push_back(std::make_unique<std::thread>(pisar, i, std::ref(db)));
  }
  for (size_t i = 0; i < Pocet_Ctenaru + Pocet_Pisaru; i++) {
    threads[i]->join();
  }

  // zkontroluje, ze soucet vsech hodnot v databazi je Spravny_Soucet()
  size_t sum = 0;
  for (size_t i = 0; i < Velikost_Databaze; i++) {
    sum += db.read(i);
  }

  if (sum == Spravny_Soucet()) {
    std::cout << "OK" << std::endl;
  } else {
    std::cout << "ERROR, suma = " << sum << std::endl;
  }

  std::cout << "Pocet chyb: " << Pocet_Chyb << std::endl;

  // zprumerujeme casy ctenaru a pisaru
  size_t prum_cas_ctenaru = 0;
  size_t prum_cas_pisaru = 0;

  for (size_t i = 0; i < Pocet_Ctenaru; i++) {
    prum_cas_ctenaru += Casy_Ctenari[i];
  }

  for (size_t i = 0; i < Pocet_Pisaru; i++) {
    prum_cas_pisaru += Casy_Pisari[i];
  }

  prum_cas_ctenaru /= Pocet_Ctenaru;
  prum_cas_pisaru /= Pocet_Pisaru;

  std::cout << "Prumerny cas ctenaru: " << prum_cas_ctenaru << " us"
            << std::endl;
  std::cout << "Prumerny cas pisaru: " << prum_cas_pisaru << " us" << std::endl;

  return 0;
}
