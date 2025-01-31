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

#ifndef __FLEX_LEXER_H
#undef yyFlexLexer
#define yyFlexLexer LibertyFlexLexer
#include <FlexLexer.h>
#endif

#include "LibertyLocation.hh"
#include "LibertyParse.hh"

namespace sta {

class Report;
class LibertyParser;

class LibertyScanner : public LibertyFlexLexer
{
public:
  LibertyScanner(std::istream *stream,
                 const char *filename,
                 LibertyParser *reader,
                 Report *report);
  virtual ~LibertyScanner() {}

  virtual int lex(LibertyParse::semantic_type *const yylval,
                  LibertyParse::location_type *yylloc);
  // YY_DECL defined in LibertyLex.ll
  // Method body created by flex in LibertyLex.cc

  // Get rid of override virtual function warning.
  using FlexLexer::yylex;

private:
  bool includeBegin();
  void fileEnd();
  void error(const char *msg);

  std::istream *stream_;
  string filename_;
  LibertyParser *reader_;
  Report *report_;
  string token_;

  // Previous lex state for include files.
  string filename_prev_;
  std::istream *stream_prev_;
};

} // namespace
