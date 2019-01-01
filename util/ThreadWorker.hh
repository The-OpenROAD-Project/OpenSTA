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

#ifndef STA_THREAD_WORKER_H
#define STA_THREAD_WORKER_H

#include "Pthread.hh"
#include "DisallowCopyAssign.hh"
#include "Mutex.hh"
#include "Condition.hh"

namespace sta {

typedef void (*ThreadFunc)(void*);

class ThreadWorker
{
public:
  ThreadWorker();
  ~ThreadWorker();
  // beginTask() caller must call wait(), or deadlock will happen.
  void beginTask(ThreadFunc func,
		 void *arg);
  void wait();

private:
  DISALLOW_COPY_AND_ASSIGN(ThreadWorker);

  static pthread_attr_t *defaultAttr();
  static void *threadBegin(void *arg);

  typedef enum {
    state_ready = 0,
    state_run,
    state_done,
    state_stop
  } TaskState;

  pthread_t thread_;
  Mutex lock_;
  Condition condition_;
  TaskState state_;
  ThreadFunc func_;
  void *arg_;

  static bool default_attr_inited_;
  static pthread_attr_t default_attr_;
};

} // namespace
#endif
