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

#include "Machine.hh"
#include "FlexPragma.hh"
#include <string.h>
#include <string>
#include "Machine.hh"
#include "StringUtil.hh"
#include "SpefReaderPvt.hh"
#include "SpefParse.hh"

#define YY_NO_INPUT

static std::string spef_token;

void
spefFlushBuffer()
{
  YY_FLUSH_BUFFER;
}

// Reset the start condition to INITIAL.
void
spefResetScanner()
{
  BEGIN(0);
}

%}

/* %option debug */
%option noyywrap
%option nounput
%option never-interactive

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

"*BUS_DELIMITER" { return BUS_DELIMITER; }
"*C2_R1_C1" { return C2_R1_C1; }
"*C" { return KW_C; }
"*CAP" { return CAP; }
"*CELL" { return CELL; }
"*CONN" { return CONN; }
"*C_UNIT" { return C_UNIT; }
"*SPEF" { return SPEF; }
"*DATE" { return DATE; }
"*DEFINE" { return DEFINE; }
"*DELIMITER" { return DELIMITER; }
"*DESIGN" { return DESIGN; }
"*DESIGN_FLOW" { return DESIGN_FLOW; }
"*DIVIDER" { return DIVIDER; }
"*DRIVER" { return DRIVER; }
"*D_NET" { return D_NET; }
"*D_PNET" { return D_PNET; }
"*D" { return KW_D; }
"*END" { return END; }
"*GROUND_NETS" { return GROUND_NETS; }
"*INDUC" { return INDUC; }
"*I" { return KW_I; }
"*K" { return KW_K; }
"*L" { return KW_L; }
"*LOADS" { return LOADS; }
"*L_UNIT" { return L_UNIT; }
"*NAME_MAP" { return NAME_MAP; }
"*N" { return KW_N; }
"*PDEFINE" { return PDEFINE; }
"*PHYSICAL_PORTS" { return PHYSICAL_PORTS; }
"*PORTS" { return PORTS; }
"*POWER_NETS" { return POWER_NETS; }
"*PROGRAM" { return PROGRAM; }
"*P" { return KW_P; }
"*Q" { return KW_Q; }
"*RC" { return RC; }
"*RES" { return RES; }
"*R_NET" { return R_NET; }
"*R_PNET" { return R_PNET; }
"*R_UNIT" { return R_UNIT; }
"*S" { return KW_S; }
"*T_UNIT" { return T_UNIT; }
"*VENDOR" { return VENDOR; }
"*VERSION" { return PVERSION; }
"*V" { return KW_V; }

"//".*\n { /* Single line comment */
	sta::spef_reader->incrLine();
	}

"/*"	{ BEGIN COMMENT; }
<COMMENT>{

.

\n	{ sta::spef_reader->incrLine(); }

"*/"	{ BEGIN INITIAL; }

<<EOF>> {
	SpefParse_error("unterminated comment");
	BEGIN(INITIAL);
	yyterminate();
	}
}

"\""	{ BEGIN QUOTE; spef_token.erase(); }
<QUOTE>{

\r?\n	{
	sta::spef_reader->incrLine();
	}

"\\".	{ spef_token += yytext[1]; }

"\"" 	{
	BEGIN INITIAL;
	SpefParse_lval.string = sta::stringCopy(spef_token.c_str());
	return QSTRING;
	}

.	{ spef_token += yytext[0]; }

<<EOF>> {
	SpefParse_error("unterminated quoted string");
	BEGIN(INITIAL);
	yyterminate();
	}
}

{BLANK}*\n {
	sta::spef_reader->incrLine();
	}

{INTEGER} {
	SpefParse_lval.integer = atoi(yytext);
	return INTEGER;
	}

{FLOAT} {
	SpefParse_lval.number = static_cast<float>(atof(yytext));
	return FLOAT;
	}

{IDENT} {
	SpefParse_lval.string = sta::spef_reader->translated(yytext);
	return IDENT;
	}

{PATH}|{NAME_PAIR} {
	SpefParse_lval.string = sta::spef_reader->translated(yytext);
	return NAME;
	}

{INDEX} {
	SpefParse_lval.string = sta::stringCopy(yytext);
	return INDEX;
	}

{HCHAR} {
	char ch = yytext[0];
	return ((int) ch);
	}

{BLANK}	/* ignore blanks */ ;

	/* Send out of bound characters to parser. */
.	{ return ((int) yytext[0]); }

%%
