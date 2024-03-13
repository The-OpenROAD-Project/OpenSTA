// Author Phillip Johnston
// Licensed under CC0 1.0 Universal
// https://github.com/embeddedartistry/embedded-resources/blob/master/examples/cpp/dispatch.cpp
// https://embeddedartistry.com/blog/2017/2/1/dispatch-queues?rq=dispatch

#include <stdio.h>
#include "DispatchQueue.hh"

namespace sta {

DispatchQueue::DispatchQueue(size_t thread_count) :
  threads_(thread_count)
{
  std::unique_lock<std::mutex> lock(lock_);
  pending_.resize(thread_count);
  for (size_t i = 0; i < thread_count; i++)
    threads_[i] = std::thread(&DispatchQueue::dispatch_thread_handler, this, i);
}

DispatchQueue::~DispatchQueue()
{
  terminateThreads();
}

void
DispatchQueue::terminateThreads()
{
  // Signal to dispatch threads that it's time to wrap up
  {
    std::unique_lock<std::mutex> lock(lock_);
    quit_ = true;
  }
  cv_.notify_all();

  // Wait for threads to finish before we exit
  for(size_t i = 0; i < threads_.size(); i++) {
    if (threads_[i].joinable()) {
      threads_[i].join();
    }
  }
}

void
DispatchQueue::setThreadCount(size_t thread_count)
{
  terminateThreads();

  threads_.resize(thread_count);
  pending_.resize(thread_count);
  for(size_t i = 0; i < thread_count; i++) {
    threads_[i] = std::thread(&DispatchQueue::dispatch_thread_handler, this, i);
  }
}

void
DispatchQueue::runTasks()
{
  pending_count_ = threads_.size();
  {
    std::unique_lock<std::mutex> lock(lock_);
    pending_.clear();
    pending_.resize(threads_.size(), true);
  }
  cv_.notify_all();
  {
      std::unique_lock<std::mutex> lock(lock_);
      cv_.wait(lock, [this] { return pending_count_ == 0; } );
  }
  q_.clear();
}

void
DispatchQueue::dispatch_thread_handler(size_t thread_id)
{
  do {
    {
      std::unique_lock<std::mutex> lock(lock_);
      // Wait until we have data or a quit signal
      cv_.wait(lock, [this, thread_id] { return pending_[thread_id] || quit_; });
    }

    if(!quit_) {
      size_t thread_chunk = q_.size() / threads_.size();
      size_t begin = thread_chunk * thread_id;
      size_t end = std::min(begin + thread_chunk, q_.size());

      for (size_t i = begin; i < end; i++) {
        q_[i](thread_id);
      }

      {
        std::unique_lock<std::mutex> lock(lock_);
        pending_[thread_id] = false;
        pending_count_--;
      }
      cv_.notify_all();
    }
  } while (!quit_);
}

} // namespace
