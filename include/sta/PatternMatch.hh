// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <string>
#include <string_view>

#include "Error.hh"

// Don't require all of tcl.h.
using Tcl_RegExp = struct Tcl_RegExp_ *;
using Tcl_Interp = struct Tcl_Interp;

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
  PatternMatch(std::string_view pattern,
               bool is_regexp,
               bool nocase,
               Tcl_Interp *interp);
  // Use unix glob style matching.
  PatternMatch(std::string_view pattern);
  PatternMatch(std::string_view pattern,
               const PatternMatch *inherit_from);
  bool match(std::string_view str) const;
  bool matchNoCase(std::string_view str) const;
  const std::string &pattern() const { return pattern_; }
  bool isRegexp() const { return is_regexp_; }
  bool nocase() const { return nocase_; }
  Tcl_Interp *tclInterp() const { return interp_; }
  bool hasWildcards() const;

private:
  void compileRegexp();

  std::string pattern_;
  bool is_regexp_;
  bool nocase_;
  Tcl_Interp *interp_;
  Tcl_RegExp regexp_;
};

// Error thrown by Pattern constructor.
class RegexpCompileError : public Exception
{
public:
  RegexpCompileError(std::string_view pattern);
  virtual ~RegexpCompileError() noexcept {}
  virtual const char *what() const noexcept;

private:
  std::string error_;
};

// Simple pattern match
// '*' matches zero or more characters
// '?' matches any character
bool
patternMatch(std::string_view pattern,
             std::string_view str);
bool
patternMatchNoCase(std::string_view pattern,
                   std::string_view str,
                   bool nocase);
// Predicate to find out if there are wildcard characters in the pattern.
bool
patternWildcards(std::string_view pattern);

} // namespace
