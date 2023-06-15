// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include "VerilogNamespace.hh"

#include <cctype>

#include "StringUtil.hh"
#include "ParseBus.hh"

namespace sta {

static string
staToVerilog(const char *sta_name,
	     const char escape);
static string
staToVerilog2(const char *sta_name,
              const char escape);
static string
verilogToSta(const char *verilog_name);

string
instanceVerilogName(const char *sta_name,
		    const char escape)
{
  return staToVerilog(sta_name, escape);
}

string
netVerilogName(const char *sta_name,
	       const char escape)
{
  bool is_bus;
  string bus_name;
  int index;
  parseBusName(sta_name, '[', ']', escape, is_bus, bus_name, index);
  if (is_bus) {
    string bus_vname = staToVerilog(bus_name.c_str(), escape);
    string vname;
    stringPrint(vname, "%s[%d]", bus_vname.c_str(), index);
    return vname;
  }
  else
    return staToVerilog2(sta_name, escape);
}

string
portVerilogName(const char *sta_name,
		const char escape)
{
  return staToVerilog2(sta_name, escape);
}

static string
staToVerilog(const char *sta_name,
	     const char escape)
{
  // Leave room for leading escape and trailing space if the name
  // needs to be escaped.
  // Assume the name has to be escaped and start copying while scanning.
  string escaped_name =  "\\";
  bool escaped = false;
  for (const char *s = sta_name; *s ; s++) {
    char ch = s[0];
    if (ch == escape) {
      char next_ch = s[1];
      if (next_ch == escape) {
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
staToVerilog2(const char *sta_name,
              const char escape)
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
    if (ch == escape) {
      char next_ch = s[1];
      if (next_ch == escape) {
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
moduleVerilogToSta(const char *module_name)
{
  return verilogToSta(module_name);
}

string
instanceVerilogToSta(const char *inst_name)
{
  return verilogToSta(inst_name);
}

string
netVerilogToSta(const char *net_name)
{
  return verilogToSta(net_name);
}

string
portVerilogToSta(const char *port_name)
{
  return verilogToSta(port_name);
}

static string
verilogToSta(const char *verilog_name)
{
  if (verilog_name && verilog_name[0] == '\\') {
    constexpr char divider = '/';
    constexpr char escape = '\\';
    constexpr char bus_brkt_left = '[';
    constexpr char bus_brkt_right = ']';

    // Ignore leading '\'.
    verilog_name = &verilog_name[1];
    size_t verilog_name_length = strlen(verilog_name);
    if (verilog_name[verilog_name_length - 1] == ' ')
      verilog_name_length--;
    string sta_name;
    for (size_t i = 0; i < verilog_name_length; i++) {
      char ch = verilog_name[i];
      if (ch == bus_brkt_left
          || ch == bus_brkt_right
          || ch == divider
          || ch == escape)
          // Escape bus brackets, dividers and escapes.
	sta_name += escape;
      sta_name += ch;
    }
    return sta_name;
  }
  else
    return string(verilog_name);
}

} // namespace
