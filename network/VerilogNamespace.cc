// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
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

#include <ctype.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "ParseBus.hh"
#include "VerilogNamespace.hh"

namespace sta {

const char *
instanceVerilogName(const char *sta_name,
		    const char escape)
{
  return staToVerilog(sta_name, escape);
}

const char *
netVerilogName(const char *sta_name,
	       const char escape)
{
  char *bus_name;
  int index;
  parseBusName(sta_name, '[', ']', escape, bus_name, index);
  if (bus_name) {
    const char *vname = stringPrintTmp("%s[%d]",
				       staToVerilog(bus_name, escape),
				       index);
    stringDelete(bus_name);
    return vname;
  }
  else
    return staToVerilog(sta_name, escape);
}

const char *
portVerilogName(const char *sta_name,
		const char escape)
{
  return staToVerilog(sta_name, escape);
}

// Append ch to str at insert.  Resize str if necessary.
static inline void
vstringAppend(char *&str,
	      char *&str_end,
	      char *&insert,
	      char ch)
{
  if (insert == str_end) {
    size_t length = str_end - str;
    size_t length2 = length * 2;
    char *new_str = makeTmpString(length2);
    strncpy(new_str, str, length);
    str = new_str;
    str_end = &str[length2];
    insert = &str[length];
  }
  *insert++ = ch;
}

const char *
staToVerilog(const char *sta_name,
	     const char escape)
{
  const char bus_brkt_left = '[';
  const char bus_brkt_right = ']';
  // Leave room for leading escape and trailing space if the name
  // needs to be escaped.
  size_t verilog_name_length = strlen(sta_name) + 3;
  char *verilog_name = makeTmpString(verilog_name_length);
  char *verilog_name_end = &verilog_name[verilog_name_length];
  char *v = verilog_name;
  // Assume the name has to be escaped and start copying while scanning.
  bool escaped = false;
  *v++ = '\\';
  for (const char *s = sta_name; *s ; s++) {
    char ch = s[0];
    if (ch == escape) {
      char next_ch = s[1];
      if (next_ch == escape) {
	vstringAppend(verilog_name, verilog_name_end, v, ch);
	vstringAppend(verilog_name, verilog_name_end, v, next_ch);
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
      vstringAppend(verilog_name, verilog_name_end, v, ch);
    }
  }
  if (escaped) {
    // Add a terminating space.
    vstringAppend(verilog_name, verilog_name_end, v, ' ');
    vstringAppend(verilog_name, verilog_name_end, v, '\0');
    return verilog_name;
  }
  else
    return sta_name;
}

const char *
verilogToSta(const char *verilog_name)
{
  if (verilog_name[0] == '\\') {
    const char divider = '/';
    const char escape = '\\';
    const char bus_brkt_left = '[';
    const char bus_brkt_right = ']';
    size_t sta_name_length = strlen(verilog_name) + 1;
    char *sta_name = makeTmpString(sta_name_length);
    char *sta_name_end = &sta_name[sta_name_length];
    char *s = sta_name;
    for (const char *v = &verilog_name[1]; *v ; v++) {
      char ch = *v;
      if (ch == divider
	  || ch == bus_brkt_left
	  || ch == bus_brkt_right
	  || ch == escape)
	// Escape dividers, bus brackets and escapes.
	vstringAppend(sta_name, sta_name_end, s, escape);
      vstringAppend(sta_name, sta_name_end, s, ch);
      // Don't include the last character, which is always a space.
      if (v[2] == '\0')
	break;
    }
    vstringAppend(sta_name, sta_name_end, s, '\0');
    return sta_name;
  }
  else
    return verilog_name;
}

} // namespace
