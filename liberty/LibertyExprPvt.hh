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

class Report;
class LibertyCell;
class FuncExpr;

class LibExprParser
{
public:
  LibExprParser(const char *func,
		LibertyCell *cell,
		const char *error_msg,
		Report *report);
  ~LibExprParser();
  FuncExpr *makeFuncExprPort(const char *port_name);
  FuncExpr *makeFuncExprOr(FuncExpr *arg1,
			   FuncExpr *arg2);
  FuncExpr *makeFuncExprAnd(FuncExpr *arg1,
			    FuncExpr *arg2);
  FuncExpr *makeFuncExprXor(FuncExpr *arg1,
			    FuncExpr *arg2);
  FuncExpr *makeFuncExprNot(FuncExpr *arg);
  void setResult(FuncExpr *result);
  FuncExpr *result() { return result_; }
  void parseError(const char *msg);
  size_t copyInput(char *buf,
		   size_t max_size);
  void tokenStart();
  const char *token();
  char *tokenCopy();
  void tokenErase();
  void tokenAppend(char ch);

private:
  DISALLOW_COPY_AND_ASSIGN(LibExprParser);

  const char *func_;
  LibertyCell *cell_;
  const char *error_msg_;
  Report *report_;
  FuncExpr *result_;
  size_t token_length_;
  char *token_;
  char *token_next_;
};

extern LibExprParser *libexpr_parser;

} // namespace

// Global namespace

void
libertyExprFlushBuffer();
int
LibertyExprParse_error(const char *msg);
