// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#include "StringUtil.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib> // exit

#include "Machine.hh"
#include "Mutex.hh"
#include "Error.hh"

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

static constexpr size_t tmp_string_count = 256;
static constexpr size_t tmp_string_initial_length = 256;
thread_local static std::array<char*, tmp_string_count> tmp_strings;
thread_local static std::array<size_t, tmp_string_count> tmp_string_lengths;
thread_local static int tmp_string_next = 0;

void
deleteTmpStrings()
{
  for (size_t i = 0; i < tmp_string_count; i++) {
    stringDelete(tmp_strings[i]);
    tmp_string_lengths[i] = 0;
    tmp_strings[i] = nullptr;
  }
  tmp_string_next = 0;
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
    tmp_length = std::max(tmp_string_initial_length, length);
    tmp_str = new char[tmp_length];
    tmp_strings[tmp_string_next] = tmp_str;
    tmp_string_lengths[tmp_string_next] = tmp_length;
  }
  tmp_string_next++;
  return tmp_str;
}

char *
makeTmpString(std::string &str)
{
  char *tmp = makeTmpString(str.length() + 1);
  strcpy(tmp, str.c_str());
  return tmp;
}

void
stringDeleteCheck(const char *str)
{
  if (isTmpString(str))
    criticalError(2600, "stringDelete for tmp string.");
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
trimRight(std::string &str)
{
  str.erase(str.find_last_not_of(" ") + 1);
}

StringSeq
split(const std::string &text,
      const std::string &delims)
{
  StringSeq tokens;
  auto start = text.find_first_not_of(delims);
  auto end = text.find_first_of(delims, start);
  while (end != std::string::npos) {
    tokens.push_back(text.substr(start, end - start));
    start = text.find_first_not_of(delims, end);
    end = text.find_first_of(delims, start);
  }
  if (start != std::string::npos)
    tokens.push_back(text.substr(start));
  return tokens;
}

// Parse space separated tokens.
StringSeq
parseTokens(const std::string &s,
            const char delimiter)
{
  StringSeq tokens;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && std::isspace(s[i]))
      ++i;
    size_t start = i;
    while (i < s.size() && s[i] != delimiter)
      ++i;
    if (start < i) {
      tokens.emplace_back(s, start, i - start);
      ++i;
    }
  }
  return tokens;
}

} // namespace
