// Author Phillip Johnston
// Licensed under CC0 1.0 Universal
// https://github.com/embeddedartistry/embedded-resources/blob/master/examples/cpp/dispatch.cpp
// https://embeddedartistry.com/blog/2017/2/1/dispatch-queues?rq=dispatch

#pragma once

#include <thread>
#include <functional>
#include <vector>
#include <cstdint>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace sta {

class DispatchQueue
{
  typedef std::function<void(int thread)> fp_t;

public:
  DispatchQueue(size_t thread_cnt);
  ~DispatchQueue();
  void setThreadCount(size_t thread_count);
  // Dispatch and copy.
  void dispatch(const fp_t& op) { q_.push_back(op); }
  // Dispatch and move.
  void dispatch(fp_t&& op) { q_.push_back(std::move(op)); }
  void runTasks();

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
  std::vector<bool> pending_;
  std::vector<fp_t> q_;
  std::condition_variable cv_;
  size_t pending_count_ = 0;
  bool quit_ = false;
};

} // namespace
