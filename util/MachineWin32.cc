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

#include <stdio.h>
#include <windows.h> // GetSystemInfo

#include "StaConfig.hh" // HAVE_PTHREAD_H

namespace sta {

// Windows returns -1 if the string does not fit rather than the
// required string length as the standard specifies.
int
vsnprint(char *str, size_t size, const char *fmt, va_list args)
{
  // Copy args before using them because consumption is destructive.
  va_list args_copy1;
  va_copy(args_copy1, args);
  int length = vsnprintf(str, size, fmt, args);

  while (length < 0) {
    size *= 2;
    char *buffer = new char[size];
    va_list args_copy2;
    va_copy(args_copy2, args_copy1);
    length = vsnprintf(buffer, size, fmt, args_copy2);
    delete [] buffer;
  }
  return length;
}

int
processorCount()
{
#if HAVE_PTHREAD_H
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
#else
  return 1;
#endif
}

void
initElapsedTime()
{
}

double
elapsedRunTime()
{
  return 0.0;
}

double
userRunTime()
{
  return 0.0;
}

double
systemRunTime()
{
  return 0.0;
}

size_t
memoryUsage()
{
  return 0;
}

} // namespace
