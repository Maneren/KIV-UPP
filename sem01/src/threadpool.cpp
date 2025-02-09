#include "threadpool.hpp"

void Threadpool::spawn(std::function<void()> task) {
  mThreads.emplace_back(std::move(task));
}

void Threadpool::join() { mThreads.clear(); }
