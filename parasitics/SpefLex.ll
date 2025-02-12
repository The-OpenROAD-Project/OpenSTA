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

#include <string.h>
#include <string>

#include "util/FlexDisableRegister.hh"
#include "StringUtil.hh"
#include "parasitics/SpefReaderPvt.hh"
#include "SpefParse.hh"
#include "parasitics/SpefScanner.hh"

#undef YY_DECL
#define YY_DECL \
int \
sta::SpefScanner::lex(sta::SpefParse::semantic_type *const yylval, \
                      sta::SpefParse::location_type *loc)

// update location on matching
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);

typedef sta::SpefParse::token token;
%}

%option c++
%option yyclass="sta::SpefScanner"
%option prefix="Spef"
%option noyywrap
%option never-interactive
%option stack
%option yylineno
%option debug

%x COMMENT
%x QUOTE

ALPHA	[A-Za-z]
DIGIT	[0-9]
BLANK	[ \t\r]
POS_SIGN "+"
NEG_SIGN "-"
POS_INTEGER {DIGIT}+
SIGN	{POS_SIGN}|{NEG_SIGN}
INTEGER	{SIGN}?{DIGIT}+
DECIMAL	{SIGN}?{DIGIT}+"."{DIGIT}*
FRACTION {SIGN}?"."{DIGIT}+
RADIX	{DECIMAL}|{FRACTION}|{INTEGER}
EXP	{RADIX}[eE]{INTEGER}
FLOAT	{DECIMAL}|{FRACTION}|{EXP}

HCHAR	"."|"/"|"|"|":"
PREFIX_BUS_DELIM "["|"{"|"("|"<"
SUFFIX_BUS_DELIM "]"|"}"|")"|">"
SPECIAL_CHAR "!"|"#"|"$"|"%"|"&"|"`"|"("|")"|"*"|"+"|","|"-"|"."|"/"|":"|";"|"<"|"="|">"|"?"|"@"|"["|"\\"|"]"|"^"|"'"|"{"|"|"|"}"|"~"
ESCAPED_CHAR_SET {SPECIAL_CHAR}|\"
ESCAPED_CHAR \\{ESCAPED_CHAR_SET}
IDENT_ACHAR {ESCAPED_CHAR}|{ALPHA}|"_"
IDENT_CHAR {IDENT_ACHAR}|{DIGIT}
ID {IDENT_ACHAR}{IDENT_CHAR}*
BUS_SUB {DIGIT}|{ALPHA}|"_"
BIT_IDENT {ID}({PREFIX_BUS_DELIM}{BUS_SUB}+{SUFFIX_BUS_DELIM})+
ID_OR_BIT {ID}|{BIT_IDENT}
IDENT {INTEGER}*{ID_OR_BIT}({HCHAR}|{INTEGER}|{ID_OR_BIT})*
IDENT_OR_BIT {ID}|{BIT_IDENT}
PATH {IDENT_OR_BIT}({HCHAR}{IDENT_OR_BIT})*
NAME_PAIR ({PATH}|{INDEX}){HCHAR}({INDEX}|{IDENT_OR_BIT}|{POS_INTEGER})
INDEX   "*"{POS_INTEGER}

%%

"*BUS_DELIMITER" { return token::BUS_DELIMITER; }
"*C2_R1_C1" { return token::C2_R1_C1; }
"*C" { return token::KW_C; }
"*CAP" { return token::CAP; }
"*CELL" { return token::CELL; }
"*CONN" { return token::CONN; }
"*C_UNIT" { return token::C_UNIT; }
"*SPEF" { return token::SPEF; }
"*DATE" { return token::DATE; }
"*DEFINE" { return token::DEFINE; }
"*DELIMITER" { return token::DELIMITER; }
"*DESIGN" { return token::DESIGN; }
"*DESIGN_FLOW" { return token::DESIGN_FLOW; }
"*DIVIDER" { return token::DIVIDER; }
"*DRIVER" { return token::DRIVER; }
"*D_NET" { return token::D_NET; }
"*D_PNET" { return token::D_PNET; }
"*D" { return token::KW_D; }
"*END" { return token::END; }
"*GROUND_NETS" { return token::GROUND_NETS; }
"*INDUC" { return token::INDUC; }
"*I" { return token::KW_I; }
"*K" { return token::KW_K; }
"*L" { return token::KW_L; }
"*LOADS" { return token::LOADS; }
"*L_UNIT" { return token::L_UNIT; }
"*NAME_MAP" { return token::NAME_MAP; }
"*N" { return token::KW_N; }
"*PDEFINE" { return token::PDEFINE; }
"*PHYSICAL_PORTS" { return token::PHYSICAL_PORTS; }
"*PORTS" { return token::PORTS; }
"*POWER_NETS" { return token::POWER_NETS; }
"*PROGRAM" { return token::PROGRAM; }
"*P" { return token::KW_P; }
"*Q" { return token::KW_Q; }
"*RC" { return token::RC; }
"*RES" { return token::RES; }
"*R_NET" { return token::R_NET; }
"*R_PNET" { return token::R_PNET; }
"*R_UNIT" { return token::R_UNIT; }
"*S" { return token::KW_S; }
"*T_UNIT" { return token::T_UNIT; }
"*VENDOR" { return token::VENDOR; }
"*VERSION" { return token::PVERSION; }
"*V" { return token::KW_V; }

"//".*\n { loc->lines(); loc->step(); } /* Single line comment */

"/*"	{ BEGIN COMMENT; }
<COMMENT>{

.

\n	{ loc->lines(); loc->step(); }

"*/"	{ BEGIN INITIAL; }

<<EOF>> {
	error("unterminated comment");
	BEGIN(INITIAL);
	yyterminate();
	}
}

"\""	{ BEGIN QUOTE; token_.erase(); }
<QUOTE>{

\r?\n	{ loc->lines(); loc->step(); }

"\\".	{ token_ += yytext[1]; }

"\"" 	{
	BEGIN INITIAL;
	yylval->string = sta::stringCopy(token_.c_str());
	return token::QSTRING;
	}

.	{ token_ += yytext[0]; }

<<EOF>> {
	error("unterminated quoted string");
	BEGIN(INITIAL);
	yyterminate();
	}
}

{BLANK}*\n { loc->lines(); loc->step(); }

{INTEGER} {
	yylval->integer = atoi(yytext);
	return token::INTEGER;
	}

{FLOAT} {
	yylval->number = static_cast<float>(atof(yytext));
	return token::FLOAT;
	}

{IDENT} {
	yylval->string = reader_->translated(yytext);
	return token::IDENT;
	}

{PATH}|{NAME_PAIR} {
	yylval->string = reader_->translated(yytext);
	return token::NAME;
	}

{INDEX} {
	yylval->string = sta::stringCopy(yytext);
	return token::INDEX;
	}

{HCHAR} {
	char ch = yytext[0];
	return ((int) ch);
	}

{BLANK}	/* ignore blanks */ ;

	/* Send out of bound characters to parser. */
.	{ return ((int) yytext[0]); }

%%
