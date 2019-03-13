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

#ifndef STA_THREAD_FOR_EACH_H
#define STA_THREAD_FOR_EACH_H

#include <mutex>
#include <thread>
#include <vector>
#include "Iterator.hh"

namespace sta {

template<class Iterator, class Func>
class ForEachArg {
public:
  ForEachArg(Iterator *iter,
	     std::mutex &lock,
	     Func *func) :
    iter_(iter),
    lock_(lock),
    func_(func)
  {}

  ~ForEachArg()
  {
    delete func_;
  }

  // Copy constructor.
  ForEachArg(const ForEachArg &arg) :
    iter_(arg.iter_),
    lock_(arg.lock_),
    func_(arg.func_->copy())
  {
  }
  // Move constructor.
  ForEachArg(ForEachArg &&arg) :
    iter_(arg.iter_),
    lock_(arg.lock_),
    func_(arg.func_)
  {
    arg.func_ = nullptr;
  }

  Iterator *iter_;
  std::mutex &lock_;
  Func *func_;
};

template<class Iterator, class Func, class FuncArg>
void
forEachBegin(ForEachArg<Iterator, Func> arg1)
{
  Iterator *iter = arg1.iter_;
  std::mutex &lock = arg1.lock_;
  Func *func = arg1.func_;
  while (true) {
    lock.lock();
    if (iter->hasNext()) {
      FuncArg arg = iter->next();
      lock.unlock();
      (*func)(arg);
    }
    else {
      lock.unlock();
      break;
    }
  }
}

// Parallel version of STL for_each.
// Each thread has its own functor.
// Func::copy() must be defined.
template<class Iterator, class Func, class FuncArg>
void
forEach(Iterator *iter,
	Func *func,
	int thread_count)
{
  if (thread_count <= 1) {
    while (iter->hasNext())
      (*func)(iter->next());
  }
  else {
    std::vector<std::thread> threads;
    std::mutex lock;
    for (int i = 0; i < thread_count; i++) {
      ForEachArg<Iterator,Func> arg(iter, lock, func->copy());
      threads.push_back(std::thread(forEachBegin<Iterator,Func,FuncArg>, arg));
    }

    for (auto &thread : threads)
      thread.join();
  }
}

} // namespace
#endif
