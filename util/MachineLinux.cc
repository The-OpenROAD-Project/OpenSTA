// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "Machine.hh"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <thread>

#include "StaConfig.hh"
#include "StringUtil.hh"

namespace sta {

static struct timeval elapsed_begin_time_;

int
processorCount()
{
  return std::thread::hardware_concurrency();
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
    constexpr size_t line_length = 128;
    char line[line_length];
    while (fgets(line, line_length, status) != nullptr) {
      char *field = strtok(line, " \t");
      if (field && stringEq(field, "VmRSS:")) {
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

} // namespace
