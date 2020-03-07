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

#pragma once

#include <stdarg.h>
#include <string.h>
#include <string>
#include "Vector.hh"

namespace sta {

using std::string;

inline bool
stringEq(const char *str1,
	 const char *str2)
{
  return strcmp(str1, str2) == 0;
}

// Compare the first length characters.
inline bool
stringEq(const char *str1,
	 const char *str2,
	 size_t length)
{
  return strncmp(str1, str2, length) == 0;
}

inline bool
stringEqIf(const char *str1,
	   const char *str2)
{
  return (str1 == nullptr && str2 == nullptr)
    || (str1 && str2 && strcmp(str1, str2) == 0);
}

// Case sensitive compare the beginning of str1 to str2.
inline bool
stringBeginEq(const char *str1,
	      const char *str2)
{
  return strncmp(str1, str2, strlen(str2)) == 0;
}

// Case insensitive compare the beginning of str1 to str2.
inline bool
stringBeginEqual(const char *str1,
		 const char *str2)
{
  return strncasecmp(str1, str2, strlen(str2)) == 0;
}

// Case insensitive compare.
inline bool
stringEqual(const char *str1,
	    const char *str2)
{
  return strcasecmp(str1, str2) == 0;
}

inline bool
stringEqualIf(const char *str1,
	      const char *str2)
{
  return (str1 == nullptr && str2 == nullptr)
    || (str1 && str2 && strcasecmp(str1, str2) == 0);
}

inline bool
stringLess(const char *str1,
	   const char *str2)
{
  return strcmp(str1, str2) < 0;
}

inline bool
stringLessIf(const char *str1,
	     const char *str2)
{
  return (str1 == nullptr && str2 != nullptr)
    || (str1 != nullptr && str2 != nullptr && strcmp(str1, str2) < 0);
}

class CharPtrLess
{
public:
  bool operator()(const char *string1,
		  const char *string2) const
  {
    return stringLess(string1, string2);
  }
};

// Case insensitive comparision.
class CharPtrCaseLess
{
public:
  bool operator()(const char *string1,
		  const char *string2) const
  {
    return strcasecmp(string1, string2) < 0;
  }
};

// strdup using new instead of malloc so delete can be used on the strings.
char *
stringCopy(const char *str);

inline void
stringAppend(char *&str1,
	     const char *str2)
{
  strcpy(str1, str2);
  str1 += strlen(str2);
}

// Delete for strings allocated with new char[].
inline void
stringDelete(const char *str)
{
  delete [] str;
}

bool
isDigits(const char *str);

// Print to a new string.
// Caller owns returned string.
char *
stringPrint(const char *fmt,
	    ...) __attribute__((format (printf, 1, 2)));
string
stdstrPrint(const char *fmt,
	       ...) __attribute__((format (printf, 1, 2)));
char *
stringPrintArgs(const char *fmt,
		va_list args);
void
stringPrint(string &str,
	    const char *fmt,
	    ...) __attribute__((format (printf, 2, 3)));

// Print to a temporary string.
char *
stringPrintTmp(const char *fmt,
	       ...)  __attribute__((format (printf, 1, 2)));
// Caller owns returned string.
char *
integerString(int number);

char *
makeTmpString(size_t length);
void
initTmpStrings();
void
deleteTmpStrings();

////////////////////////////////////////////////////////////////

// Trim right spaces.
void
trimRight(string &str);

typedef Vector<string> StringVector;

void
split(const string &text,
      const string &delims,
      // Return values.
      StringVector &tokens);

} // namespace
