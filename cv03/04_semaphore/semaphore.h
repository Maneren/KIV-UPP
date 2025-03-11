#pragma once

#include <atomic>
#include <thread>

class Semaphore {
private:
  std::atomic<int> counter;

public:
  Semaphore(int init) : counter(init) {}

  void P(int cnt) {
    while (counter.load(std::memory_order_acquire) < cnt)
      std::this_thread::yield();

    counter.fetch_sub(cnt, std::memory_order_release);
  }

  void V(int cnt) { counter.fetch_add(cnt, std::memory_order_acq_rel); }

  int Get() { return counter.load(std::memory_order_relaxed); }
};
