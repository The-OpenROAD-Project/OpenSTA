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

#ifndef STA_READ_WRITE_LOCK_H
#define STA_READ_WRITE_LOCK_H

#include "DisallowCopyAssign.hh"
#include "Error.hh"
#include "Pthread.hh"
#include "Mutex.hh"
#include "Condition.hh"

namespace sta {

// C++ wrapper/facade for pthread_rwlock_t.
class ReadWriteLock
{
public:
  ReadWriteLock();
  ~ReadWriteLock();
  void readLock();
  void writeLock();
  void unlock();

private:
  DISALLOW_COPY_AND_ASSIGN(ReadWriteLock);

  pthread_rwlock_t rw_lock_;
};

inline void
ReadWriteLock::readLock()
{
  int error = pthread_rwlock_rdlock(&rw_lock_);
  CheckThreadError(error);
}

inline void
ReadWriteLock::writeLock()
{
  int error = pthread_rwlock_wrlock(&rw_lock_);
  CheckThreadError(error);
}

inline void
ReadWriteLock::unlock()
{
  int error = pthread_rwlock_unlock(&rw_lock_);
  CheckThreadError(error);
}

} // namespace
#endif
