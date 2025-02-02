// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "LibExprParse.hh"

#ifndef __FLEX_LEXER_H
#undef yyFlexLexer
#define yyFlexLexer LibExprFlexLexer
#include <FlexLexer.h>
#endif

namespace sta {

using std::string;

class Report;
class LibExprParse;

class LibExprScanner : public LibExprFlexLexer
{
public:
  LibExprScanner(std::istringstream &stream);
  virtual ~LibExprScanner() {}

  virtual int lex(LibExprParse::semantic_type *const yylval);
  // YY_DECL defined in LibertyLex.ll
  // Method body created by flex in LibertyLex.cc

  // Get rid of override virtual function warning.
  using FlexLexer::yylex;

private:
  Report *report_;
  string token_;
};

} // namespace
