// Author Phillip Johnston
// Licensed under CC0 1.0 Universal
// https://github.com/embeddedartistry/embedded-resources/blob/master/examples/cpp/dispatch.cpp
// https://embeddedartistry.com/blog/2017/2/1/dispatch-queues?rq=dispatch

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace sta {

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
  // Monotonic total of dispatch() calls since construction. Exposed so
  // the Kahn's BFS regression can measure concurrency overhead via
  // dispatch-count deltas (Kahn's dispatches per ready-transition vs
  // the original BFS's per level-chunk).
  uint64_t dispatchCallCount() const;

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
  std::atomic<size_t> pending_task_count_;
  bool quit_ = false;
  std::atomic<uint64_t> dispatch_call_count_{0};
};

} // namespace sta
