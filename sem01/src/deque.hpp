#include <array>
#include <atomic>
#include <cstddef>
#include <print>

#if defined(__x86_64__) || defined(__i386__)
#include <emmintrin.h>
static inline void spin_loop_pause() noexcept { _mm_pause(); }
#else
static inline void spin_loop_pause() noexcept { std::this_thread::yield(); }
#endif

/**
 * @class Deque
 * @brief Threadsafe lock-free deque
 *
 */
template <typename T, const std::size_t N> class Deque {
  enum State { EMPTY, LOADING, STORING, STORED };

public:
  Deque() {}

  ~Deque() = default;

  Deque(const Deque &) = delete;
  Deque &operator=(const Deque &) = delete;

  Deque(Deque &&other) {
    mBuffer = std::move(other.mBuffer);
    mHead = other.mHead.exchange(0);
    mTail = other.mTail.exchange(0);
  };

  Deque &operator=(Deque &&other) {
    mBuffer = std::move(other.mBuffer);
    mHead = other.mHead.exchange(0);
    mTail = other.mTail.exchange(0);
    return *this;
  };

  // void push_back(const T &value) {
  //   auto tail = mTail.fetch_add(1, std::memory_order_relaxed);
  //
  //   auto &q_element = mBuffer[tail % N];
  //   auto &state = mState[tail % N];
  //
  //   for (;;) {
  //     auto expected = EMPTY;
  //     if (state.compare_exchange_weak(expected, STORING,
  //                                     std::memory_order_acquire,
  //                                     std::memory_order_relaxed)) {
  //       q_element = value;
  //       state.store(STORED, std::memory_order_release);
  //       return;
  //     }
  //     do
  //       std::this_thread::yield();
  //     while (state.load(std::memory_order_relaxed) != EMPTY);
  //   }
  // }

  bool emplace(T &&value) {
    auto head = mHead.fetch_add(1, std::memory_order_relaxed);
    // std::println("emplace {}", head);

    auto &q_element = mBuffer[head % N];
    auto &state = mState[head % N];

    for (;;) {
      if (!open()) {
        // std::println("Deque is closed");
        return false;
      }

      auto expected = EMPTY;
      if (state.compare_exchange_weak(expected, STORING,
                                      std::memory_order_acquire,
                                      std::memory_order_relaxed)) {
        // std::println("emplaced");
        q_element = std::forward<T>(value);
        state.store(STORED, std::memory_order_release);
        return true;
      }

      do {
        if (!open())
          return false;

        // std::println("Spinning emplace: {}",
        // static_cast<int>(state.load(std::memory_order_relaxed)));
        spin_loop_pause();
      } while (state.load(std::memory_order_relaxed) != expected);
    }
  }

  std::optional<T> try_pop() {
    auto tail = mTail.fetch_add(1, std::memory_order_relaxed);
    // std::println("try_pop {}", tail);

    auto &q_element = mBuffer[tail % N];
    auto &state = mState[tail % N];

    for (;;) {
      auto expected = STORED;
      if (state.compare_exchange_weak(expected, LOADING,
                                      std::memory_order_acquire,
                                      std::memory_order_relaxed)) {
        T element{std::move(q_element)};
        state.store(EMPTY, std::memory_order_release);
        // std::println("popped");
        return element;
      }

      do {
        if (!open())
          return std::nullopt;

        // std::println("Spinning pop: {} != {}",
        //              static_cast<int>(state.load(std::memory_order_relaxed)),
        //              static_cast<int>(STORED));
        spin_loop_pause();
      } while (state.load(std::memory_order_relaxed) != STORED);
    }
  }

  bool empty() const {
    return mHead.load(std::memory_order_relaxed) <=
           mTail.load(std::memory_order_relaxed);
  }

  bool open() const { return mOpen.load(std::memory_order_relaxed); }

  void close() { mOpen.store(false, std::memory_order_relaxed); }

private:
  std::array<T, N> mBuffer;
  std::array<std::atomic<State>, N> mState;
  std::atomic<std::size_t> mHead;
  std::atomic<std::size_t> mTail;

  std::atomic<bool> mOpen{true};
};
