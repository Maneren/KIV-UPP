#pragma once

#include <algorithm>
#include <functional>
#include <future>
#include <queue>
#include <ranges>
#include <thread>
#include <vector>

/**
 * @class Threadpool
 * @brief Manage and execute tasks concurrently using a thread pool.
 *
 * This class provides a thread pool that allows for spawning tasks with or
 * without futures (return values) or various forms of parallel ranges
 * processing.
 *
 * @note The thread pool size is by default determined by the available hardware
 * concurrency.
 */
class Threadpool {
public:
  /**
   * @brief Construct a new Threadpool object.
   */
  Threadpool() : Threadpool(std::thread::hardware_concurrency()) {};

  /**
   * @brief Construct a new Threadpool object with a custom worker thread count.
   * @param threadCount The number of worker threads to create.
   */
  Threadpool(size_t thread_count);

  /**
   * @brief Destroy the Threadpool object, joining all worker threads.
   */
  ~Threadpool() { join(); }

  /**
   * @brief Delete copy constructor.
   */
  Threadpool(const Threadpool &) = delete;

  /**
   * @brief Delete copy assignment operator.
   */
  Threadpool &operator=(const Threadpool &) = delete;

  /**
   * @brief Move construct a new Threadpool object.
   * @param other The other Threadpool to move from.
   */
  Threadpool(Threadpool &&other) {
    mWorkers = std::move(other.mWorkers);
    mTasks = std::move(other.mTasks);
    mRunning = std::move(other.mRunning);
  }

  /**
   * @brief Move assign a Threadpool object.
   *
   * @param other The other Threadpool to move from.
   *
   * @return Threadpool& Reference to this Threadpool.
   */
  Threadpool &operator=(Threadpool &&other) {
    mWorkers = std::move(other.mWorkers);
    mTasks = std::move(other.mTasks);
    mRunning = std::move(other.mRunning);
    return *this;
  }

  /**
   * @brief Spawn a task and return a future to retrieve the result.
   *
   * @tparam F The type of the function to execute.
   * @tparam Args The types of the arguments to pass to the function.
   * @tparam U The return type of the function.
   *
   * @param f The function to execute.
   * @param args The arguments to pass to the function.
   *
   * @return std::future<U> A future to retrieve the result of the task.
   */
  template <class F, class... Args,
            typename U = std::invoke_result_t<F, Args...>>
  inline std::future<U> spawn_with_future(F &&f, Args &&...args) {
    auto task = std::make_shared<std::packaged_task<U()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto res = task->get_future();
    spawn([task]() { (*task)(); });
    return res;
  }

  /**
   * @brief Spawn a task.
   *
   * @param task The function to execute.
   */
  inline void spawn(std::function<void()> task) {
    {
      std::unique_lock<std::mutex> lock(mMutex);
      mTasks.push(task);
    }
    mCondition.notify_one();
  }

  /**
   * @brief Spawn a task.
   *
   * @tparam U The return type of the function.
   * @tparam F The type of the function to execute.
   * @tparam Args The types of the arguments to pass to the function.
   *
   * @param f The function to execute.
   * @param args The arguments to pass to the function.
   */
  template <typename U, typename F, typename... Args>
  inline void spawn(F &&f, Args &&...args) {
    spawn(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
  }

  /**
   * @brief Gracefully stop the thread pool and join all worker threads
   * (automatically called in destructor).
   */
  void join();

  /**
   * @brief Transform a range of data using a functor and return a vector of
   * futures.
   *
   * @tparam R The type of the range.
   * @tparam T The type of the elements in the range.
   * @tparam F The type of the functor.
   * @tparam U The return type of the functor.
   *
   * @param range The range of data to transform.
   * @param functor The functor to apply to each element of the range.
   *
   * @return std::vector<std::future<U>> A vector of futures to retrieve the
   * results of the transformations.
   */
  template <std::ranges::forward_range R,
            typename T = std::ranges::range_value_t<R>, typename F,
            typename U = std::invoke_result_t<F, T &>>
  inline std::vector<std::future<U>> transform(const R &range, F functor) {
    return range | std::views::transform([this, &functor](auto item) {
             return spawn_with_future(functor, item);
           }) |
           std::ranges::to<std::vector>();
  }

  /**
   * @brief Apply a functor to each element of a range.
   *
   * @tparam R The type of the range.
   * @tparam T The type of the elements in the range.
   *
   * @param range The range of data to process.
   * @param functor The functor to apply to each element of the range.
   */
  template <std::ranges::forward_range R,
            typename T = std::ranges::range_value_t<R>, typename F,
            typename U = std::invoke_result_t<F, T &>>
  inline void for_each(const R &range, F functor) {
    static_assert(std::is_same<U, void>::value);

    for (auto &future : transform(range, functor)) {
      future.get();
    }
  }

private:
  std::vector<std::jthread> mWorkers;
  std::queue<std::function<void()>> mTasks;
  std::mutex mMutex;
  std::condition_variable mCondition;
  bool mRunning = true;
};

extern Threadpool pool;
