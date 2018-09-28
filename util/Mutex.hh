// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
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

#ifndef STA_MUTEX_H
#define STA_MUTEX_H

#include <errno.h>
#include "Pthread.hh"
#include "DisallowCopyAssign.hh"
#include "ThreadException.hh"

namespace sta {

// C++ wrapper/facade for pthread_mutex_t.
class Mutex
{
public:
  Mutex();
  ~Mutex();
  void lock();
  bool trylock();
  void unlock();

private:
  DISALLOW_COPY_AND_ASSIGN(Mutex);

  pthread_mutex_t mutex_;

  friend class Condition;
};

inline void
Mutex::lock()
{
  int error = pthread_mutex_lock(&mutex_);
  CheckThreadError(error);
}

// Return true if lock is successful.
inline bool
Mutex::trylock()
{
  int error = pthread_mutex_trylock(&mutex_);
  if (error == 0)
    return true;
  else if (error != EBUSY)
    throw ThreadException(__FILE__, __LINE__, error);
  return false;
}

inline void
Mutex::unlock()
{
  int error = pthread_mutex_unlock(&mutex_);
  CheckThreadError(error);
}

} // namespace
#endif
