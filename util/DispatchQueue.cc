// Author Phillip Johnston
// Licensed under CC0 1.0 Universal
// https://github.com/embeddedartistry/embedded-resources/blob/master/examples/cpp/dispatch.cpp
// https://embeddedartistry.com/blog/2017/2/1/dispatch-queues?rq=dispatch

#include "DispatchQueue.hh"

namespace sta {

DispatchQueue::DispatchQueue(size_t thread_count) :
  threads_(thread_count),
  pending_task_count_(0)
{
  for(size_t i = 0; i < thread_count; i++)
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
  std::unique_lock<std::mutex> lock(lock_);
  quit_ = true;
  lock.unlock();
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
  for(size_t i = 0; i < thread_count; i++) {
    threads_[i] = std::thread(&DispatchQueue::dispatch_thread_handler, this, i);
  }
}

void
DispatchQueue::finishTasks()
{
  while (pending_task_count_.load(std::memory_order_acquire) != 0)
    std::this_thread::yield();
}

void
DispatchQueue::dispatch(const fp_t& op)
{
  std::unique_lock<std::mutex> lock(lock_);
  q_.push(op);
  pending_task_count_++;

  // Manual unlocking is done before notifying, to avoid waking up
  // the waiting thread only to block again (see notify_one for details)
  lock.unlock();
  cv_.notify_all();
}

void
DispatchQueue::dispatch(fp_t&& op)
{
  std::unique_lock<std::mutex> lock(lock_);
  q_.push(std::move(op));
  pending_task_count_++;

  // Manual unlocking is done before notifying, to avoid waking up
  // the waiting thread only to block again (see notify_one for details)
  lock.unlock();
  cv_.notify_all();
}

void
DispatchQueue::dispatch_thread_handler(size_t i)
{
  std::unique_lock<std::mutex> lock(lock_);

  do {
    // Wait until we have data or a quit signal
    cv_.wait(lock, [this] { return (q_.size() || quit_); } );

    //after wait, we own the lock
    if(!quit_ && q_.size()) {
      auto op = std::move(q_.front());
      q_.pop();

      lock.unlock();

      op(i);

      pending_task_count_--;
      lock.lock();
    }
  } while (!quit_);
}

} // namespace
