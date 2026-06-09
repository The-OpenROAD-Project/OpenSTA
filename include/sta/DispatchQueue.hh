// Original Author: Phillip Johnston
// Licensed under CC0 1.0 Universal
// Original source: https://github.com/embeddedartistry/embedded-resources/blob/master/examples/cpp/dispatch.cpp
// Original article: https://embeddedartistry.com/blog/2017/2/1/dispatch-queues?rq=dispatch
//
// Modified for OpenSTA to use C++20 non-spinning DynamicLatch for synchronization.

#pragma once

#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace sta {

class DynamicLatch
{
public:
  explicit DynamicLatch(std::ptrdiff_t initial_count = 0) :
    count_(initial_count)
  {
  }

  // Delete copy/move constructors to prevent accidental slicing/copying of atomics
  DynamicLatch(const DynamicLatch&) = delete;
  DynamicLatch& operator=(const DynamicLatch&) = delete;

  // Increases the latch count (used when a new task is dispatched)
  void
  countUp()
  {
    count_.fetch_add(1, std::memory_order_release);
  }

  // Decreases the latch count and wakes waiting threads if it hits zero
  void
  countDown(std::ptrdiff_t n = 1)
  {
    if (count_.fetch_sub(n, std::memory_order_release) == n) {
      count_.notify_all();
    }
  }

  // Blocks until the count reaches zero
  void
  wait() const
  {
    std::ptrdiff_t current = count_.load(std::memory_order_acquire);
    while (current != 0) {
      count_.wait(current, std::memory_order_acquire);
      current = count_.load(std::memory_order_acquire);
    }
  }

private:
  mutable std::atomic<std::ptrdiff_t> count_{0};
};

class DispatchQueue
{
  using fp_t = std::function<void(int thread)>;

public:
  DispatchQueue(size_t thread_count);
  ~DispatchQueue();
  void setThreadCount(size_t thread_count);
  size_t getThreadCount() const;
  // Dispatch and copy.
  void dispatch(const fp_t& op);
  // Dispatch and move.
  void dispatch(fp_t&& op);
  void finishTasks();

  // Deleted operations
  DispatchQueue(const DispatchQueue& rhs) = delete;
  DispatchQueue& operator=(const DispatchQueue& rhs) = delete;
  DispatchQueue(DispatchQueue&& rhs) = delete;
  DispatchQueue& operator=(DispatchQueue&& rhs) = delete;

private:
  void dispatch_thread_handler(size_t i);
  void terminateThreads();

  std::mutex lock_;
  std::vector<std::thread> threads_;
  std::queue<fp_t> q_;
  std::condition_variable cv_;
  DynamicLatch pending_task_count_latch_;
  bool quit_ = false;
};

} // namespace sta
