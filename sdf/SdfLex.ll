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
#include "sdf/SdfReaderPvt.hh"
#include "SdfParse.hh"
#include "sdf/SdfScanner.hh"

#undef YY_DECL
#define YY_DECL \
int \
sta::SdfScanner::lex(sta::SdfParse::semantic_type *const yylval, \
                     sta::SdfParse::location_type *loc)

// update location on matching
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);

typedef sta::SdfParse::token token;
%}

%option c++
%option yyclass="sta::SdfScanner"
%option prefix="Sdf"
%option noyywrap
%option never-interactive
%option stack
%option yylineno
/* %option debug */

%x COMMENT
%x QUOTE
%x COND_EXPR

ID	([A-Za-z_]|\\.)([A-Za-z0-9_\[\]]|\\.)*
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
	yylval->string = new std::string(token_);
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

("-"|"+")?([0-9]*)("."[0-9]+)([eE]("-"|"+")?[0-9]+)? {
	yylval->number = atof(yytext);
	return token::FNUMBER;
	}

"+"?[0-9]+ {
	yylval->integer = atoi(yytext);
        return token::DNUMBER;
        }

":"|"{"|"}"|"["|"]"|","|"*"|";"|"="|"-"|"+"|"|"|"("|")"|{HCHAR}	{
	return ((int) yytext[0]);
	}

ABSOLUTE { return token::ABSOLUTE; }
CELL	{ return token::CELL; }
CELLTYPE { return token::CELLTYPE; }
DATE	{ return token::DATE; }
DELAY	{ return token::DELAY; }
DELAYFILE { return token::DELAYFILE; }
DESIGN	{ return token::DESIGN; }
DEVICE	{ return token::DEVICE; }
DIVIDER	{ return token::DIVIDER; }
HOLD	{ return token::HOLD; }
INCREMENTAL|INCREMENT { return token::INCREMENTAL; }
INSTANCE { return token::INSTANCE; }
INTERCONNECT { return token::INTERCONNECT; }
IOPATH	{ return token::IOPATH; }
NOCHANGE { return token::NOCHANGE; }
PERIOD	{ return token::PERIOD; }
PORT	{ return token::PORT; }
PROCESS	{ return token::PROCESS; }
PROGRAM	{ return token::PROGRAM; }
RECOVERY { return token::RECOVERY; }
RECREM { return token::RECREM; }
REMOVAL { return token::REMOVAL; }
RETAIN { return token::RETAIN; }
SDFVERSION { return token::SDFVERSION; }
SETUP	{ return token::SETUP; }
SETUPHOLD { return token::SETUPHOLD; }
SKEW	{ return token::SKEW; }
TEMPERATURE { return token::TEMPERATURE; }
TIMESCALE { return token::TIMESCALE; }
TIMINGCHECK { return token::TIMINGCHECK; }
VENDOR	{ return token::VENDOR; }
VERSION { return token::PVERSION; }
VOLTAGE	{ return token::VOLTAGE; }
WIDTH	{ return token::WIDTH; }
negedge { return token::NEGEDGE; }
posedge { return token::POSEDGE; }
CONDELSE { return token::CONDELSE; }

COND	{
	BEGIN COND_EXPR;
	token_.clear();
        return token::COND;
	}

<COND_EXPR>"("{BLANK}*IOPATH {
	BEGIN INITIAL;
	yylval->string = new std::string(token_);
	return token::EXPR_OPEN_IOPATH;
	}

<COND_EXPR>"(" {
        /* Timing check conditions don't allow parens,
	 * so use the paren as a marker for the end of the expr.
 	 */
	if (reader_->inTimingCheck()) {
	  BEGIN INITIAL;
	  yylval->string = new std::string(token_);
	  return token::EXPR_OPEN;
	}
        else
          token_ += yytext[0];
}

<COND_EXPR>{BLANK}+{ID}{BLANK}*")" {
        /* (COND expr port) */
        if (reader_->inTimingCheck()) {
	  BEGIN INITIAL;
          /* remove trailing ")" */
          std::string cond_id(token_);
          cond_id += yytext;
          yylval->string = new std::string(cond_id.substr(0, cond_id.size() - 1));
	  /* No way to pass expr and id separately, so pass them together. */
	  return token::EXPR_ID_CLOSE;
	}
        else
          token_ += yytext[0];
}

<COND_EXPR>{BLANK} {}

<COND_EXPR>.   { token_ += yytext[0]; }

{ID}	{
	  yylval->string = new std::string(yytext);
	  return token::ID;
	}

{EOL}	{ loc->lines(); loc->step(); }

{BLANK}	{ /* Ignore blanks. */ }

	/* Send out of bound characters to parser. */
.	{ return ((int) yytext[0]); }

%%
