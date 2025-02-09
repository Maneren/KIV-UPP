#include "threadpool.hpp"

Threadpool::Threadpool(size_t thread_count) {
  mTasks = std::queue<std::function<void()>>{};

  mWorkers.reserve(thread_count);

  for (std::size_t i = 0; i < thread_count; ++i) {
    mWorkers.emplace_back([this] {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(mMutex);
          mCondition.wait(lock,
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

// inline void Threadpool::spawn(std::function<void()> task)

void Threadpool::join() {
  {
    std::unique_lock<std::mutex> lock(mMutex);
    mRunning = false;
  }
  mCondition.notify_all();
  mWorkers.clear();
}
