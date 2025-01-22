// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#pragma once

#ifndef __FLEX_LEXER_H
#include <FlexLexer.h>
#endif

#include "location.hh"
#include "VerilogParse.hh"

namespace sta {

class Report;

class VerilogScanner : public yyFlexLexer
{
public:
  VerilogScanner(std::istream *stream,
                 const char *filename,
                 Report *report);
  virtual ~VerilogScanner() {}

  virtual int yylex(VerilogParse::semantic_type *const yylval,
                    VerilogParse::location_type *yylloc);
  // YY_DECL defined in VerilogLex.ll
  // Method body created by flex in VerilogLex.yy.cc

  void error(const char *msg);

  // Get rid of override virtual function warning.
  using FlexLexer::yylex;

private:
  const char *filename_;
  Report *report_;
};

} // namespace
