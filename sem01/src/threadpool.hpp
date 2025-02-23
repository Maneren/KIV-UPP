#pragma once

#include <functional>
#include <future>
#include <queue>
#include <ranges>
#include <thread>
#include <utility>
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
   * @param thread_count The number of worker threads to create. Defaults to the
   * number of CPU threads.
   */
  Threadpool(size_t thread_count = std::thread::hardware_concurrency());

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
   * @tparam Functor The type of the function to execute.
   * @tparam Args The types of the arguments to pass to the function.
   * @tparam Result The return type of the function.
   *
   * @param f The function to execute.
   * @param args The arguments to pass to the function.
   *
   * @return std::future<Result> A future to retrieve the result of the task.
   */
  template <typename Functor, typename... Args,
            typename Result = std::invoke_result_t<Functor, Args...>>
  inline std::future<Result> spawn_with_future(Functor &&f, Args &&...args) {
    auto task = std::make_shared<std::packaged_task<Result()>>(
        [f = std::forward<Functor>(f), &args...] {
          return f(std::forward<Args>(args)...);
        });
    spawn([task]() { (*task)(); });
    return task->get_future();
  }

  /**
   * @brief Spawn a task.
   *
   * @param task The function to execute.
   */
  inline void spawn(std::function<void()> &&task) {
    {
      std::unique_lock<std::mutex> lock(mMutex);
      mTasks.emplace(std::move(task));
    }
    mCondition.notify_one();
  }

  /**
   * @brief Spawn a task.
   *
   * @tparam F The type of the function to execute.
   * @tparam Args The types of the arguments to pass to the function.
   * @tparam U The return type of the function.
   *
   * @param f The function to execute.
   * @param args The arguments to pass to the function.
   */
  template <typename F, typename... Args>
  inline void spawn(F &&f, Args &&...args) {
    static_assert(std::is_same_v<std::invoke_result_t<F, Args...>, void>,
                  "The function must not return any value.");
    spawn(static_cast<std::function<void()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)));
  }

  /**
   * @brief Gracefully stop the thread pool and join all worker threads
   * (automatically called by the destructor).
   */
  void join();

  /**
   * @brief Transform a range of data using a functor and return a vector of
   * futures.
   *
   * @tparam Range The type of the range.
   * @tparam Item The type of the elements in the range.
   * @tparam Functor The type of the functor.
   * @tparam Result The return type of the functor.
   *
   * @param range The range of data to transform.
   * @param functor The functor to apply to each element of the range.
   *
   * @return std::vector<std::future<Result>> A vector of futures to retrieve
   * the results of the transformations.
   */
  template <std::ranges::input_range Range,
            typename Item = std::ranges::range_value_t<Range>, typename Functor,
            typename Result = std::invoke_result_t<Functor, Item &>>
  inline std::vector<std::future<Result>> transform(Range &range,
                                                    Functor &&functor) {
    auto f = [this, &functor](auto &item) {
      return spawn_with_future(std::forward<Functor>(functor), item);
    };
    return range | std::views::transform(f) | std::ranges::to<std::vector>();
  }

  /**
   * @brief Transform a range of data using a functor and return a vector of
   * futures.
   *
   * @tparam Range The type of the range.
   * @tparam Item The type of the elements in the range.
   * @tparam Functor The type of the functor.
   * @tparam Result The return type of the functor.
   *
   * @param range The range of data to transform.
   * @param functor The functor to apply to each element of the range.
   *
   * @return std::vector<std::future<Result>> A vector of futures to retrieve
   * the results of the transformations.
   */
  template <std::ranges::input_range Range,
            typename Item = std::ranges::range_value_t<Range>, typename Functor,
            typename Result = std::invoke_result_t<Functor, Item>>
  inline std::vector<std::future<Result>> transform(Range &&range,
                                                    Functor &&functor) {
    auto f = [this, &functor](auto &&item) {
      return spawn_with_future(std::bind(std::forward<Functor>(functor), item));
    };
    return range | std::views::transform(f) | std::ranges::to<std::vector>();
  }

  /**
   * @brief Apply a functor to each element of a range.
   *
   * @tparam Range The type of the range.
   * @tparam Item The type of the elements in the range.
   * @tparam Functor The type of the functor.
   * @tparam Result The return type of the functor (should be void).
   *
   * @param range The range of data to process.
   * @param functor The functor to apply to each element of the range.
   */
  template <std::ranges::input_range Range,
            typename Item = std::ranges::range_value_t<Range>, typename Functor,
            typename Result = std::invoke_result_t<Functor, Item &>>
  inline void for_each(Range &&range, Functor &&functor) {
    static_assert(std::is_same_v<Result, void>,
                  "for_each functor should return void");

    for (auto &future : transform(std::forward<Range>(range),
                                  std::forward<Functor>(functor))) {
      future.wait();
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
