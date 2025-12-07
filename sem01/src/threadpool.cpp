#include "threadpool.hpp"
#include <mutex>

namespace threadpool {

Threadpool::Threadpool(size_t thread_count) {
  mWorkers.reserve(thread_count);

  for (std::size_t i = 0; i < thread_count; ++i) {
    mWorkers.emplace_back([this] {
      std::function<void()> task;
      while (true) {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondition.wait(lock,
                        // shutting down or task available
                        [&] { return !mRunning || !mTasks.empty(); });

        if (!mRunning && mTasks.empty())
          return;

        task = std::move(mTasks.front());
        mTasks.pop();

        lock.unlock();

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

} // namespace threadpool
