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

#ifndef STA_THREAD_POOL_H
#define STA_THREAD_POOL_H

#include "DisallowCopyAssign.hh"
#include "Vector.hh"
#include "Mutex.hh"

namespace sta {

class ThreadWorker;

class ThreadPool
{
public:
  ThreadPool() {}
  ~ThreadPool();
  // Create a new working thread if the pool is empty.
  ThreadWorker *pop();
  void push(ThreadWorker *thread);

private:
  DISALLOW_COPY_AND_ASSIGN(ThreadPool);

  Vector<ThreadWorker*> threads_;
  Mutex lock_;
};

} // namespace
#endif
