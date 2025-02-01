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

#include <cstdint>

#include "util/FlexDisableRegister.hh"
#include "StringUtil.hh"
#include "power/SaifReaderPvt.hh"
#include "SaifParse.hh"
#include "power/SaifScanner.hh"

#undef YY_DECL
#define YY_DECL \
int \
sta::SaifScanner::lex(sta::SaifParse::semantic_type *const yylval, \
                      sta::SaifParse::location_type *loc)

// update location on matching
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);

typedef sta::SaifParse::token token;
%}

%option c++
%option yyclass="sta::SaifScanner"
%option prefix="Saif"
%option noyywrap
%option never-interactive
%option stack
%option yylineno
/* %option debug */

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
{EOL}	{ loc->lines(); loc->step(); }

<<EOF>> {
	error("unterminated comment");
	BEGIN(INITIAL);
	yyterminate();
	}
}

"\""	{ BEGIN QUOTE; token_.clear(); }
<QUOTE>{

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

"//"[^\n]*{EOL} { loc->lines(); loc->step(); }

[0-9]+ {
	yylval->uint = atoll(yytext);
        return token::UINT;
        }

":"|"{"|"}"|"["|"]"|","|"*"|";"|"="|"-"|"+"|"|"|"("|")"|{HCHAR}	{
	return ((int) yytext[0]);
	}

SAIFILE { return token::SAIFILE; }
SAIFVERSION { return token::SAIFVERSION; }
DIRECTION { return token::DIRECTION; }
DESIGN	{ return token::DESIGN; }
DATE	{ return token::DATE; }
VENDOR	{ return token::VENDOR; }
PROGRAM_NAME { return token::PROGRAM_NAME; }
VERSION { return token::VERSION; }
DIVIDER	{ return token::DIVIDER; }
TIMESCALE { return token::TIMESCALE; }
DURATION { return token::DURATION; }
INSTANCE { return token::INSTANCE; }
NET { return token::NET; }
PORT { return token::PORT; }
T0  { return token::T0; }
T1  { return token::T1; }
TX  { return token::TX; }
TZ  { return token::TZ; }
TB  { return token::TB; }
TC  { return token::TC; }
IG  { return token::IG; }

{ID}	{
	yylval->string = sta::stringCopy(yytext);
	return token::ID;
	}

{EOL}	{ loc->lines(); loc->step(); }

{BLANK}	{ /* Ignore blanks. */ }

	/* Send out of bound characters to parser. */
.	{ return ((int) yytext[0]); }

%%
