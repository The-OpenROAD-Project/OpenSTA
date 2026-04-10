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

#include "VerilogNamespace.hh"

#include <cctype>

#include "StringUtil.hh"
#include "ParseBus.hh"

namespace sta {

constexpr char verilog_escape = '\\';

static std::string
staToVerilog(std::string_view sta_name);
static std::string
staToVerilog2(std::string_view sta_name);
static std::string
verilogToSta(const std::string_view verilog_name);

std::string
cellVerilogName(std::string_view sta_name)
{
  return staToVerilog(sta_name);
}

std::string
instanceVerilogName(std::string_view sta_name)
{
  return staToVerilog(sta_name);
}

std::string
netVerilogName(std::string_view sta_name)
{
  bool is_bus;
  std::string bus_name;
  int index;
  parseBusName(sta_name, '[', ']', verilog_escape, is_bus, bus_name, index);
  if (is_bus) {
    std::string bus_vname = staToVerilog(bus_name);
    std::string vname = bus_vname + '[' + std::to_string(index) + ']';
    return vname;
  }
  else
    return staToVerilog2(sta_name);
}

std::string
portVerilogName(std::string_view sta_name)
{
  return staToVerilog2(sta_name);
}

// <cctype> functions expect a value representable as unsigned char or EOF.
static bool
isAlnumUnderscore(char ch)
{
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

static std::string
staToVerilog(std::string_view sta_name)
{
  // Leave room for leading escape and trailing space if the name
  // needs to be escaped.
  // Assume the name has to be escaped and start copying while scanning.
  std::string escaped_name =  "\\";
  bool escaped = false;
  size_t sta_length = sta_name.size();
  for (size_t i = 0; i < sta_length; i++) {
    char ch = sta_name[i];
    if (ch == verilog_escape) {
      escaped = true;
      if (i + 1 < sta_length) {
        char next_ch = sta_name[i + 1];
        if (next_ch == verilog_escape) {
          escaped_name += next_ch;
          i++;
        }
      }
      else
        escaped_name += ch;
    }
    else {
      if (!isAlnumUnderscore(ch))
        escaped = true;
      escaped_name += ch;
    }
  }
  if (escaped) {
    // Add a terminating space.
    escaped_name += ' ';
    return escaped_name;
  }
  else
    return std::string(sta_name);
}

static std::string
staToVerilog2(std::string_view sta_name)
{
  constexpr char bus_brkt_left = '[';
  constexpr char bus_brkt_right = ']';
  // Leave room for leading escape and trailing space if the name
  // needs to be escaped.
  std::string escaped_name =  "\\";
  // Assume the name has to be escaped and start copying while scanning.
  bool escaped = false;
  size_t sta_length = sta_name.size();
  for (size_t i = 0; i < sta_length; i++) {
    char ch = sta_name[i];
    if (ch == verilog_escape) {
      escaped = true;
      if (i + 1 < sta_length) {
        char next_ch = sta_name[i + 1];
        if (next_ch == verilog_escape) {
          escaped_name += next_ch;
          i++;
        }
      }
      else
        escaped_name += ch;
    }
    else {
      bool is_brkt = (ch == bus_brkt_left || ch == bus_brkt_right);
      if ((!isAlnumUnderscore(ch) && !is_brkt) || is_brkt)
        escaped = true;
      escaped_name += ch;
    }
  }
  if (escaped) {
    // Add a terminating space.
    escaped_name += ' ';
    return escaped_name;
  }
  else
    return std::string(sta_name);
}

////////////////////////////////////////////////////////////////

std::string
moduleVerilogToSta(std::string_view module_name)
{
  return verilogToSta(module_name);
}

std::string
instanceVerilogToSta(std::string_view inst_name)
{
  return verilogToSta(inst_name);
}

std::string
netVerilogToSta(std::string_view net_name)
{
  return verilogToSta(net_name);
}

std::string
portVerilogToSta(std::string_view port_name)
{
  return verilogToSta(port_name);
}

static std::string
verilogToSta(std::string_view verilog_name)
{
  if (verilog_name.empty())
    return std::string(verilog_name);

  if (verilog_name.front() == '\\') {
    constexpr char divider = '/';
    constexpr char bus_brkt_left = '[';
    constexpr char bus_brkt_right = ']';

    size_t verilog_name_length = verilog_name.size();
    if (verilog_name_length > 1
        && std::isspace(static_cast<unsigned char>(verilog_name.back())) != 0)
      verilog_name_length--;
    std::string sta_name;
    // Ignore leading '\'.
    for (size_t i = 1; i < verilog_name_length; i++) {
      char ch = verilog_name[i];
      if (ch == bus_brkt_left
          || ch == bus_brkt_right
          || ch == divider
          || ch == verilog_escape)
          // Escape bus brackets, dividers and escapes.
        sta_name += verilog_escape;
      sta_name += ch;
    }
    return sta_name;
  }
  else
    return std::string(verilog_name);
}

} // namespace
