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
#include "Sdf.hh"
#include "SdfParse.hh"

#define YY_NO_INPUT

static std::string sdf_token;

void
sdfFlushBuffer()
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

ID	([A-Za-z_]|\\.)([A-Za-z0-9_\[\]]|\\.)*
HCHAR "."|"/"
BLANK	[ \n\t\r\b]
EOL	\r?\n

%%

"/*"		{ BEGIN COMMENT; }
<COMMENT>{

"*/"	{ BEGIN INITIAL; }

.
{EOL}	{ sta::sdf_reader->incrLine(); }

<<EOF>> {
	SdfParse_error("unterminated comment");
	BEGIN(INITIAL);
	yyterminate();
	}
}

"\""	{ BEGIN QUOTE; sdf_token.erase(); }
<QUOTE>{

"\\".	{ sdf_token += yytext[1]; }

"\"" 	{
	BEGIN INITIAL;
	SdfParse_lval.string = sta::stringCopy(sdf_token.c_str());
	return QSTRING;
	}

.	{ sdf_token += yytext[0]; }

<<EOF>> {
	SdfParse_error("unterminated quoted string");
	BEGIN(INITIAL);
	yyterminate();
	}
}

"//"[^\n]*{EOL} { sta::sdf_reader->incrLine(); }

("-"|"+")?([0-9]*)("."[0-9]+)?([eE]("-"|"+")?[0-9]+)? {
	SdfParse_lval.number = static_cast<float>(atof(yytext));
	return NUMBER;
	}

":"|"{"|"}"|"["|"]"|","|"*"|";"|"="|"-"|"+"|"|"|"("|")"|{HCHAR}	{
	return ((int) yytext[0]);
	}

ABSOLUTE { return ABSOLUTE; }
CELL	{ return CELL; }
CELLTYPE { return CELLTYPE; }
DATE	{ return DATE; }
DELAY	{ return DELAY; }
DELAYFILE { return DELAYFILE; }
DESIGN	{ return DESIGN; }
DEVICE	{ return DEVICE; }
DIVIDER	{ return DIVIDER; }
HOLD	{ return HOLD; }
INCREMENTAL|INCREMENT { return INCREMENTAL; }
INSTANCE { return INSTANCE; }
INTERCONNECT { return INTERCONNECT; }
IOPATH	{ return IOPATH; }
NOCHANGE { return NOCHANGE; }
PERIOD	{ return PERIOD; }
PORT	{ return PORT; }
PROCESS	{ return PROCESS; }
PROGRAM	{ return PROGRAM; }
RECOVERY { return RECOVERY; }
RECREM { return RECREM; }
REMOVAL { return REMOVAL; }
RETAIN { return RETAIN; }
SDFVERSION { return SDFVERSION; }
SETUP	{ return SETUP; }
SETUPHOLD { return SETUPHOLD; }
SKEW	{ return SKEW; }
TEMPERATURE { return TEMPERATURE; }
TIMESCALE { return TIMESCALE; }
TIMINGCHECK { return TIMINGCHECK; }
VENDOR	{ return VENDOR; }
VERSION { return PVERSION; }
VOLTAGE	{ return VOLTAGE; }
WIDTH	{ return WIDTH; }
negedge { return NEGEDGE; }
posedge { return POSEDGE; }

CONDELSE { return CONDELSE; }

COND	{
	BEGIN COND_EXPR;
	sdf_token.erase();
	return COND;
	}

<COND_EXPR>"("{BLANK}*IOPATH {
	BEGIN INITIAL;
	SdfParse_lval.string = sta::stringCopy(sdf_token.c_str());
	return EXPR_OPEN_IOPATH;
	}

<COND_EXPR>"(" {
        /* Timing check conditions don't allow parens,
	 * so use the paren as a marker for the end of the expr.
 	 */
	if (sta::sdf_reader->inTimingCheck()) {
	  BEGIN INITIAL;
	  SdfParse_lval.string= sta::stringCopy(sdf_token.c_str());
	  return EXPR_OPEN;
	}
	else
	  sdf_token += yytext[0];
	}

<COND_EXPR>{BLANK}+{ID}{BLANK}*")" {
        /* (COND expr port) */
	if (sta::sdf_reader->inTimingCheck()) {
	  BEGIN INITIAL;
          /* remove trailing ")" */
          yytext[strlen(yytext)-1] = '\0';
          sdf_token += yytext;
	  SdfParse_lval.string= sta::stringCopy(sdf_token.c_str());
	  /* No way to pass expr and id separately, so pass them together. */
	  return EXPR_ID_CLOSE;
	}
	else
          sdf_token += yytext;
        }

<COND_EXPR>{BLANK} {}

<COND_EXPR>.   { sdf_token += yytext[0]; }

{ID}	{
	SdfParse_lval.string = sta::stringCopy(sta::sdf_reader->unescaped(yytext));
	return ID;
	}

{ID}({HCHAR}{ID})* {
	SdfParse_lval.string = sta::stringCopy(sta::sdf_reader->unescaped(yytext));
	return PATH;
	}

{EOL}	{ sta::sdf_reader->incrLine(); }

{BLANK}	{ /* Ignore blanks. */ }

	/* Send out of bound characters to parser. */
.	{ return ((int) yytext[0]); }

%%
