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

#if defined(_WINDOWS) || defined(_WIN32)

#include <stdio.h>
#include <windows.h> // GetSystemInfo
#include "Machine.hh"
#include "StaConfig.hh"

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

}

#else // _WINDOWS

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "Machine.hh"
#include "StaConfig.hh"
#include "StringUtil.hh"

namespace sta {

static struct timeval elapsed_begin_time_;

int
processorCount()
{
#if HAVE_PTHREAD_H
  return sysconf(_SC_NPROCESSORS_CONF);
#else
  return 1;
#endif
}

void
initElapsedTime()
{
  struct timezone tz;
  gettimeofday(&elapsed_begin_time_, &tz);
}

double
elapsedRunTime()
{
  static struct timeval time;
  struct timezone tz;
  gettimeofday(&time, &tz);
  return time.tv_sec - elapsed_begin_time_.tv_sec
    + (time.tv_usec - elapsed_begin_time_.tv_usec) * 1E-6;
}

double
userRunTime()
{
  struct rusage rusage;
  getrusage(RUSAGE_SELF, &rusage);
  return rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec * 1e-6;
}

double
systemRunTime()
{
  struct rusage rusage;
  getrusage(RUSAGE_SELF, &rusage);
  return rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec * 1e-6;
}

// rusage->ru_maxrss is not set in linux so read it from /proc.
size_t
memoryUsage()
{
  string proc_filename;
  stringPrint(proc_filename, "/proc/%d/status", getpid());
  size_t memory = 0;
  FILE *status = fopen(proc_filename.c_str(), "r");
  if (status) {
    const size_t line_length = 128;
    char line[line_length];
    while (fgets(line, line_length, status) != nullptr) {
      char *field = strtok(line, " \t");
      if (stringEq(field, "VmRSS:")) {
	char *size = strtok(nullptr, " \t");
	if (size) {
	  char *ignore;
	  // VmRSS is in kilobytes.
	  memory = strtol(size, &ignore, 10) * 1000;
	  break;
	}
      }
    }
    fclose(status);
  }
  return memory;
}

}

#endif // !_WINDOWS
