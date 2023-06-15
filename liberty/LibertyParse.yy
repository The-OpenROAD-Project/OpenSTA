%{
// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include <cstdlib>
#include <cstring>

#include "StringUtil.hh"
#include "liberty/LibertyParser.hh"

int LibertyLex_lex();
#define LibertyParse_lex LibertyLex_lex
// Use yacc generated parser errors.
#define YYERROR_VERBOSE

#define YYDEBUG 1

%}

%union {
  char *string;
  float number;
  int line;
  char ch;
  sta::LibertyAttrValue *attr_value;
  sta::LibertyAttrValueSeq *attr_values;
  sta::LibertyGroup *group;
  sta::LibertyStmt *stmt;
}

%left '+' '-' '|'
%left '*' '/' '&'
%left '^'
%left '!'

%token <number> FLOAT
%token <string> STRING KEYWORD

%type <stmt> statement complex_attr simple_attr variable group file
%type <attr_values> attr_values
%type <attr_value> attr_value
%type <string> string expr expr_term expr_term1 volt_expr
%type <line> line
%type <ch> expr_op volt_op

%expect 2

%start file

%{
%}

%%

file:
	group
	;

group:
	KEYWORD '(' ')' line '{'
	{ sta::libertyGroupBegin($1, nullptr, $4); }
	'}' semi_opt
	{ $$ = sta::libertyGroupEnd(); }
|	KEYWORD '(' ')' line '{'
	{ sta::libertyGroupBegin($1, nullptr, $4); }
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
	KEYWORD ':' attr_value line semi_opt
	{ $$ = sta::makeLibertySimpleAttr($1, $3, $4); }
	;

complex_attr:
	KEYWORD '(' ')' line semi_opt
	{ $$ = sta::makeLibertyComplexAttr($1, nullptr, $4); }
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
|       expr
	{ $$ = sta::makeLibertyStringAttrValue($1); }
|	volt_expr
	{ $$ = sta::makeLibertyStringAttrValue($1); }
	;

/* Voltage expressions are ignored. */
/* Crafted to avoid conflicts with expr */
volt_expr:
        FLOAT volt_op FLOAT
	{ $$ = sta::stringPrint("%e%c%e", $1, $2, $3); }
|       string volt_op FLOAT
	{ $$ = sta::stringPrint("%s%c%e", $1, $2, $3);
          sta::stringDelete($1);
        }
|       FLOAT volt_op string
	{ $$ = sta::stringPrint("%e%c%s", $1, $2, $3);
          sta::stringDelete($3);
        }
|       volt_expr volt_op FLOAT
	{ $$ = sta::stringPrint("%s%c%e", $1, $2, $3);
          sta::stringDelete($1);
        }
        ;

volt_op:
	'+'
        { $$ = '+'; }
|	'-'
        { $$ = '-'; }
|	'*'
        { $$ = '*'; }
|	'/'
        { $$ = '/'; }
	;

expr:
        expr_term1
|	expr_term1 expr_op expr
	{ $$ = sta::stringPrint("%s%c%s", $1, $2, $3);
          sta::stringDelete($1);
          sta::stringDelete($3);
        }
	;

expr_term:
	string
|	'0'
	{ $$ = sta::stringPrint("0"); }
|	'1'
	{ $$ = sta::stringPrint("1"); }
|	'(' expr ')'
	{ $$ = sta::stringPrint("(%s)", $2);
          sta::stringDelete($2);
        }
	;

expr_term1:
	expr_term
|       '!' expr_term
	{ $$ = sta::stringPrint("!%s", $2);
          sta::stringDelete($2);
        }
|	expr_term '\''
	{ $$ = sta::stringPrint("%s'", $1);
          sta::stringDelete($1);
        }
	;

expr_op:
	'+'
        { $$ = '+'; }
|	'|'
        { $$ = '|'; }
|	'*'
        { $$ = '*'; }
|	'&'
        { $$ = '&'; }
|	'^'
        { $$ = '^'; }
	;

semi_opt:
	/* empty */
|	semi_opt ';'
	;

%%
