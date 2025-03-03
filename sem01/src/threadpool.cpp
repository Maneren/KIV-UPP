#include "threadpool.hpp"
#include <iostream>
#include <mutex>
#include <print>

thread_local WorkerThread *worker = nullptr;

WorkerThread::WorkerThread(size_t index) : index(index) {
  worker = this;
  // const auto &flag = pool.mRunning;
  // auto &cond = pool.mCondition;

  mThread = std::thread{[this] {
    while (pool.mRunning.load()) {
      if (auto maybe_task = pop_local(); maybe_task) {
        auto task = std::move(maybe_task.value());
        task();
        continue;
      }

      while (pool.mRunning.load()) {
        if (auto maybe_task = find_work(); maybe_task) {
          auto task = std::move(maybe_task.value());
          task();
          break;
        } else {
          std::unique_lock<std::mutex> lock(mMutex);
          pool.mCondition.wait(lock, [this] {
            return !pool.mRunning.load() || has_work() || pool.has_work();
          });
        }
      }
    }
  }};
};

WorkerThread::~WorkerThread() {
  mThread.join();
  worker = nullptr;
}

void WorkerThread::push(Task &&task) {
  std::unique_lock<std::mutex> lock(mMutex);
  tasks.push_back(std::move(task));
  pool.mCondition.notify_all();
}

std::optional<Task> WorkerThread::find_work() {
  return pop_local().or_else([this] { return pop_global(); }).or_else([this] {
    return steal();
  });
}

std::optional<Task> WorkerThread::pop_local() {
  std::unique_lock<std::mutex> lock(mMutex);

  if (tasks.empty())
    return std::nullopt;

  const auto task = tasks.front();
  tasks.pop_front();
  return task;
}

std::optional<Task> WorkerThread::pop_global() {
  std::unique_lock<std::mutex> lock(mMutex);

  return pool.pop_task();
}

std::optional<Task> WorkerThread::steal() { return pool.steal_task(index); }

Threadpool::Threadpool(size_t thread_count) {
  mWorkers.reserve(thread_count);

  for (std::size_t i = 0; i < thread_count; ++i) {
    mWorkers.emplace_back(i);
  }
}

void Threadpool::join() {
  mRunning.store(false);
  mCondition.notify_all();
  mWorkers.clear(); // joins the threads
}

void Threadpool::spawn(Task &&task) {
  if (!mRunning.load())
    return;

  if (worker) {
    worker->push(std::forward<Task>(task));
    return;
  }

  {
    std::unique_lock<std::mutex> lock(mMutex);
    mTasks.emplace(std::forward<Task>(task));
  }
  mCondition.notify_all();
}

std::optional<Task> Threadpool::steal_task(size_t stealer_index) {
  for (auto [index, victim] : mWorkers | std::views::enumerate) {
    if (index == stealer_index)
      continue;

    if (!mRunning.load())
      return std::nullopt;

    if (auto task = victim.pop_local())
      return task;
  }

  return std::nullopt;
}

Threadpool pool{};
