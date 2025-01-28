%{
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

#include "util/FlexDisableRegister.hh"
#include "VerilogNamespace.hh"
#include "VerilogReader.hh"
#include "verilog/VerilogReaderPvt.hh"
#include "VerilogParse.hh"
#include "verilog/VerilogScanner.hh"

#undef YY_DECL
#define YY_DECL \
int \
sta::VerilogScanner::lex(sta::VerilogParse::semantic_type *const yylval, \
                         sta::VerilogParse::location_type *loc)

// update location on matching
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);

typedef sta::VerilogParse::token token;
%}

%option c++
%option yyclass="sta::VerilogScanner"
%option prefix="Verilog"
%option noyywrap
%option never-interactive
%option stack
%option yylineno
/* %option debug */

%x COMMENT
%x QSTRING

SIGN	"+"|"-"
UNSIGNED_NUMBER [0-9][0-9_]*
BLANK	[ \t\r]
EOL	\r?\n
ID_ESCAPED_TOKEN \\[^ \t\r\n]+[\r\n\t ]
ID_ALPHA_TOKEN [A-Za-z_][A-Za-z0-9_$]*
ID_TOKEN {ID_ESCAPED_TOKEN}|{ID_ALPHA_TOKEN}

%%

^[ \t]*`.*{EOL} { /* Macro definition. */
	loc->lines();
        loc->step();
	}

"//"[^\n]*{EOL} { /* Single line comment. */
	loc->lines();
        loc->step();
	}

"/*"	{ BEGIN COMMENT; }
<COMMENT>{
.

{EOL}	{
        loc->lines();
        loc->step();
        }

"*/"	{ BEGIN INITIAL; }

<<EOF>> {
	error("unterminated comment");
	BEGIN(INITIAL);
	yyterminate();
	}
}

{SIGN}?{UNSIGNED_NUMBER}?"'"[sS]?[bB][01_xz]+ {
  yylval->constant = new string(yytext);
  return token::CONSTANT;
}

{SIGN}?{UNSIGNED_NUMBER}?"'"[sS]?[oO][0-7_xz]+ {
  yylval->constant = new string(yytext);
  return token::CONSTANT;
}

{SIGN}?{UNSIGNED_NUMBER}?"'"[sS]?[dD][0-9_]+ {
  yylval->constant = new string(yytext);
  return token::CONSTANT;
}

{SIGN}?{UNSIGNED_NUMBER}?"'"[sS]?[hH][0-9a-fA-F_xz]+ {
  yylval->constant = new string(yytext);
  return token::CONSTANT;
}

{SIGN}?[0-9]+ {
  yylval->ival = atol(yytext);
  return token::INT;
}

":"|"."|"{"|"}"|"["|"]"|","|"*"|";"|"="|"-"|"+"|"|"|"("|")" {
  return ((int) yytext[0]);
}

"(*" { return token::ATTR_OPEN; }
"*)" { return token::ATTR_CLOSED; }
assign { return token::ASSIGN; }
endmodule { return token::ENDMODULE; }
inout { return token::INOUT; }
input { return token::INPUT; }
module { return token::MODULE; }
output { return token::OUTPUT; }
parameter { return token::PARAMETER; }
defparam { return token::DEFPARAM; }
reg { return token::REG; }
supply0 { return token::SUPPLY0; }
supply1 { return token::SUPPLY1; }
tri { return token::TRI; }
wand { return token::WAND; }
wire { return token::WIRE; }
wor { return token::WOR; }

{ID_TOKEN}("."{ID_TOKEN})* {
	yylval->string = new string(yytext, yyleng);
	return token::ID;
}

{EOL}	{
        loc->lines();
        loc->step();
        }

{BLANK}	{ /* ignore blanks */ }

\"	{
	yylval->string = new string;
	BEGIN(QSTRING);
	}

<QSTRING>\" {
	BEGIN(INITIAL);
	return token::STRING;
	}

<QSTRING>{EOL} {
	error("unterminated quoted string");
	BEGIN(INITIAL);
	return token::STRING;
	}

<QSTRING>\\{EOL} {
	/* Line continuation. */
        loc->lines();
        loc->step();
	}

<QSTRING>[^\r\n\"]+ {
	/* Anything return token::or double quote */
	*yylval->string += yytext;
	}

<QSTRING><<EOF>> {
	error("unterminated string constant");
	BEGIN(INITIAL);
	yyterminate();
	}

	/* Send out of bound characters to parser. */
.	{ return (int) yytext[0]; }

%%
