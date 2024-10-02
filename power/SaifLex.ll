%{

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

#include <cstdint>

#include "util/FlexDisableRegister.hh"
#include "StringUtil.hh"
#include "power/SaifReaderPvt.hh"
#include "SaifParse.hh"

#define YY_NO_INPUT

static std::string saif_token;

void
saifFlushBuffer()
{
  YY_FLUSH_BUFFER;
}

%}

/* %option debug */
%option noyywrap
%option nounput
%option never-interactive

%x COMMENT
%x QUOTE
%x COND_EXPR

ID	([A-Za-z_])([A-Za-z0-9_$\[\]\\.])*
HCHAR "."|"/"
BLANK	[ \n\t\r\b]
EOL	\r?\n

%%

"/*"		{ BEGIN COMMENT; }
<COMMENT>{

"*/"	{ BEGIN INITIAL; }

.
{EOL}	{ sta::saif_reader->incrLine(); }

<<EOF>> {
	SaifParse_error("unterminated comment");
	BEGIN(INITIAL);
	yyterminate();
	}
}

"\""	{ BEGIN QUOTE; saif_token.erase(); }
<QUOTE>{

"\\".	{ saif_token += yytext[1]; }

"\"" 	{
	BEGIN INITIAL;
	SaifParse_lval.string = sta::stringCopy(saif_token.c_str());
	return QSTRING;
	}

.	{ saif_token += yytext[0]; }

<<EOF>> {
	SaifParse_error("unterminated quoted string");
	BEGIN(INITIAL);
	yyterminate();
	}
}

"//"[^\n]*{EOL} { sta::saif_reader->incrLine(); }

[0-9]+ {
	SaifParse_lval.uint = atoll(yytext);
        return UINT;
        }

":"|"{"|"}"|"["|"]"|","|"*"|";"|"="|"-"|"+"|"|"|"("|")"|{HCHAR}	{
	return ((int) yytext[0]);
	}

SAIFILE { return SAIFILE; }
SAIFVERSION { return SAIFVERSION; }
DIRECTION { return DIRECTION; }
DESIGN	{ return DESIGN; }
DATE	{ return DATE; }
VENDOR	{ return VENDOR; }
PROGRAM_NAME { return PROGRAM_NAME; }
VERSION { return VERSION; }
DIVIDER	{ return DIVIDER; }
TIMESCALE { return TIMESCALE; }
DURATION { return DURATION; }
INSTANCE { return INSTANCE; }
NET { return NET; }
PORT { return PORT; }
T0  { return T0; }
T1  { return T1; }
TX  { return TX; }
TZ  { return TZ; }
TB  { return TB; }
TC  { return TC; }
IG  { return IG; }

{ID}	{
	SaifParse_lval.string = sta::stringCopy(yytext);
	return ID;
	}

{EOL}	{ sta::saif_reader->incrLine(); }

{BLANK}	{ /* Ignore blanks. */ }

	/* Send out of bound characters to parser. */
.	{ return ((int) yytext[0]); }

%%
