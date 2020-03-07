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

#include "Machine.hh"
#include "ThreadWorker.hh"
#include "ThreadPool.hh"

namespace sta {

ThreadPool::~ThreadPool()
{
  threads_.deleteContentsClear();
}

ThreadWorker *
ThreadPool::pop()
{
  lock_.lock();
  if (!threads_.empty()) {
    ThreadWorker *thread = threads_.back();
    threads_.pop_back();
    lock_.unlock();
    return thread;
  }
  lock_.unlock();
  // No threads in the pool, get a new one.
  return new ThreadWorker;
}

void
ThreadPool::push(ThreadWorker *thread)
{
  lock_.lock();
  threads_.push_back(thread);
  lock_.unlock();
}

} // namespace
