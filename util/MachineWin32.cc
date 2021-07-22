// Parallax Static Timing Analyzer
// Copyright (c) 2021, Parallax Software, Inc.
// All rights reserved.
// 
// No part of this document may be copied, transmitted or
// disclosed in any form or fashion without the express
// written consent of Parallax Software, Inc.

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
vasprintf(char **str, const char *fmt, va_list args)
{
  size_t size = 1024;
  for (;;) {
    size *= 2;
    char *buffer = new char[size];
    // Copy args before using them because consumption is destructive.
    va_list args_copy2;
    va_copy(args_copy2, args);
    int length = vsnprintf(buffer, size, fmt, args_copy2);
    if (length >= 0) {
      *str = buffer;
      return length;
    }
    delete[] buffer;
  }
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
