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

Semaphore sem_ukol(1);

void A() {
  sem_ukol.P(2);
  std::cout << "vlakna ";
  sem_ukol.V(3);

  sem_ukol.P(5);
  std::cout << "sranda ";
  sem_ukol.V(1);
}

void B() {
  sem_ukol.P(1);
  std::cout << "Synchronizovat ";
  sem_ukol.V(2);

  sem_ukol.P(3);
  std::cout << "neni ";
  sem_ukol.V(4);
}

void C() {
  sem_ukol.P(4);
  std::cout << "zadna ";
  sem_ukol.V(5);
}

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
