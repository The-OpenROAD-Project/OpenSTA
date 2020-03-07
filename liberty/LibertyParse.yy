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
	
#include <stdlib.h>
#include <string.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "LibertyParser.hh"

int LibertyLex_lex();
#define LibertyParse_lex LibertyLex_lex
// Use yacc generated parser errors.
#define YYERROR_VERBOSE

%}

%union {
  char *string;
  float number;
  int line;
  sta::LibertyAttrValue *attr_value;
  sta::LibertyAttrValueSeq *attr_values;
  sta::LibertyGroup *group;
  sta::LibertyStmt *stmt;
}

%token <number> FLOAT
%token <string> STRING KEYWORD

%type <stmt> statement complex_attr simple_attr variable group file
%type <attr_values> attr_values
%type <attr_value> attr_value simple_attr_value
%type <string> string
%type <line> line
%type <number> volt_expr volt_token

%start file

%{
%}

%%

file:
	group
	;

group:
	KEYWORD '(' ')' line '{'
	{ sta::libertyGroupBegin($1, NULL, $4); }
	'}' semi_opt
	{ $$ = sta::libertyGroupEnd(); }
|	KEYWORD '(' ')' line '{'
	{ sta::libertyGroupBegin($1, NULL, $4); }
	statements '}' semi_opt
	{ $$ = sta::libertyGroupEnd(); }
|	KEYWORD '(' attr_values ')' line '{'
	{ sta::libertyGroupBegin($1, $3, $5); }
	'}' semi_opt
	{ $$ = sta::libertyGroupEnd(); }
|	KEYWORD '(' attr_values ')' line '{'
	{ sta::libertyGroupBegin($1, $3, $5); }
	statements '}' semi_opt
	{ $$ = sta::libertyGroupEnd(); }
	;

line: /* empty */
	{ $$ = sta::libertyLine(); }
	;

statements:
	statement
|	statements statement
	;

statement:
	simple_attr
|	complex_attr
|	group
|	variable
	;

simple_attr:
	KEYWORD ':' simple_attr_value line semi_opt
	{ $$ = sta::makeLibertySimpleAttr($1, $3, $4); }
	;

simple_attr_value:
	attr_value
|	volt_expr
	{ $$ = static_cast<sta::LibertyAttrValue*>(NULL); }
/* Unquoted NOT function. */
/* clocked_on : !CP; */
|	'!' string
	{ $$ = sta::makeLibertyStringAttrValue(sta::stringPrint("!%s", $2)); sta::stringDelete($2); }
	;

complex_attr:
	KEYWORD '(' ')' line semi_opt
	{ $$ = sta::makeLibertyComplexAttr($1, NULL, $4); }
|	KEYWORD '(' attr_values ')' line semi_opt
	{ $$ = sta::makeLibertyComplexAttr($1, $3, $5); }
	;

attr_values:
	attr_value
	{ $$ = new sta::LibertyAttrValueSeq;
	  $$->push_back($1);
	}
|	attr_values ',' attr_value
        { $1->push_back($3);
	  $$ = $1;
	}
|	attr_values attr_value
        { $1->push_back($2);
	  $$ = $1;
	}
	;

variable:
	string '=' FLOAT line semi_opt
	{ $$ = sta::makeLibertyVariable($1, $3, $4); }
	;

string:
	STRING
	{ $$ = $1; }
|	KEYWORD
	{ $$ = $1; }
	;

attr_value:
	FLOAT
	{ $$ = sta::makeLibertyFloatAttrValue($1); }
	| string
	{ $$ = sta::makeLibertyStringAttrValue($1); }
	;

/* Voltage expressions are ignored. */
volt_expr:
	volt_token expr_op volt_token
|	volt_expr expr_op volt_token
	;

volt_token:
	FLOAT
|	KEYWORD
	{ sta::stringDelete($1);
	  $$ = 0.0;
	}
	;

expr_op:
	'+'
|	'-'
|	'*'
|	'/'
	;

semi_opt:
	/* empty */
|	semi_opt ';'
	;

%%
