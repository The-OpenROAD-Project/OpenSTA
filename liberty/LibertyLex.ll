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
#include <ctype.h>
#include <string.h>
#include "Machine.hh"
#include "LibertyParser.hh"
#include "LibertyParse.hh"

#define YY_NO_INPUT

#if defined(YY_FLEX_MAJOR_VERSION) \
    && defined(YY_FLEX_MINOR_VERSION) \
    && YY_FLEX_MAJOR_VERSION >=2 \
    && (YY_FLEX_MINOR_VERSION > 5 \
	|| (YY_FLEX_MINOR_VERSION == 5 \
	    && defined(YY_FLEX_SUBMINOR_VERSION) \
	    && YY_FLEX_SUBMINOR_VERSION >= 31))
 #define INCLUDE_SUPPORTED
#endif

static std::string string_buf;

void
libertyParseFlushBuffer()
{
  YY_FLUSH_BUFFER;
}

%}

/* %option debug */
%option noyywrap
%option nounput
%option never-interactive

%x comment
%x qstring

DIGIT [0-9]
ALPHA [a-zA-Z]
FLOAT [-+]?(({DIGIT}+\.?{DIGIT}*)|(\.{DIGIT}+))([Ee][-+]?{DIGIT}+)?
BLANK [ \t]
BUS_LEFT [\[<]
BUS_RIGHT [\]>]
BUS_SUB {BUS_LEFT}{DIGIT}+{BUS_RIGHT}
BUS_RANGE {BUS_LEFT}{DIGIT}+:{DIGIT}+{BUS_RIGHT}
PIN_NAME ({ALPHA}|_)({ALPHA}|{DIGIT}|_)*
BUS_NAME {PIN_NAME}({BUS_SUB}|{BUS_RANGE})
BUS_NAME2 {PIN_NAME}{BUS_SUB}({BUS_SUB}|{BUS_RANGE})
MIXED_NAME {BUS_NAME}_{PIN_NAME}
HNAME ({PIN_NAME}|{BUS_NAME}|{MIXED_NAME})([\/.]({PIN_NAME}|{BUS_NAME}|{MIXED_NAME}))+
/* ocv_table_template(2D_ocv_template) */
/* leakage_power_unit             : 1pW; */
/* default_operating_conditions : slow_100_3.00 ; */
/* revision : 1.0.17; */
/* default_wire_load : xc2v250-5_avg; */
TOKEN ({ALPHA}|{DIGIT}|_)({ALPHA}|{DIGIT}|[._\-])*
/* bus_naming_style : %s[%d] ; */
BUS_STYLE "%s"{BUS_LEFT}"%d"{BUS_RIGHT}
PUNCTUATION [,\:;|(){}+*&!'=]
TOKEN_END {PUNCTUATION}|[ \t\r\n]
EOL \r?\n
%%

{PUNCTUATION} { return ((int) LibertyLex_text[0]); }

{FLOAT}{TOKEN_END} {
	/* Push back the TOKEN_END character. */
	yyless(LibertyLex_leng - 1);
	LibertyParse_lval.number = static_cast<float>(strtod(LibertyLex_text,
	                                                     NULL));
	return FLOAT;
	}

{ALPHA}({ALPHA}|_|{DIGIT})*{TOKEN_END} {
	/* Push back the TOKEN_END character. */
	yyless(LibertyLex_leng - 1);
	LibertyParse_lval.string = sta::stringCopy(LibertyLex_text);
	return KEYWORD;
	}

{PIN_NAME}{TOKEN_END} |
{BUS_NAME}{TOKEN_END} |
{BUS_NAME2}{TOKEN_END} |
{MIXED_NAME}{TOKEN_END} |
{HNAME}{TOKEN_END} |
{BUS_STYLE}{TOKEN_END} |
{TOKEN}{TOKEN_END} {
	/* Push back the TOKEN_END character. */
	yyless(LibertyLex_leng - 1);
	LibertyParse_lval.string = sta::stringCopy(LibertyLex_text);
	return STRING;
	}

\\?{EOL} { sta::libertyIncrLine(); }

"include_file"[ \t]*"(".+")"[ \t]*";"? {
#ifdef INCLUDE_SUPPORTED
	if (sta::libertyInInclude())
	  sta::libertyParseError("nested include_file's are not supported\n");
	else {
	  char *filename = &yytext[strlen("include_file")];
	  /* Skip blanks between include_file and '('. */
	  while (isspace(*filename) && *filename != '\0')
	    filename++;
          /* Skip '('. */
	  filename++;
	  /* Skip blanks between '(' and filename. */
	  while (isspace(*filename) && *filename != '\0')
	    filename++;
	  char *filename_end = strpbrk(filename, ")");
	  if (filename_end == NULL)
	    sta::libertyParseError("include_file missing ')'\n");
	  else {
	    /* Trim trailing blanks. */
	    while (isspace(filename_end[-1]) && filename_end > filename)
	      filename_end--;
	    *filename_end = '\0';
	    FILE *stream = sta::libertyIncludeBegin(filename);
	    if (stream) {
	      yypush_buffer_state(yy_create_buffer(stream, YY_BUF_SIZE));
	      BEGIN(INITIAL);
	    }
	  }
	}
#else
	sta::libertyParseError("include_file is not supported.\n");
#endif
}

"/*"	BEGIN(comment);

	/* Straight out of the flex man page. */
<comment>[^*\r\n]*		/* eat anything that's not a '*' */
<comment>"*"+[^*/\r\n]*		/* eat up '*'s not followed by '/'s */
<comment>{EOL}	sta::libertyIncrLine();
<comment>"*"+"/" BEGIN(INITIAL);

\"	{
	string_buf.erase();
	BEGIN(qstring);
	}

<qstring>\" {
	BEGIN(INITIAL);
	LibertyParse_lval.string = sta::stringCopy(string_buf.c_str());
	return STRING;
	}

<qstring>{EOL} {
	LibertyParse_error("unterminated string constant");
	BEGIN(INITIAL);
	LibertyParse_lval.string = sta::stringCopy(string_buf.c_str());
	return STRING;
	}

<qstring>\\{EOL} {
	/* Line continuation. */
	sta::libertyIncrLine();
	}

<qstring>\\. {
	/* Escaped character. */
	string_buf += '\\';
	string_buf += LibertyLex_text[1];
	}

<qstring>[^\\\r\n\"]+ {
	/* Anything but escape, return or double quote */
	string_buf += LibertyLex_text;
	}

<qstring><<EOF>> {
	LibertyParse_error("unterminated string constant");
	BEGIN(INITIAL);
	yyterminate();
	}

{BLANK}* {}
	/* Send out of bound characters to parser. */
.	{ return (int) LibertyLex_text[0]; }

<<EOF>> {
#ifdef INCLUDE_SUPPORTED
	if (sta::libertyInInclude()) {
	  sta::libertyIncludeEnd();
	  yypop_buffer_state();
	}
	else
#endif
	  yyterminate();
}

%%
