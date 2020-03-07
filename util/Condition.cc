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
#include "Condition.hh"
#include "ThreadException.hh"
#include "Mutex.hh"

namespace sta {

Condition::Condition()
{
  int error = pthread_cond_init(&condition_, NULL);
  CheckThreadError(error);
}

Condition::~Condition()
{
  pthread_cond_destroy(&condition_);
}

void
Condition::signal()
{
  int error = pthread_cond_signal(&condition_);
  CheckThreadError(error);
}


void
Condition::broadcast()
{
  int error = pthread_cond_broadcast(&condition_);
  CheckThreadError(error);
}


void
Condition::wait(Mutex &mutex)
{
  int error = pthread_cond_wait(&condition_, &mutex.mutex_);
  CheckThreadError(error);
}

} // namespace
