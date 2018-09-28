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

#include <limits>
#include <ctype.h>
#include <stdio.h>
#include "Machine.hh"
#include "Mutex.hh"
#include "StringUtil.hh"

namespace sta {

char *
stringCopy(const char *str)
{
  if (str) {
    char *copy = new char[strlen(str) + 1];
    strcpy(copy, str);
    return copy;
  }
  else
    return NULL;
}

bool
isDigits(const char *str)
{
  for (const char *s = str; *s; s++) {
    if (!isdigit(*s))
      return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////

char *
integerString(int number)
{
  // Leave room for sign and '\0'.
  return stringPrint(INT_DIGITS + 2, "%d", number);
}

char *
stringPrint(int length_estimate,
	    const char *fmt,
	    ...)
{
  va_list args;
  va_start(args, fmt);
  char *result = stringPrintArgs(length_estimate, fmt, args);
  va_end(args);
  return result;
}

char *
stringPrintArgs(int length_estimate,
		const char *fmt,
		va_list args)
{
  va_list args_copy;
  va_copy(args_copy, args);
  char *result = new char[length_estimate];
  int length = vsnprint(result, length_estimate, fmt, args);
  if (length >= length_estimate) {
    stringDelete(result);
    result = new char[length + 1];
    vsnprint(result, length + 1, fmt, args_copy);
  }
  va_end(args_copy);
  return result;
}

char *
stringPrintTmp(int length_estimate,
	       const char *fmt,
	       ...)
{
  char *result = makeTmpString(length_estimate);
  va_list args;

  va_start(args, fmt);
  int length = vsnprint(result, length_estimate, fmt, args);
  va_end(args);

  // Returned length does NOT include trailing '\0'.
  if (length >= length_estimate) {
    result = makeTmpString(length + 1);
    va_start(args, fmt);
    vsnprint(result, length + 1, fmt, args);
    va_end(args);
  }
  return result;
}

////////////////////////////////////////////////////////////////

static int tmp_string_count_ = 100;
static size_t tmp_string_length_ = 100;
static int tmp_string_next_ = 0;
static char **tmp_strings_ = NULL;
static size_t *tmp_string_lengths_ = NULL;
static Mutex string_lock_;

char *
makeTmpString(size_t length)
{
  string_lock_.lock();
  if (tmp_strings_ == NULL) {
    tmp_strings_ = new char*[tmp_string_count_];
    tmp_string_lengths_ = new size_t[tmp_string_count_];
    for (int i = 0; i < tmp_string_count_; i++) {
      tmp_strings_[i] = new char[tmp_string_length_];
      tmp_string_lengths_[i] = tmp_string_length_;
    }
  }
  if (tmp_string_next_ == tmp_string_count_)
    tmp_string_next_ = 0;
  char *tmp_str = tmp_strings_[tmp_string_next_];
  size_t tmp_length = tmp_string_lengths_[tmp_string_next_];
  if (tmp_length < length) {
    // String isn't long enough.  Make a new one.
    stringDelete(tmp_str);
    tmp_str = new char[length];
    tmp_strings_[tmp_string_next_] = tmp_str;
    tmp_string_lengths_[tmp_string_next_] = length;
  }
  tmp_string_next_++;
  string_lock_.unlock();
  return tmp_str;
}

void
deleteTmpStrings()
{
  if (tmp_strings_) {
    for (int i = 0; i < tmp_string_count_; i++)
      delete [] tmp_strings_[i];
    delete [] tmp_strings_;
    tmp_strings_ = NULL;

    delete [] tmp_string_lengths_;
    tmp_string_lengths_ = NULL;
  }
}

////////////////////////////////////////////////////////////////

void
trimRight(string &str)
{
  str.erase(str.find_last_not_of(" ") + 1);
}

} // namespace
