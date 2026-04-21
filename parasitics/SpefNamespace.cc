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

#include "SpefNamespace.hh"

#include <cctype>
#include <cstring>
#include <string>

namespace sta {

std::string
spefToSta(std::string_view spef_name,
          char spef_divider,
          char path_divider,
          char path_escape)
{
  const char spef_escape = '\\';
  std::string sta_name;
  for (size_t i = 0; i < spef_name.size(); i++) {
    char ch = spef_name[i];
    if (ch == spef_escape) {
      char next_ch = spef_name[i + 1];
      if (next_ch == spef_divider) {
        // Translate spef escape to network escape.
        sta_name += path_escape;
        // Translate spef divider to network divider.
        sta_name += path_divider;
      }
      else if (next_ch == '['
               || next_ch == ']'
               || next_ch == spef_escape) {
        // Translate spef escape to network escape.
        sta_name += path_escape;
        sta_name += next_ch;
      }
      else
        // No need to escape other characters.
        sta_name += next_ch;
      i++;
    }
    else if (ch == spef_divider)
      // Translate spef divider to network divider.
      sta_name += path_divider;
    else
      // Just the normal noises.
      sta_name += ch;
  }
  return sta_name;
}

std::string
staToSpef(std::string_view sta_name,
          char spef_divider,
          char path_divider,
          char path_escape)
{
  const char spef_escape = '\\';
  std::string spef_name;
  for (size_t i = 0; i < sta_name.size(); i++) {
    char ch = sta_name[i];
    if (ch == path_escape) {
      char next_ch = sta_name[i + 1];
      if (next_ch == path_divider) {
        // Translate network escape to spef escape.
        spef_name += spef_escape;
        // Translate network divider to spef divider.
        spef_name += spef_divider;
      }
      else if (next_ch == '['
               || next_ch == ']') {
        // Translate network escape to spef escape.
        spef_name += spef_escape;
        spef_name += next_ch;
      }
      else
        // No need to escape other characters.
        spef_name += next_ch;
      i++;
    }
    else if (ch == path_divider)
      // Translate network divider to spef divider.
      spef_name += spef_divider;
    else if (!(isdigit(ch) || isalpha(ch) || ch == '_')) {
      // Escape non-alphanum characters.
      spef_name += spef_escape;
      spef_name += ch;
    }
    else
      // Just the normal noises.
      spef_name += ch;
  }
  return spef_name;
}

} // namespace sta
