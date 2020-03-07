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

#include <string.h>
#include <stdlib.h>
#include <string>
#include "Machine.hh"
#include "StringUtil.hh"
#include "ParseBus.hh"

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
	     char escape,
	     // Return values.
	     char *&bus_name,
	     int &index)
{
  const char brkts_left[2] = {brkt_left, '\0'};
  const char brkts_right[2] = {brkt_right, '\0'};
  parseBusName(name, brkts_left, brkts_right, escape, bus_name, index);
}

void
parseBusName(const char *name,
	     const char *brkts_left,
	     const char *brkts_right,
	     char escape,
	     // Return values.
	     char *&bus_name,
	     int &index)
{
  bus_name = nullptr;
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
	size_t bus_name_len = left - name;
	bus_name = new char[bus_name_len + 1];
	strncpy(bus_name, name, bus_name_len);
	bus_name[bus_name_len] = '\0';
	// Simple bus subscript.
	index = atoi(left + 1);
      }
    }
  }
}

void
parseBusRange(const char *name,
	      const char brkt_left,
	      const char brkt_right,
	      char escape,
	      // Return values.
	      char *&bus_name,
	      int &from,
	      int &to)
{
  const char brkts_left[2] = {brkt_left, '\0'};
  const char brkts_right[2] = {brkt_right, '\0'};
  parseBusRange(name, brkts_left, brkts_right, escape, bus_name, from, to);
}

void
parseBusRange(const char *name,
	      const char *brkts_left,
	      const char *brkts_right,
	      char escape,
	      // Return values.
	      char *&bus_name,
	      int &from,
	      int &to)
{
  bus_name = nullptr;
  size_t len = strlen(name);
  // Shortest bus range is a[1:0].
  if (len >= 6
      // Escaped bus brackets are not buses.
      && name[len - 2] != escape) {
    char last_ch = name[len - 1];
    const char *brkt_right_ptr = strchr(brkts_right, last_ch);
    if (brkt_right_ptr) {
      size_t brkt_index = brkt_right_ptr - brkts_right;
      char brkt_left = brkts_left[brkt_index];
      const char *left = strrchr(name, brkt_left);
      if (left) {
	// Check for bus range.
	const char range_sep = ':';
	const char *range = strchr(name, range_sep);
	if (range) {
	  size_t bus_name_len = left - name;
	  bus_name = new char[bus_name_len + 1];
	  strncpy(bus_name, name, bus_name_len);
	  bus_name[bus_name_len] = '\0';
	  // No need to terminate bus subscript because atoi stops
	  // scanning at first non-digit character.
	  from = atoi(left + 1);
	  to = atoi(range + 1);
	}
      }
    }
  }
}

const char *
escapeChars(const char *token,
	    char ch1,
	    char ch2,
	    char escape)
{
  int result_length = 0;
  bool need_escapes = false;
  for (const char *s = token; *s; s++) {
    char ch = *s;
    if (ch == escape) {
      char next_ch = s[1];
      // Make sure we don't skip the null if escape is the last char.
      if (next_ch != '\0')
	result_length++;
    }
    else if (ch == ch1 || ch == ch2) {
      result_length++;
      need_escapes = true;
    }
    result_length++;
  }
  if (need_escapes) {
    char *result = makeTmpString(result_length + 1);
    char *r = result;
    for (const char *s = token; *s; s++) {
      char ch = *s;
      if (ch == escape) {
	char next_ch = s[1];
	// Make sure we don't skip the null if escape is the last char.
	if (next_ch != '\0') {
	  *r++ = ch;
	  *r++ = next_ch;
	  s++;
	}
      }
      else if (ch == ch1 || ch == ch2) {
	*r++ = escape;
	*r++ = ch;
      }
      else
	*r++ = ch;
    }
    *r++ = '\0';
    return result;
  }
  else
    return token;
}

} // namespace
