%{

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

// Liberty function expression parser.

#include "Machine.hh"
#include "FuncExpr.hh"
#include "LibertyExpr.hh"
#include "LibertyExprPvt.hh"

int LibertyExprLex_lex();
#define LibertyExprParse_lex LibertyExprLex_lex

%}

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

%{
%}

%%

result_expr:
	expr	{ sta::libexpr_parser->setResult($1); }
|	expr ';'{ sta::libexpr_parser->setResult($1); }
;

terminal:
	PORT		{ $$ = sta::libexpr_parser->makeFuncExprPort($1); }
|	'0'		{ $$ = sta::FuncExpr::makeZero(); }
|	'1'		{ $$ = sta::FuncExpr::makeOne(); }
|	'(' expr ')'	{ $$ = $2; }
;

terminal_expr:
	terminal
|	'!' terminal	{ $$ = sta::libexpr_parser->makeFuncExprNot($2); }
|	terminal '\''	{ $$ = sta::libexpr_parser->makeFuncExprNot($1); }
;

implicit_and:
	terminal_expr terminal_expr
	{ $$ = sta::libexpr_parser->makeFuncExprAnd($1, $2); }
|	implicit_and terminal_expr
	{ $$ = sta::libexpr_parser->makeFuncExprAnd($1, $2); }
;

expr:
	terminal_expr
|	implicit_and
|	expr '+' expr	{ $$ = sta::libexpr_parser->makeFuncExprOr($1, $3); }
|	expr '|' expr	{ $$ = sta::libexpr_parser->makeFuncExprOr($1, $3); }
|	expr '*' expr   { $$ = sta::libexpr_parser->makeFuncExprAnd($1, $3); }
|	expr '&' expr   { $$ = sta::libexpr_parser->makeFuncExprAnd($1, $3); }
|	expr '^' expr	{ $$ = sta::libexpr_parser->makeFuncExprXor($1, $3); }
;

%%
