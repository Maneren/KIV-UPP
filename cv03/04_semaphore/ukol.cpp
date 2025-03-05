#include <iostream>
#include <thread>

#include "semaphore.h"

/*
 * Vas ukol je jednoduchy:
 *  - nejprve opravte implementaci Semaphore, aby fungoval, jak ma
 *  - pote doplnte semafory nutne k synchronizaci nize uvedeneho kodu, aby se
 * vypsalo: Synchronizovat vlakna neni zadna sranda
 *    - neupravujte vypisy samotne! Donutte vlakna synchronizovat se tak, aby se
 * text spravne prostridal
 */

// TODO: SEM DOPLNTE SEMAFORY

void A() {

  std::cout << "vlakna ";

  std::cout << "sranda ";
}

void B() {

  std::cout << "Synchronizovat ";

  std::cout << "neni ";
}

void C() { std::cout << "zadna "; }

// tuto funkci pak odnekud zavolejte - vymazte obsah main() a volejte ji
// tamodtud
void ukol() {

  std::cout << "Moudro dne: ";

  std::thread t1(A);
  std::thread t2(B);
  std::thread t3(C);

  t1.join();
  t2.join();
  t3.join();

  std::cout << std::endl;
}
