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
#include <string>
#include <ctype.h>
#include <tcl.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "PatternMatch.hh"

namespace sta {

using std::string;

PatternMatch::PatternMatch(const char *pattern,
			   bool is_regexp,
			   bool nocase,
			   Tcl_Interp *interp) :
  pattern_(pattern),
  is_regexp_(is_regexp),
  nocase_(nocase),
  interp_(interp),
  regexp_(nullptr)
{
  if (is_regexp_)
    compileRegexp();
}

PatternMatch::PatternMatch(const char *pattern) :
  pattern_(pattern),
  is_regexp_(false),
  nocase_(false),
  interp_(nullptr),
  regexp_(nullptr)
{
}

PatternMatch::PatternMatch(const char *pattern,
			   const PatternMatch *inherit_from) :
  pattern_(pattern),
  is_regexp_(inherit_from->is_regexp_),
  nocase_(inherit_from->nocase_),
  interp_(inherit_from->interp_),
  regexp_(nullptr)
{
  if (is_regexp_)
    compileRegexp();
}

void
PatternMatch::compileRegexp()
{
  int flags = TCL_REG_ADVANCED;
  if (nocase_)
    flags |= TCL_REG_NOCASE;
  string anchored_pattern;
  anchored_pattern += '^';
  anchored_pattern += pattern_;
  anchored_pattern += '$';
  Tcl_Obj *pattern_obj = Tcl_NewStringObj(anchored_pattern.c_str(),
					  anchored_pattern.size());
  regexp_ = Tcl_GetRegExpFromObj(interp_, pattern_obj, flags);
  if (regexp_ == nullptr && interp_)
    throw RegexpCompileError(pattern_);
}

static bool
regexpWildcards(const char *pattern)
{
  return strpbrk(pattern, ".+*?[]") != nullptr;
}

bool
PatternMatch::hasWildcards() const
{
  if (is_regexp_)
    return regexpWildcards(pattern_);
  else
    return patternWildcards(pattern_);
}

bool
PatternMatch::match(const char *str) const
{
  if (regexp_)
    return Tcl_RegExpExec(nullptr, regexp_, str, str) == 1;
  else
    return patternMatch(pattern_, str);
}

bool
PatternMatch::matchNoCase(const char *str) const
{
  if (regexp_)
    return Tcl_RegExpExec(0, regexp_, str, str) == 1;
  else
    return patternMatchNoCase(pattern_, str, nocase_);
}

////////////////////////////////////////////////////////////////

RegexpCompileError::RegexpCompileError(const char *pattern)  :
  Exception()
{
  error_ = stringPrint("TCL failed to compile regular expression '%s'.", pattern);
}

const char *
RegexpCompileError::what() const noexcept
{
  return error_;
}

////////////////////////////////////////////////////////////////

bool
patternMatch(const char *pattern,
	     const char *str)
{
  const char *p = pattern;
  const char *s = str;

  while (*p && *s && (*s == *p || *p == '?')) {
    p++;
    s++;
  }
  if (*p == '\0' && *s == '\0')
    return true;
  else if (*p == '*') {
    if (p[1] == '\0')
      return true;
    while (*s) {
      if (patternMatch(p + 1, s))
	return true;
      s++;
    }
  }
  return false;
}

inline
bool equalCase(char s,
	       char p,
	       bool nocase)
{
  return nocase
    ? tolower(s) == tolower(p)
    : s == p;
}

bool
patternMatchNoCase(const char *pattern,
		   const char *str,
		   bool nocase)
{
  const char *p = pattern;
  const char *s = str;

  while (*p && *s && (equalCase(*s, *p, nocase) || *p == '?')) {
    p++;
    s++;
  }
  if (*p == '\0' && *s == '\0')
    return true;
  else if (*p == '*') {
    if (p[1] == '\0')
      return true;
    while (*s) {
      if (patternMatchNoCase(p + 1, s, nocase))
	return true;
      s++;
    }
  }
  return false;
}

bool
patternWildcards(const char *pattern)
{
  return strpbrk(pattern, "*?") != 0;
}

} // namespace
