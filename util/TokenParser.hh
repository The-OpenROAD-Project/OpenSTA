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

#include "DisallowCopyAssign.hh"

namespace sta {

// Iterate over the tokens in str separated by character sep.
// Similar in functionality to strtok, but does not leave the string
// side-effected.  This is preferable to using strtok because it leaves
// string terminators where the separators were.
// Using STL string functions to parse tokens is messy and extremely slow
// on the RogueWave/Solaris implementation, apparently because of mutexes
// on temporary strings.
class TokenParser
{
public:
  TokenParser(const char *str,
	      const char *delimiters);
  bool hasNext();
  char *next();

private:
  DISALLOW_COPY_AND_ASSIGN(TokenParser);

  const char *delimiters_;
  char *token_;
  char *token_end_;
  char token_delimiter_;
  bool first_;
};

} // namespace
