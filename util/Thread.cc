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
#include "ThreadException.hh"
#include "ThreadWorker.hh"
#include "Thread.hh"

namespace sta {

ThreadPool Thread::pool_;

Thread::Thread() :
  worker_(NULL)
{
}

void
Thread::beginTask(ThreadFunc func,
		  void *arg)
{
  worker_ = pool_.pop();
  worker_->beginTask(func, arg);
}

void
Thread::wait()
{
  worker_->wait();
#if 0
  delete worker_;
#else
  pool_.push(worker_);
#endif
  worker_ = NULL;
}

} // namespace
