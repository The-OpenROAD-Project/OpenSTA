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
#include "Debug.hh"
#include "VerilogNamespace.hh"
#include "VerilogReaderPvt.hh"
#include "VerilogParse.hh"

#define YY_NO_INPUT

int verilog_line = 1;
static std::string string_buf;

void
verilogFlushBuffer()
{
  YY_FLUSH_BUFFER;
}

%}

/* %option debug */
%option noyywrap
%option nounput
%option never-interactive

%x COMMENT
%x ATTRIBUTE
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
	sta::verilog_reader->incrLine();
	}

"//"[^\n]*{EOL} { /* Single line comment. */
	sta::verilog_reader->incrLine();
	}

"/*"	{ BEGIN COMMENT; }
<COMMENT>{
.

{EOL}	{ sta::verilog_reader->incrLine(); }

"*/"	{ BEGIN INITIAL; }

<<EOF>> {
	VerilogParse_error("unterminated comment");
	BEGIN(INITIAL);
	yyterminate();
	}
}

"(*"	{ BEGIN ATTRIBUTE; }
<ATTRIBUTE>{
.

{EOL}	{ sta::verilog_reader->incrLine(); }

"*)"	{ BEGIN INITIAL; }

<<EOF>> {
	VerilogParse_error("unterminated attribute");
	BEGIN(INITIAL);
	yyterminate();
	}
}

{SIGN}?{UNSIGNED_NUMBER}?"'"[bB][01_xz]+ {
  VerilogParse_lval.constant = sta::stringCopy(VerilogLex_text);
  return CONSTANT;
}

{SIGN}?{UNSIGNED_NUMBER}?"'"[oO][0-7_xz]+ {
  VerilogParse_lval.constant = sta::stringCopy(VerilogLex_text);
  return CONSTANT;
}

{SIGN}?{UNSIGNED_NUMBER}?"'"[dD][0-9_]+ {
  VerilogParse_lval.constant = sta::stringCopy(VerilogLex_text);
  return CONSTANT;
}

{SIGN}?{UNSIGNED_NUMBER}?"'"[hH][0-9a-fA-F_xz]+ {
  VerilogParse_lval.constant = sta::stringCopy(VerilogLex_text);
  return CONSTANT;
}

{SIGN}?[0-9]+ {
  VerilogParse_lval.ival = atol(VerilogLex_text);
  return INT;
}

":"|"."|"{"|"}"|"["|"]"|","|"*"|";"|"="|"-"|"+"|"|"|"("|")" {
  return ((int) VerilogLex_text[0]);
}

assign { return ASSIGN; }
endmodule { return ENDMODULE; }
inout { return INOUT; }
input { return INPUT; }
module { return MODULE; }
output { return OUTPUT; }
parameter { return PARAMETER; }
defparam { return DEFPARAM; }
reg { return REG; }
supply0 { return SUPPLY0; }
supply1 { return SUPPLY1; }
tri { return TRI; }
wand { return WAND; }
wire { return WIRE; }
wor { return WOR; }

{ID_TOKEN}("."{ID_TOKEN})* {
	VerilogParse_lval.string = sta::stringCopy(sta::verilogToSta(VerilogLex_text));
	return ID;
}

{EOL}	{ sta::verilog_reader->incrLine(); }

{BLANK}	{ /* ignore blanks */ }

\"	{
	string_buf.erase();
	BEGIN(QSTRING);
	}

<QSTRING>\" {
	BEGIN(INITIAL);
	VerilogParse_lval.string = sta::stringCopy(string_buf.c_str());
	return STRING;
	}

<QSTRING>{EOL} {
	VerilogParse_error("unterminated string constant");
	BEGIN(INITIAL);
	VerilogParse_lval.string = sta::stringCopy(string_buf.c_str());
	return STRING;
	}

<QSTRING>\\{EOL} {
	/* Line continuation. */
	sta::verilog_reader->incrLine();
	}

<QSTRING>[^\r\n\"]+ {
	/* Anything return or double quote */
	string_buf += VerilogLex_text;
	}

<QSTRING><<EOF>> {
	VerilogParse_error("unterminated string constant");
	BEGIN(INITIAL);
	yyterminate();
	}

	/* Send out of bound characters to parser. */
.	{ return (int) VerilogLex_text[0]; }

%%
