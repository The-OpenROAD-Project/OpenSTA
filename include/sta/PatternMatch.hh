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
#include "Error.hh"

// Don't require all of tcl.h.
typedef struct Tcl_RegExp_ *Tcl_RegExp;
typedef struct Tcl_Interp Tcl_Interp;

namespace sta {

using ::Tcl_RegExp;
using ::Tcl_Interp;

class PatternMatch
{
public:
  // If regexp is false, use unix glob style matching.
  // If regexp is true, use TCL regular expression matching.
  //   Regular expressions are always anchored.
  // If nocase is true, ignore case in the pattern.
  // Tcl_Interp is optional for reporting regexp compile errors.
  PatternMatch(const char *pattern,
	       bool is_regexp,
	       bool nocase,
	       Tcl_Interp *interp);
  // Use unix glob style matching.
  PatternMatch(const char *pattern);
  PatternMatch(const char *pattern,
	       const PatternMatch *inherit_from);
  bool match(const char *str) const;
  bool matchNoCase(const char *str) const;
  const char *pattern() const { return pattern_; }
  bool isRegexp() const { return is_regexp_; }
  bool nocase() const { return nocase_; }
  Tcl_Interp *tclInterp() const { return interp_; }
  bool hasWildcards() const;

private:
  DISALLOW_COPY_AND_ASSIGN(PatternMatch);
  void compileRegexp();

  const char *pattern_;
  bool is_regexp_;
  bool nocase_;
  Tcl_Interp *interp_;
  Tcl_RegExp regexp_;
};

// Error thrown by Pattern constructor.
class RegexpCompileError : public Exception
{
public:
  explicit RegexpCompileError(const char *error);
  virtual ~RegexpCompileError() noexcept {}
  virtual const char *what() const noexcept;

private:
  const char *error_;
};

// Simple pattern match
// '*' matches zero or more characters
// '?' matches any character
bool
patternMatch(const char *pattern,
	     const char *str);
bool
patternMatchNoCase(const char *pattern,
		   const char *str,
		   bool nocase);
// Predicate to find out if there are wildcard characters in the pattern.
bool
patternWildcards(const char *pattern);

} // namespace
