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
#include <cctype>
#include <charconv>
#include <system_error>
#include <version>

namespace sta {

bool
isDigits(const char *str)
{
  for (const char *s = str; *s; s++) {
    if (!isdigit(*s))
      return false;
  }
  return true;
}

bool
isDigits(std::string_view str)
{
  for (char ch : str) {
    if (!isdigit(ch))
      return false;
  }
  return true;
}

// Wrapper for the absurdly named std::from_chars.
std::pair<float, bool>
stringFloat(const std::string &str)
{
  float value;
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
  if (ec == std::errc()
      && *ptr == '\0')
    return {value, true};
  else
    return {0.0, false};
#else
  char *ptr;
  value = strtof(str.data(), &ptr);
  if (!errno || *ptr != '\0')
    return {0.0, false};
  else
    return {value, true};
#endif
}

void
trimRight(std::string &str)
{
  str.erase(str.find_last_not_of(" ") + 1);
}

StringSeq
parseTokens(const std::string &text,
            std::string_view delims)
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

} // namespace
