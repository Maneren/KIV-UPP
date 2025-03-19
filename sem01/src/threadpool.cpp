#include "threadpool.hpp"
#include <algorithm>

Threadpool::Threadpool(size_t thread_count) {
  thread_count = std::min(thread_count, static_cast<size_t>(8));
  mWorkers.reserve(thread_count);

  for (std::size_t i = 0; i < thread_count; ++i) {
    mWorkers.emplace_back([this] {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(mMutex);
          mCondition.wait(lock,
                          // shutting down or task available
                          [this] { return !mRunning || !mTasks.empty(); });

          if (!mRunning && mTasks.empty())
            return;

          task = std::move(mTasks.front());
          mTasks.pop();
        }
        task();
      }
    });
  }
}

void Threadpool::join() {
  {
    std::unique_lock<std::mutex> lock(mMutex);
    mRunning = false;
  }
  mCondition.notify_all();
  mWorkers.clear(); // joins the threads
}

Threadpool pool{};
