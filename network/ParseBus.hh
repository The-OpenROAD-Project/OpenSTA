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

#pragma once

namespace sta {

// Return true if name is a bus.
bool
isBusName(const char *name,
	  const char brkt_left,
	  const char brkt_right,
	  char escape);

// Parse name as a bus.
// signal
//  bus_name = nullptr
// bus[bit]
//  bus_name = "bus"
//  index = bit
// Caller must delete returned bus_name string.
void
parseBusName(const char *name,
	     const char brkt_left,
	     const char brkt_right,
	     char escape,
	     // Return values.
	     char *&bus_name,
	     int &index);
// Allow multiple different left/right bus brackets.
void
parseBusName(const char *name,
	     const char *brkts_left,
	     const char *brkts_right,
	     char escape,
	     // Return values.
	     char *&bus_name,
	     int &index);

// Parse a bus range, such as BUS[4:0].
// bus_name is set to null if name is not a range.
// Caller must delete returned bus_name string.
void
parseBusRange(const char *name,
	      const char brkt_left,
	      const char brkt_right,
	      char escape,
	      // Return values.
	      char *&bus_name,
	      int &from,
	      int &to);
// brkt_lefts and brkt_rights are corresponding strings of legal
// bus brackets such as "[(<" and "])>".
void
parseBusRange(const char *name,
	      const char *brkts_left,
	      const char *brkts_right,
	      char escape,
	      // Return values.
	      char *&bus_name,
	      int &from,
	      int &to);

// Insert escapes before ch1 and ch2 in token.
const char *
escapeChars(const char *token,
	    char ch1,
	    char ch2,
	    char escape);

} // namespace
