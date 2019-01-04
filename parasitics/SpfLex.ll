%{

// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include <string.h>
#include <string>
#include "Machine.hh"
#include "StringUtil.hh"
#include "SpfReaderPvt.hh"
#include "SpfParse.hh"

#define YY_NO_INPUT

#define keyword(_key_) \
	BEGIN(STMT); \
        return _key_;

static std::string spf_token;

void
spfFlushBuffer()
{
  YY_FLUSH_BUFFER;
}

// Reset the start condition to INITIAL.
void
spfResetScanner()
{
  BEGIN(0);
}

%}

/* %option debug */
%option noyywrap
%option nounput
%option never-interactive

%x STMT
%x QUOTE

INUMBER	[0-9]+
NUMBER	([0-9]*)"."[0-9]+
ENUMBER	({NUMBER}|{INUMBER})[eE]("-"|"+")?[0-9]+
UNIT	[KMUNPF]
HCHAR	"."|"/"|"|"|":"|"["|"]"|"<"|">"|"("|")"
ID	([A-Za-z_]|\\.)([A-Za-z0-9_\[\]]|\\.)*
BLANK	[ \n\t\r\b]
EOL	\r?\n

%%

"*|RSPF" { keyword(RSPF) }
"*|DSPF" { keyword(DSPF) }
"*|DESIGN" { keyword(DESIGN) }
"*|DATE" { keyword(DATE) }
"*|VENDOR" { keyword(VENDOR) }
"*|PROGRAM" { keyword(PROGRAM) }
"*|VERSION" { keyword(PVERSION) }
"*|DIVIDER" { keyword(DIVIDER) }
"*|DELIMITER" { keyword(DELIMITER) }
"*|BUSBIT" { keyword(BUSBIT) }
"*|GROUND_NET" { keyword(GROUND_NET) }
"*|NET" { keyword(NET) }
"*|DRIVER" { keyword(DRIVER) }
"*|LOAD" { keyword(LOAD) }
"*|P" { keyword(PINDEF) }
"*|I" { keyword(INSTPIN) }
"*|S" { keyword(SUBNODE) }
".SUBCKT" { keyword(SUBCKT) }
".ENDS" { keyword(ENDS) }
".END" { keyword(END) }
X { keyword(SUBCKT_CALL) }
C { keyword(CAPACITOR) }
R { keyword(RESISTOR) }
E { keyword(VCVS) }

"*"[^|+\r\n][^\r\n]*{EOL} {
	/* comment */
	sta::spf_reader->incrLine();
	}

"*"{EOL} {
	/* blank single line comment */
	sta::spf_reader->incrLine();
	}

[ \t]*{EOL} {
	/* blank line */
	sta::spf_reader->incrLine();
	}

<STMT>{

{INUMBER} {
	SpfParse_lval.integer = atoi(yytext);
	return INUMBER;
	}

{ENUMBER} {
	SpfParse_lval.number = static_cast<float>(atof(yytext));
	return ENUMBER;
	}

{NUMBER} {
	SpfParse_lval.number = static_cast<float>(atof(yytext));
	return NUMBER;
	}

({INUMBER}|{NUMBER}|{ENUMBER}){UNIT} {
	int len = strlen(yytext);
	char unit = yytext[len - 1];
	float scale = sta::spf_reader->unitScale(unit);
	yytext[len - 1] = '\0';
	SpfParse_lval.number = static_cast<float>(atof(yytext) * scale);
	return NUMBER;
	}

({INUMBER}|{NUMBER}|{ENUMBER}){UNIT}?F {
	int len = strlen(yytext);
	char unit = yytext[len - 2];
	float scale = sta::spf_reader->unitScale(unit);
	yytext[len - 2] = '\0';
	SpfParse_lval.number = static_cast<float>(atof(yytext) * scale);
	return CVALUE;
	}

{HCHAR}	{ return ((int) yytext[0]); }

{ID}	{
	SpfParse_lval.string = sta::spf_reader->translated(yytext);
	return ID;
	}

{ID}({HCHAR}({ID}|{INUMBER}))+ {
	SpfParse_lval.string = sta::spf_reader->translated(yytext);
	return PATH;
	}

{EOL}"+" {
	/* continuation */
	sta::spf_reader->incrLine();
	}

{EOL}"*+" {
	/* comment continuation */
	sta::spf_reader->incrLine();
	}

{EOL}	{
	sta::spf_reader->incrLine();
	BEGIN(INITIAL);
	}

"\""	{
	BEGIN(QUOTE);
	spf_token.erase();
	}

{BLANK}	/* ignore blanks */

	/* Send out of bound characters to parser. */
.	{ return ((int) yytext[0]); }

}

<QUOTE>{

"\\".	{ spf_token += yytext[1]; }

"\"" 	{
	BEGIN(STMT);
	const char *token = spf_token.c_str();
	SpfParse_lval.string= sta::stringCopy(token);
	return QSTRING;
	}

.	{ spf_token += yytext[0]; }

}

	/* Send out of bound characters to parser. */
.	{ return ((int) yytext[0]); }

%%
