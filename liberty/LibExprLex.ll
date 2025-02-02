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

// Liberty function expression lexical analyzer

#include "util/FlexDisableRegister.hh"
#include "Debug.hh"
#include "StringUtil.hh"
#include "liberty/LibExprReaderPvt.hh"
#include "liberty/LibExprReader.hh"
#include "liberty/LibExprScanner.hh"

using sta::stringCopy;
using sta::FuncExpr;

#include "LibExprParse.hh"

#undef YY_DECL
#define YY_DECL \
int \
sta::LibExprScanner::lex(sta::LibExprParse::semantic_type *const yylval)

typedef sta::LibExprParse::token token;

%}

%option c++
%option yyclass="sta::LibExprScanner"
%option prefix="LibExpr"
%option noyywrap
%option never-interactive
%option stack
/* %option debug */

%x ESCAPED_STRING

PORT	[A-Za-z_]([A-Za-z0-9_\.\[\]])*
OP	"'"|"!"|"^"|"*"|"&"|"+"|"|"|1|0
PAREN	"("|")"
BLANK	[ \t\r]
ESCAPE	\\
QUOTE	\"
EOL	\r?\n

%%

{OP}|{PAREN} { return ((int) yytext[0]); }

{ESCAPE}{EOL} { /* I doubt that escaped returns get thru the parser */ }

{ESCAPE}{QUOTE}	{ BEGIN(ESCAPED_STRING); token_.clear(); }

<ESCAPED_STRING>. { token_ += yytext[0]; }

<ESCAPED_STRING>{ESCAPE}{QUOTE} {
	BEGIN(INITIAL);
	yylval->string = stringCopy(token_.c_str());
	return token::PORT;
	}

{PORT}	{
	yylval->string = stringCopy(yytext);
	return token::PORT;
	}

{BLANK}	{}

	/* Send out of bound characters to parser. */
.	{ return (int) yytext[0]; }

%%
