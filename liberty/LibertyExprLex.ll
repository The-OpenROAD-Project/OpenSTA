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

// Liberty function expression lexical analyzer

#include "Machine.hh"
#include "FlexPragma.hh"
#include "Debug.hh"
#include "StringUtil.hh"
#include "LibertyExprPvt.hh"

using sta::libexpr_parser;
using sta::stringCopy;
using sta::FuncExpr;

#include "LibertyExprParse.hh"

#define YY_NO_INPUT

#define YY_INPUT(buf,result,max_size) \
  result = libexpr_parser->copyInput(buf, max_size)

void
libertyExprFlushBuffer()
{
  YY_FLUSH_BUFFER;
}

%}

/* %option debug */
%option noyywrap
%option nounput
%option never-interactive

%x ESCAPED_STRING

PORT	[A-Za-z_]([A-Za-z0-9_\[\]])*
OP	"'"|"!"|"^"|"*"|"&"|"+"|"|"|1|0
PAREN	"("|")"
BLANK	[ \t\r]
ESCAPE	\\
QUOTE	\"
EOL	\r?\n

%%

{OP}|{PAREN} { return ((int) LibertyExprLex_text[0]); }

{ESCAPE}{EOL} { /* I doubt that escaped returns get thru the parser */ }

{ESCAPE}{QUOTE}	{ BEGIN(ESCAPED_STRING); libexpr_parser->tokenErase(); }

<ESCAPED_STRING>. { libexpr_parser->tokenAppend(LibertyExprLex_text[0]); }

<ESCAPED_STRING>{ESCAPE}{QUOTE} {
	BEGIN(INITIAL);
	LibertyExprParse_lval.string = libexpr_parser->tokenCopy();
	return PORT;
	}

{PORT}	{
	LibertyExprParse_lval.string = stringCopy(LibertyExprLex_text);
	return PORT;
	}

{BLANK}	{}

	/* Send out of bound characters to parser. */
.	{ return (int) LibertyExprLex_text[0]; }

%%
