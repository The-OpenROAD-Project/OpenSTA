// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
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

#ifndef STA_THREAD_H
#define STA_THREAD_H

#include "DisallowCopyAssign.hh"
#include "ThreadWorker.hh"
#include "ThreadPool.hh"

namespace sta {

// Basic thread class implemented using a pool of threads,
// i.e., the threads are put to sleep when their task is done
// and they are re-cycled for the next task.
class Thread
{
public:
  Thread();
  // After calling start(), the caller must call wait() before
  // destroying this object.
  void beginTask(ThreadFunc func,
		 void *arg);
  void wait();

private:
  DISALLOW_COPY_AND_ASSIGN(Thread);

  ThreadWorker *worker_;

  static ThreadPool pool_;
};

} // namespace
#endif
