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

#include "ParseBus.hh"

#include <string>
#include <string_view>

#include "StringUtil.hh"

namespace sta {

bool
isBusName(std::string_view name,
          const char brkt_left,
          const char brkt_right,
          char escape)
{
  size_t len = name.size();
  // Shortest bus name is a[0].
  if (len >= 4
      // Escaped bus brackets are not buses.
      && name[len - 2] != escape
      && name[len - 1] == brkt_right) {
    size_t left = name.rfind(brkt_left);
    return left != std::string_view::npos;
  }
  else
    return false;
}

void
parseBusName(std::string_view name,
             const char brkt_left,
             const char brkt_right,
             const char escape,
             // Return values.
             bool &is_bus,
             std::string &bus_name,
             int &index)
{
  parseBusName(name, std::string_view(&brkt_left, 1),
               std::string_view(&brkt_right, 1), escape,
               is_bus, bus_name, index);
}

void
parseBusName(std::string_view name,
             std::string_view brkts_left,
             std::string_view brkts_right,
             char escape,
             // Return values.
             bool &is_bus,
             std::string &bus_name,
             int &index)
{
  is_bus = false;
  size_t len = name.size();
  // Shortest bus name is a[0].
  if (len >= 4
      // Escaped bus brackets are not buses.
      && name[len - 2] != escape) {
    char last_ch = name[len - 1];
    size_t brkt_index = brkts_right.find(last_ch);
    if (brkt_index != std::string_view::npos) {
      char brkt_left_ch = brkts_left[brkt_index];
      size_t left = name.rfind(brkt_left_ch);
      if (left != std::string_view::npos) {
        is_bus = true;
        bus_name.append(name.substr(0, left));
        // Simple bus subscript.
        index = std::stoi(std::string(name.substr(left + 1)));
      }
    }
  }
}

void
parseBusName(std::string_view name,
             const char brkt_left,
             const char brkt_right,
             char escape,
             // Return values.
             bool &is_bus,
             bool &is_range,
             std::string &bus_name,
             int &from,
             int &to,
             bool &subscript_wild)
{
  parseBusName(name, std::string_view(&brkt_left, 1),
               std::string_view(&brkt_right, 1), escape,
               is_bus, is_range, bus_name, from, to, subscript_wild);
}

void
parseBusName(std::string_view name,
             std::string_view brkts_left,
             std::string_view brkts_right,
             char escape,
             // Return values.
             bool &is_bus,
             bool &is_range,
             std::string &bus_name,
             int &from,
             int &to,
             bool &subscript_wild)
{
  is_bus = false;
  is_range = false;
  subscript_wild = false;
  size_t len = name.size();
  // Shortest bus is a[0].
  if (len >= 4
      // Escaped bus brackets are not buses.
      && name[len - 2] != escape) {
    char last_ch = name[len - 1];
    size_t brkt_index = brkts_right.find(last_ch);
    if (brkt_index != std::string_view::npos) {
      char brkt_left_ch = brkts_left[brkt_index];
      size_t left = name.rfind(brkt_left_ch);
      if (left != std::string_view::npos) {
        is_bus = true;
        bus_name.append(name.substr(0, left));
        // Check for bus range.
        size_t range = name.find(':', left);
        if (range != std::string_view::npos) {
          is_range = true;
          from = std::stoi(std::string(name.substr(left + 1)));
          to = std::stoi(std::string(name.substr(range + 1)));
        }
        else {
          if (left + 1 < len && name[left + 1] == '*')
            subscript_wild = true;
          else
            from = to = std::stoi(std::string(name.substr(left + 1)));
        }
      }
    }
  }
}

std::string
escapeChars(std::string_view token,
            const char ch1,
            const char ch2,
            const char escape)
{
  std::string escaped;
  escaped.reserve(token.size());
  for (size_t i = 0; i < token.size(); i++) {
    char ch = token[i];
    if (ch == escape) {
      if (i + 1 < token.size()) {
        escaped += ch;
        escaped += token[i + 1];
        i++;
      }
      else
        escaped += ch;
    }
    else if (ch == ch1 || ch == ch2) {
      escaped += escape;
      escaped += ch;
    }
    else
      escaped += ch;
  }
  return escaped;
}

} // namespace sta
