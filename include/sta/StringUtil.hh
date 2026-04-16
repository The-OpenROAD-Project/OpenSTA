// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#pragma once

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "Machine.hh" // __attribute__

namespace sta {

using StringSeq = std::vector<std::string>;
using StringSet = std::set<std::string>;

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

// Case sensitive compare the beginning of str1 to str2.
inline bool
stringBeginEq(const char *str1,
              const char *str2)
{
  return strncmp(str1, str2, strlen(str2)) == 0;
}

inline bool
charEqual(unsigned char c1,
          unsigned char c2)
{
  return std::tolower(c1) == std::tolower(c2);
}

// Case insensitive compare the beginning of str1 to str2.
inline bool
stringBeginEqual(std::string_view str,
                 std::string_view prefix)
{
  if (str.size() < prefix.size())
    return false;
  return std::ranges::equal(str.substr(0, prefix.size()), prefix, charEqual);
}

// Case insensitive compare.
inline bool
stringEqual(std::string_view s1,
            std::string_view s2)
{
  return std::ranges::equal(s1, s2, charEqual);
}

std::pair<float, bool>
stringFloat(const std::string &str);

bool
isDigits(const char *str);

bool
isDigits(std::string_view str);

////////////////////////////////////////////////////////////////

// Trim right spaces.
void
trimRight(std::string &str);

// Parse text into delimiter separated tokens and skip whitepace.
StringSeq
parseTokens(const std::string &text,
            std::string_view delims = " \t");

} // namespace sta
