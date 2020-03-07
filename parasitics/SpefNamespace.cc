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
#include <string.h>
#include "Machine.hh"

namespace sta {

char *
spefToSta(const char *token, char spef_divider,
	  char path_divider, char path_escape)
{
  const char spef_escape = '\\';
  char *trans_token = new char[strlen(token) + 1];
  char *t = trans_token;

  for (const char *s = token; *s ; s++) {
    char ch = *s;
    if (ch == spef_escape) {
      char next_ch = s[1];
      if (next_ch == spef_divider) {
	// Translate spef escape to network escape.
	*t++ = path_escape;
	// Translate spef divider to network divider.
	*t++ = path_divider;
      }
      else if (next_ch == '['
	       || next_ch == ']'
	       || next_ch == spef_escape) {
	// Translate spef escape to network escape.
	*t++ = path_escape;
	*t++ = next_ch;
      }
      else
	// No need to escape other characters.
	*t++ = next_ch;
      s++;
    }
    else if (ch == spef_divider)
      // Translate spef divider to network divider.
      *t++ = path_divider;
    else
      // Just the normal noises.
      *t++ = ch;
  }
  *t++ = '\0';
  return trans_token;
}

char *
staToSpef(const char *token, char spef_divider,
	  char path_divider, char path_escape)
{
  const char spef_escape = '\\';
  char *trans_token = new char[strlen(token) + 1];
  char *t = trans_token;

  for (const char *s = token; *s ; s++) {
    char ch = *s;
    if (ch == path_escape) {
      char next_ch = s[1];
      if (next_ch == path_divider) {
	// Translate network escape to spef escape.
	*t++ = spef_escape;
	// Translate network divider to spef divider.
	*t++ = spef_divider;
      }
      else if (next_ch == '['
	       || next_ch == ']') {
	// Translate network escape to spef escape.
	*t++ = spef_escape;
	*t++ = next_ch;
      }
      else
	// No need to escape other characters.
	*t++ = next_ch;
      s++;
    }
    else if (ch == path_divider)
      // Translate network divider to spef divider.
      *t++ = spef_divider;
    else if (!(isdigit(ch) || isalpha(ch) || ch == '_')) {
      // Escape non-alphanum characters.
      *t++ = spef_escape;
      *t++ = ch;
    }
    else
      // Just the normal noises.
      *t++ = ch;
  }
  *t++ = '\0';
  return trans_token;
}

} // namespace
