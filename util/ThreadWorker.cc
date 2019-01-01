// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <cstdio>
#include "Machine.hh"
#include "Error.hh"
#include "Error.hh"
#include "ThreadException.hh"
#include "ThreadWorker.hh"

namespace sta {

bool ThreadWorker::default_attr_inited_ = false;
pthread_attr_t ThreadWorker::default_attr_;

ThreadWorker::ThreadWorker() :
  state_(state_ready),
  func_(NULL),
  arg_(NULL)
{
  int error = pthread_create(&thread_, defaultAttr(), threadBegin, this);
  CheckThreadError(error);
}

ThreadWorker::~ThreadWorker()
{
  // Stop the pthread function.
  lock_.lock();
  state_ = state_stop;
  condition_.signal();
  lock_.unlock();
  pthread_join(thread_, NULL);
}

pthread_attr_t *
ThreadWorker::defaultAttr()
{
  if (!default_attr_inited_) {
    int error;

    error = pthread_attr_init(&default_attr_);
    CheckThreadError(error);

    // Make sure the thread contends for CPU time with threads
    // in other processes.
    error = pthread_attr_setscope(&default_attr_, STA_PTHREAD_SCOPE_SYSTEM);
    CheckThreadError(error);

    // Set stack size to 1MB.
//    error = pthread_attr_setstacksize(&default_attr_, (1 << 20));
    CheckThreadError(error);

    default_attr_inited_ = true;
  }
  return &default_attr_;
}

void
ThreadWorker::beginTask(ThreadFunc func,
			void *arg)
{
  lock_.lock();
  while (state_ != state_ready)
    condition_.wait(lock_);

  func_ = func;
  arg_ = arg;
  state_ = state_run;
  // Use broadcast instead of signal as start(), wait(), threadStart()
  // may all be in different threads.
  condition_.broadcast();
  lock_.unlock();
}

void
ThreadWorker::wait()
{
  lock_.lock();
  while (state_ != state_ready)
    condition_.wait(lock_);
  lock_.unlock();
}

// Evaluate tasks as they are assigned by and then wait for another
// task.
// This function is stopped by the ThreadWorker destructor.
void *
ThreadWorker::threadBegin(void *arg)
{
  ThreadWorker *worker = reinterpret_cast<ThreadWorker*>(arg);
  while (true) {
    worker->lock_.lock();
    while (worker->state_ != state_run &&
           worker->state_ != state_stop)
      worker->condition_.wait(worker->lock_);

    if (worker->state_ == state_stop) {
      // Stop the thread.
      worker->lock_.unlock();
      break;
    }

    try {
      (*worker->func_)(worker->arg_);
    }
    // Keep the thread worker alive even after an exception.
    catch (StaException &except) {
      printf("Caught %s exception.", except.what());
    }
    catch (...) {
      printf("Caught ... exception.");
    }

    worker->state_ = state_ready;
    worker->condition_.broadcast();
    worker->lock_.unlock();
  }
  return NULL;
}

} // namespace
