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

#include "ParseBus.hh"

#include <cstring>
#include <cstdlib>
#include <string>

#include "StringUtil.hh"

namespace sta {

using std::string;

bool
isBusName(const char *name,
	  const char brkt_left,
	  const char brkt_right,
	  char escape)
{
  size_t len = strlen(name);
  // Shortest bus name is a[0].
  if (len >= 4
      // Escaped bus brackets are not buses.
      && name[len - 2] != escape
      && name[len - 1] == brkt_right) {
    const char *left = strrchr(name, brkt_left);
    return left != nullptr;
  }
  else
    return false;
}

void
parseBusName(const char *name,
	     const char brkt_left,
	     const char brkt_right,
	     const char escape,
	     // Return values.
	     bool &is_bus,
	     string &bus_name,
	     int &index)
{
  const char brkts_left[2] = {brkt_left, '\0'};
  const char brkts_right[2] = {brkt_right, '\0'};
  parseBusName(name, brkts_left, brkts_right, escape,
               is_bus, bus_name, index);
}

void
parseBusName(const char *name,
	     const char *brkts_left,
	     const char *brkts_right,
	     char escape,
	     // Return values.
	     bool &is_bus,
	     string &bus_name,
	     int &index)
{
  is_bus = false;
  size_t len = strlen(name);
  // Shortest bus name is a[0].
  if (len >= 4
      // Escaped bus brackets are not buses.
      && name[len - 2] != escape) {
    char last_ch = name[len - 1];
    const char *brkt_right_ptr = strchr(brkts_right, last_ch);
    if (brkt_right_ptr) {
      size_t brkt_index = brkt_right_ptr - brkts_right;
      char brkt_left = brkts_left[brkt_index];
      const char *left = strrchr(name, brkt_left);
      if (left) {
        is_bus = true;
	size_t bus_name_len = left - name;
	bus_name.append(name, bus_name_len);
	// Simple bus subscript.
	index = atoi(left + 1);
      }
    }
  }
}

void
parseBusName(const char *name,
             const char brkt_left,
             const char brkt_right,
             char escape,
             // Return values.
             bool &is_bus,
             bool &is_range,
             string &bus_name,
             int &from,
             int &to,
             bool &subscript_wild)
{
  const char brkts_left[2] = {brkt_left, '\0'};
  const char brkts_right[2] = {brkt_right, '\0'};
  parseBusName(name, brkts_left, brkts_right, escape,
               is_bus, is_range, bus_name, from, to, subscript_wild);
}

void
parseBusName(const char *name,
             const char *brkts_left,
             const char *brkts_right,
             char escape,
             // Return values.
             bool &is_bus,
             bool &is_range,
             string &bus_name,
             int &from,
             int &to,
             bool &subscript_wild)
{
  is_bus = false;
  is_range = false;
  subscript_wild = false;
  size_t len = strlen(name);
  // Shortest bus is a[0].
  if (len >= 4
      // Escaped bus brackets are not buses.
      && name[len - 2] != escape) {
    char last_ch = name[len - 1];
    const char *brkt_right_ptr = strchr(brkts_right, last_ch);
    if (brkt_right_ptr) {
      size_t brkt_index = brkt_right_ptr - brkts_right;
      char brkt_left = brkts_left[brkt_index];
      const char *left = strrchr(name, brkt_left);
      if (left) {
        is_bus = true;
	// Check for bus range.
	const char range_sep = ':';
	const char *range = strchr(name, range_sep);
	if (range) {
          is_range = true;
          bus_name.append(name, left - name);
	  // No need to terminate bus subscript because atoi stops
	  // scanning at first non-digit character.
	  from = atoi(left + 1);
	  to = atoi(range + 1);
	}
        else {
          bus_name.append(name, left - name);
	  if (left[1] == '*')
            subscript_wild = true;
          else
            from = to = atoi(left + 1);
        }
      }
    }
  }
}

string
escapeChars(const char *token,
	    const char ch1,
	    const char ch2,
	    const char escape)
{
  string escaped;
  for (const char *s = token; *s; s++) {
    char ch = *s;
    if (ch == escape) {
      char next_ch = s[1];
      // Make sure we don't skip the null if escape is the last char.
      if (next_ch != '\0') {
        escaped += ch;
        escaped += next_ch;
        s++;
      }
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

} // namespace
