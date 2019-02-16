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

#include "Iterator.hh"
#include "Thread.hh"
#include "Mutex.hh"

namespace sta {

template<class Iterator, class Func>
class ForEachArg {
public:
  ForEachArg() {}
  ForEachArg(Iterator *iter,
	     Mutex *lock,
	     Func *func) :
    iter_(iter),
    lock_(lock),
    func_(func)
  {}

  Iterator *iter_;
  Mutex *lock_;
  Func *func_;
};

template<class Iterator, class Func, class FuncArg>
void
forEachBegin(void *arg)
{
  ForEachArg<Iterator, Func> *arg1 =
    reinterpret_cast<ForEachArg<Iterator, Func>*>(arg);
  Iterator *iter = arg1->iter_;
  Mutex *lock = arg1->lock_;
  Func *func = arg1->func_;
  while (true) {
    lock->lock();
    if (iter->hasNext()) {
      FuncArg arg = iter->next();
      lock->unlock();
      (*func)(arg);
    }
    else {
      lock->unlock();
      break;
    }
  }
}

// Parallel version of STL for_each.
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
    Mutex lock;
    ForEachArg<Iterator, Func> arg(iter, &lock, func);
    Thread *threads = new Thread[thread_count];
    for (int i = 0; i < thread_count; i++)
      threads[i].beginTask(forEachBegin<Iterator, Func, FuncArg>,
			   reinterpret_cast<void*>(&arg));

    for (int i = 0; i < thread_count; i++)
      threads[i].wait();

    delete [] threads;
  }
}

// forEach2 is similar to forEach, except that each thread has
// its own functor.
// Func::copy() must be defined.
template<class Iterator, class Func, class FuncArg>
void
forEach2(Iterator *iter,
	 Func *func,
	 int thread_count)
{
  if (thread_count <= 1) {
    while (iter->hasNext())
      (*func)(iter->next());
  }
  else {
    ForEachArg<Iterator, Func> *args =
      new ForEachArg<Iterator, Func>[thread_count];
    Thread *threads = new Thread[thread_count];
    Mutex lock;
    for (int i = 0; i < thread_count; i++) {
      ForEachArg<Iterator, Func> *arg = &args[i];
      arg->iter_ = iter;
      arg->lock_ = &lock;
      arg->func_ = func->copy();
      threads[i].beginTask(forEachBegin<Iterator, Func, FuncArg>,
			   reinterpret_cast<void*>(arg));
    }

    for (int i = 0; i < thread_count; i++) {
      threads[i].wait();
      ForEachArg<Iterator, Func> *arg = &args[i];
      delete arg->func_;
    }

    delete [] threads;
    delete [] args;
  }
}

} // namespace
#endif
