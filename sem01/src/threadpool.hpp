#pragma once

#include <functional>
#include <future>
#include <queue>
#include <ranges>
#include <thread>
#include <utility>
#include <vector>

namespace threadpool {

namespace detail {

bool try_run_task(std::unique_lock<std::mutex> &&lock,
                  std::queue<std::function<void()>> &tasks);

} // namespace detail

template <typename R> class future;

/**
 * @class Threadpool
 * @brief A thread pool for managing and executing tasks concurrently.
 *
 * This class provides functionality to spawn tasks with or without return
 * values (futures) and supports parallel processing of ranges. It simplifies
 * concurrent task execution by managing a pool of worker threads.
 *
 * @note By default, the thread pool size is determined by the number of
 * hardware threads available on the system.
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
   * @brief Destroy the Threadpool object and joins all worker threads.
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

  using Task = std::function<void()>;

  /**
   * @brief Spawn a task and returns a future to retrieve its result.
   *
   * @tparam Functor The type of the callable object to execute.
   * @tparam Args The types of the arguments to pass to the callable object.
   * @tparam Result The return type of the callable object.
   *
   * @param f The callable object to execute.
   * @param args The arguments to pass to the callable object.
   *
   * @return std::future<Result> A future that can be used to retrieve the
   * result of the task once it completes.
   */
  template <typename Functor, typename... Args,
            typename Result = std::invoke_result_t<Functor, Args...>>
  inline std::future<Result> spawn_with_future(Functor &&f, Args &&...args) {
    auto task = std::make_shared<std::packaged_task<Result()>>(
        [f = std::forward<Functor>(f), &args...] {
          return f(std::forward<Args>(args)...);
        });
    spawn([task]() { (*task)(); });
    return future(task->get_future(), *this);
  }

  /**
   * @brief Spawn a task to be executed by the thread pool.
   *
   * @param task A callable object representing the task to execute.
   */
  inline void spawn(Task &&task) {
    {
      std::unique_lock<std::mutex> lock(mMutex);
      mTasks.emplace(std::move(task));
    }
    mCondition.notify_one();
  }

  /**
   * @brief Spawn a background task inside the thread pool.
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
    // This may be better handled by a lambda-capture, to prevent unneccessary
    // binder objects, but I can't get it to work with the forwarding. It has to
    // be able to pass args both as references and as values, but I only managed
    // to get it to work as values.
    spawn(static_cast<Task>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)));
  }

  /**
   * @brief Gracefully stop the thread pool and joins all worker threads.
   *
   * This method ensures that all tasks are completed before the thread pool
   * shuts down. It is automatically called by the destructor.
   */
  void join();

  /**
   * @brief Transform a range into a vector of futures by applying a functor
   * to each element in parallel.
   *
   * @tparam Range The type of the input range.
   * @tparam Item The type of the elements in the range.
   * @tparam Functor The type of the callable object to apply.
   * @tparam Result The return type of the callable object.
   *
   * @param range The range of elements to transform.
   * @param functor The callable object to apply to each element of the range.
   *
   * @return std::vector<std::future<Result>> A vector of futures representing
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
   * @brief Transform a range into a vector of futures by applying a functor
   * to each element in parallel.
   *
   * @tparam Range The type of the input range.
   * @tparam Item The type of the elements in the range.
   * @tparam Functor The type of the callable object to apply.
   * @tparam Result The return type of the callable object.
   *
   * @param range The range of elements to transform.
   * @param functor The callable object to apply to each element of the range.
   *
   * @return std::vector<std::future<Result>> A vector of futures representing
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
   * @brief Apply a callable object to each element of a range in parallel.
   *
   * @tparam Range The type of the input range.
   * @tparam Item The type of the elements in the range.
   * @tparam Functor The type of the callable object to apply.
   * @tparam Result The return type of the callable object (must be void).
   *
   * @param range The range of elements to process.
   * @param functor The callable object to apply to each element of the range.
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

  size_t size() const { return mWorkers.size(); }

private:
  std::vector<std::jthread> mWorkers;
  std::queue<Task> mTasks;
  std::mutex mMutex;
  std::condition_variable mCondition;
  bool mRunning = true;

  template <typename R> friend class future;
  friend bool detail::try_run_task(std::unique_lock<std::mutex> &&lock,
                                   std::queue<Task> &tasks);
};

template <typename R> class future : public std::future<R> {
public:
  future(std::future<R> &&future, Threadpool &pool)
      : std::future<R>(std::move(future)), mPool(pool) {}

  R get() {
    Threadpool::Task task;
    while (!this->valid()) {
      std::unique_lock<std::mutex> lock(mPool.mMutex);
      if (mPool.mTasks.empty()) {
        lock.unlock();
        std::this_thread::yield();
        return this->get();
      }

      task = std::move(mPool.mTasks.front());
      mPool.mTasks.pop();

      lock.unlock();
      task();
    }
  }

private:
  Threadpool &mPool;
};

/**
 * @brief A global thread pool instance initialized at program startup.
 */
extern Threadpool pool;

} // namespace threadpool
