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

#include "VerilogNamespace.hh"

#include <cctype>

#include "StringUtil.hh"
#include "ParseBus.hh"

namespace sta {

constexpr char verilog_escape = '\\';

static string
staToVerilog(const char *sta_name);
static string
staToVerilog2(const char *sta_name);
static string
verilogToSta(const string *verilog_name);

string
cellVerilogName(const char *sta_name)
{
  return staToVerilog(sta_name);
}

string
instanceVerilogName(const char *sta_name)
{
  return staToVerilog(sta_name);
}

string
netVerilogName(const char *sta_name)
{
  bool is_bus;
  string bus_name;
  int index;
  parseBusName(sta_name, '[', ']', verilog_escape, is_bus, bus_name, index);
  if (is_bus) {
    string bus_vname = staToVerilog(bus_name.c_str());
    string vname;
    stringPrint(vname, "%s[%d]", bus_vname.c_str(), index);
    return vname;
  }
  else
    return staToVerilog2(sta_name);
}

string
portVerilogName(const char *sta_name)
{
  return staToVerilog2(sta_name);
}

static string
staToVerilog(const char *sta_name)
{
  // Leave room for leading escape and trailing space if the name
  // needs to be escaped.
  // Assume the name has to be escaped and start copying while scanning.
  string escaped_name =  "\\";
  bool escaped = false;
  for (const char *s = sta_name; *s ; s++) {
    char ch = s[0];
    if (ch == verilog_escape) {
      char next_ch = s[1];
      if (next_ch == verilog_escape) {
	escaped_name += ch;
	escaped_name += next_ch;
	s++;
      }
      else
	// Skip escape.
	escaped = true;
    }
    else {
      if ((!(isalnum(ch) || ch == '_')))
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
    return string(sta_name);
}

static string
staToVerilog2(const char *sta_name)
{
  constexpr char bus_brkt_left = '[';
  constexpr char bus_brkt_right = ']';
  // Leave room for leading escape and trailing space if the name
  // needs to be escaped.
  string escaped_name =  "\\";
  // Assume the name has to be escaped and start copying while scanning.
  bool escaped = false;
  for (const char *s = sta_name; *s ; s++) {
    char ch = s[0];
    if (ch == verilog_escape) {
      char next_ch = s[1];
      if (next_ch == verilog_escape) {
	escaped_name += ch;
	escaped_name += next_ch;
	s++;
      }
      else
	// Skip escape.
	escaped = true;
    }
    else {
      bool is_brkt = (ch == bus_brkt_left || ch == bus_brkt_right);
      if ((!(isalnum(ch) || ch == '_') && !is_brkt)
	  || is_brkt)
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
    return string(sta_name);
}

////////////////////////////////////////////////////////////////

string
moduleVerilogToSta(const string *module_name)
{
  return verilogToSta(module_name);
}

string
instanceVerilogToSta(const string *inst_name)
{
  return verilogToSta(inst_name);
}

string
netVerilogToSta(const string *net_name)
{
  return verilogToSta(net_name);
}

string
portVerilogToSta(const string *port_name)
{
  return verilogToSta(port_name);
}

static string
verilogToSta(const string *verilog_name)
{
  if (verilog_name->front() == '\\') {
    constexpr char divider = '/';
    constexpr char bus_brkt_left = '[';
    constexpr char bus_brkt_right = ']';

    size_t verilog_name_length = verilog_name->size();
    if (isspace(verilog_name->back()))
      verilog_name_length--;
    string sta_name;
    // Ignore leading '\'.
    for (size_t i = 1; i < verilog_name_length; i++) {
      char ch = verilog_name->at(i);
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
    return string(*verilog_name);
}

} // namespace
