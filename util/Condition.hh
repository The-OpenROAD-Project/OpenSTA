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

#ifndef STA_CONDITION_H
#define STA_CONDITION_H

#include "Pthread.hh"

namespace sta {

class Mutex;

// C++ wrapper/facade for pthread_cond_t.
class Condition
{
public:
  Condition();
  ~Condition();
  void signal();
  void broadcast();
  void wait(Mutex &mutex);

private:
  pthread_cond_t condition_;
};

} // namespace
#endif
