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
#include "TokenParser.hh"

namespace sta {

TokenParser::TokenParser(const char *str, const char *delimiters) :
  delimiters_(delimiters),
  token_(const_cast<char*>(str)),
  first_(true)
{
  // Skip leading spaces.
  while (*token_ != '\0' && isspace(*token_))
    token_++;
  token_end_ = strpbrk(token_, delimiters_);
  if (token_end_) {
    // Save the delimiter.
    token_delimiter_ = *token_end_;
    // Replace the separator with a terminator.
    *token_end_ = '\0';
  }
}

bool
TokenParser::hasNext()
{
  if (!first_) {
    // Replace the previous separator.
    if (token_end_) {
      *token_end_ = token_delimiter_;
      token_ = token_end_ + 1;
      // Skip spaces.
      while (*token_ != '\0' && isspace(*token_))
	token_++;
      // Skip delimiters.
      while (*token_ != '\0' && strchr(delimiters_,*token_) != nullptr)
        token_++;
      if (*token_ == '\0')
	token_ = nullptr;
      else {
	token_end_ = strpbrk(token_, delimiters_);
	if (token_end_) {
	  // Save the delimiter.
	  token_delimiter_ = *token_end_;
	  // Replace the separator with a terminator.
	  *token_end_ = '\0';
	}
      }
    }
    else
      token_ = nullptr;
  }
  return token_ != nullptr;
}

char *
TokenParser::next()
{
  first_ = false;
  return token_;
}

} // namespace
