// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#pragma once

// This header contains global os/port specific definitions.

// Pragma placeholder for non-gcc compilers.
#ifndef __GNUC__
  #define __attribute__(x)
#endif // __GNUC__

#ifdef _MSC_VER
  // Microcruft Visual C++
  // Obtuse warning codes enabled by pragma.
  //   4018 = signed, unsigned mismatch
  //   4032 = unexpected type promotions
  //   4100 = unused argument
  //   4132 = uninitialized const
  //   4189 = unused local variable
  //   4201 = unnamed struct
  //   4222 = static class member declared static at file scope
  //   4234 = use of keyword reserved for future use
  //   4245 = negative const converted to unsigned
  //   4355 = use of 'this' in invalid context
  //   4505 = function not used
  //   4611 = setjmp used in C++ function
  //   4701 = variable used but not initialized
  #pragma warning( 3 : 4018 4032 4132 4189 4201 4222 4234 4505 4611 4701 )
  // Disable security warnings for posix functions.
  // _CRT_SECURE_NO_WARNINGS does not seem to work
  #pragma warning( disable : 4996 )
#endif // _MSC_VER

#if defined(_WINDOWS) || defined(_WIN32)
  #include <stdarg.h>
  #include <inttypes.h>
  #define va_copy(d,s) ((d)=(s))
  #ifndef __GNUC__
    #define strcasecmp _stricmp
    #define strncasecmp strncmp
    #define strtoull _strtoui64
  #endif
  // Flex doesn't check for unistd.h.
  #define YY_NO_UNISTD_H
  namespace sta {
    int vsnprint(char *str, size_t size, const char *fmt, va_list args);
    int vasprintf(char **str, const char *fmt, va_list args);
  }
#else
  #define vsnprint vsnprintf
#endif

#include <cstddef>		// size_t

namespace sta {

int
processorCount();

// Init elapsed (wall) time.
void
initElapsedTime();

// Elapsed/wall time (in seconds).
double
elapsedRunTime();

// User run time (in seconds).
double
userRunTime();

// System run time (in seconds).
double
systemRunTime();

// Memory usage in bytes.
size_t
memoryUsage();

} // namespace sta
