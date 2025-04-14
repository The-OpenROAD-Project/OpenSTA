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

// Liberty function expression parser.

%{
#include "FuncExpr.hh"
#include "liberty/LibExprReader.hh"
#include "liberty/LibExprReaderPvt.hh"
#include "liberty/LibExprScanner.hh"

#undef yylex
#define yylex scanner->lex

// warning: variable 'yynerrs_' set but not used
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

void
sta::LibExprParse::error(const std::string &msg)
{
  reader->parseError(msg.c_str());
}

%}

%require  "3.2"
%skeleton "lalr1.cc"
%debug
%define api.namespace {sta}
%define parse.assert
%parse-param{LibExprScanner *scanner}
%parse-param{LibExprReader *reader}
%define api.parser.class {LibExprParse}

%union {
  int int_val;
  const char *string;
  sta::FuncExpr *expr;
}

%token PORT
%left '+' '|'
%left '*' '&'
%left '^'
%left '!' '\''

%type <string> PORT
%type <expr> expr terminal terminal_expr implicit_and

%%

result_expr:
	expr	{ reader->setResult($1); }
|	expr ';'{ reader->setResult($1); }
;

terminal:
	PORT		{ $$ = reader->makeFuncExprPort($1); }
|	'0'		{ $$ = sta::FuncExpr::makeZero(); }
|	'1'		{ $$ = sta::FuncExpr::makeOne(); }
|	'(' expr ')'	{ $$ = $2; }
;

terminal_expr:
	terminal
|	'!' terminal	{ $$ = reader->makeFuncExprNot($2); }
|	terminal '\''	{ $$ = reader->makeFuncExprNot($1); }
;

implicit_and:
	terminal_expr terminal_expr
	{ $$ = reader->makeFuncExprAnd($1, $2); }
|	implicit_and terminal_expr
	{ $$ = reader->makeFuncExprAnd($1, $2); }
;

expr:
	terminal_expr
|	implicit_and
|	expr '+' expr	{ $$ = reader->makeFuncExprOr($1, $3); }
|	expr '|' expr	{ $$ = reader->makeFuncExprOr($1, $3); }
|	expr '*' expr   { $$ = reader->makeFuncExprAnd($1, $3); }
|	expr '&' expr   { $$ = reader->makeFuncExprAnd($1, $3); }
|	expr '^' expr	{ $$ = reader->makeFuncExprXor($1, $3); }
;

%%
