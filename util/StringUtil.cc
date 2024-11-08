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

#include "StringUtil.hh"

#include <limits>
#include <cctype>
#include <cstdio>
#include <array>

#include "Machine.hh"
#include "Mutex.hh"

namespace sta {

static void
stringPrintTmp(const char *fmt,
	       va_list args,
	       // Return values.
	       char *&str,
	       size_t &length);
static void
getTmpString(// Return values.
	     char *&str,
	     size_t &length);

char *
stringCopy(const char *str)
{
  if (str) {
    char *copy = new char[strlen(str) + 1];
    strcpy(copy, str);
    return copy;
  }
  else
    return nullptr;
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

// print for c++ strings.
void
stringPrint(string &str,
	    const char *fmt,
	    ...)
{
  va_list args;
  va_start(args, fmt);
  char *tmp;
  size_t tmp_length;
  stringPrintTmp(fmt, args, tmp, tmp_length);
  va_end(args);
  str = tmp;
}

void
stringAppend(string &str,
             const char *fmt,
             ...)
{
  va_list args;
  va_start(args, fmt);
  char *tmp;
  size_t tmp_length;
  stringPrintTmp(fmt, args, tmp, tmp_length);
  va_end(args);
  str += tmp;
}

string
stdstrPrint(const char *fmt,
	    ...)
{
  va_list args;
  va_start(args, fmt);
  char *tmp;
  size_t tmp_length;
  stringPrintTmp(fmt, args, tmp, tmp_length);
  va_end(args);
  return tmp;
}

char *
stringPrint(const char *fmt,
	    ...)
{
  va_list args;
  va_start(args, fmt);
  char *result = stringPrintArgs(fmt, args);
  va_end(args);
  return result;
}

char *
stringPrintArgs(const char *fmt,
		va_list args)
{
  char *tmp;
  size_t tmp_length;
  stringPrintTmp(fmt, args, tmp, tmp_length);
  char *result = new char[tmp_length + 1];
  strcpy(result, tmp);
  return result;
}

char *
stringPrintTmp(const char *fmt,
	       ...)
{
  va_list args;
  va_start(args, fmt);
  char *tmp;
  size_t tmp_length;
  stringPrintTmp(fmt, args, tmp, tmp_length);
  va_end(args);
  return tmp;
}

static void
stringPrintTmp(const char *fmt,
	       va_list args,
	       // Return values.
	       char *&tmp,
	       // strlen(tmp), not including terminating '\0'.
	       size_t &tmp_length)
{
  size_t tmp_length1;
  getTmpString(tmp, tmp_length1);

  va_list args_copy;
  va_copy(args_copy, args);
  // Returned length does NOT include trailing '\0'.
  tmp_length = vsnprint(tmp, tmp_length1, fmt, args_copy);
  va_end(args_copy);

  if (tmp_length >= tmp_length1) {
    tmp_length1 = tmp_length + 1;
    stringDelete(tmp);
    tmp = makeTmpString(tmp_length1);
    va_copy(args_copy, args);
    tmp_length = vsnprint(tmp, tmp_length1, fmt, args_copy);
    va_end(args_copy);
  }
}

////////////////////////////////////////////////////////////////

static constexpr size_t tmp_string_count = 256;
static constexpr size_t tmp_string_initial_length = 256;
thread_local static std::array<char*, tmp_string_count> tmp_strings;
thread_local static std::array<size_t, tmp_string_count> tmp_string_lengths;
thread_local static int tmp_string_next = 0;

static void
getTmpString(// Return values.
	     char *&str,
	     size_t &length)
{
  if (tmp_string_next == tmp_string_count)
    tmp_string_next = 0;
  str = tmp_strings[tmp_string_next];
  length = tmp_string_lengths[tmp_string_next];
  if (str == nullptr) {
    str = tmp_strings[tmp_string_next] = new char[tmp_string_initial_length];
    length = tmp_string_lengths[tmp_string_next] = tmp_string_initial_length;
  }
  tmp_string_next++;
}

char *
makeTmpString(size_t length)
{
  if (tmp_string_next == tmp_string_count)
    tmp_string_next = 0;
  char *tmp_str = tmp_strings[tmp_string_next];
  size_t tmp_length = tmp_string_lengths[tmp_string_next];
  if (tmp_length < length) {
    // String isn't long enough.  Make a new one.
    delete [] tmp_str;
    tmp_str = new char[length];
    tmp_strings[tmp_string_next] = tmp_str;
    tmp_string_lengths[tmp_string_next] = length;
  }
  tmp_string_next++;
  return tmp_str;
}

void
stringDeleteCheck(const char *str)
{
  if (isTmpString(str)) {
    printf("Critical error: stringDelete for tmp string.");
    exit(1);
  }
}

bool
isTmpString(const char *str)
{
  if (!tmp_strings.empty()) {
    for (size_t i = 0; i < tmp_string_count; i++) {
      if (str == tmp_strings[i])
        return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////

void
trimRight(string &str)
{
  str.erase(str.find_last_not_of(" ") + 1);
}

void
split(const string &text,
      const string &delims,
      // Return values.
      StringVector &tokens)
{
  auto start = text.find_first_not_of(delims);
  auto end = text.find_first_of(delims, start);
  while (end != string::npos) {
    tokens.push_back(text.substr(start, end - start));
    start = text.find_first_not_of(delims, end);
    end = text.find_first_of(delims, start);
  }
  if (start != string::npos)
    tokens.push_back(text.substr(start));
}

} // namespace
