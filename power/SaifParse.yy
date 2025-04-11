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

%{
#include <cctype>

#include "Report.hh"
#include "StringUtil.hh"
#include "power/SaifReaderPvt.hh"
#include "power/SaifScanner.hh"

#undef yylex
#define yylex scanner->lex

// warning: variable 'yynerrs_' set but not used
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define loc_line(loc) loc.begin.line

void
sta::SaifParse::error(const location_type &loc,
                      const std::string &msg)
{
  reader->report()->fileError(169,reader->filename(),loc.begin.line,"%s",msg.c_str());
}
%}

%require  "3.2"
%skeleton "lalr1.cc"
%debug
%define api.namespace {sta}
%locations
%define api.location.file "SaifLocation.hh"
%define parse.assert
%parse-param { SaifScanner *scanner }
%parse-param { SaifReader *reader }
%define api.parser.class {SaifParse}

// expected shift/reduce conflicts
%expect 2

%union {
  char character;
  const char *string;
  uint64_t uint;
  sta::SaifState state;
  sta::SaifStateDurations state_durations;
}

%token SAIFILE SAIFVERSION DIRECTION DESIGN DATE VENDOR PROGRAM_NAME VERSION
%token DIVIDER TIMESCALE DURATION
%token INSTANCE NET PORT
%token T0 T1 TX TZ TB TC IG
%token QSTRING ID FNUMBER DNUMBER UINT

%type <uint> UINT
%type <string> QSTRING ID
%type <character> hchar
%type <state> state
%type <state_durations> state_durations

%start file

%{
%}

%%

file:
	'(' SAIFILE  header instance ')' {}
;

header:
	header_stmt
|	header header_stmt
;

header_stmt:
	'(' SAIFVERSION QSTRING ')' { sta::stringDelete($3); }
|	'(' DIRECTION QSTRING ')' { sta::stringDelete($3); }
|	'(' DESIGN QSTRING ')' { sta::stringDelete($3); }
|	'(' DESIGN ')' { }
|	'(' DATE QSTRING ')' { sta::stringDelete($3); }
|	'(' VENDOR QSTRING ')' { sta::stringDelete($3); }
|	'(' PROGRAM_NAME QSTRING ')' { sta::stringDelete($3); }
|	'(' VERSION QSTRING ')' { sta::stringDelete($3); }
|	'(' DIVIDER hchar ')' { reader->setDivider($3); }
|	'(' TIMESCALE UINT ID ')' { reader->setTimescale($3, $4); }
|	'(' DURATION UINT ')' { reader->setDuration($3); }
;

hchar:
	'/'
	{ $$ = '/'; }
|	'.'
	{ $$ = '.'; }
;

instance:
	'(' INSTANCE ID
	{ reader->instancePush($3); }
        instance_contents ')'
	{ reader->instancePop(); }
|	'(' INSTANCE QSTRING ID
	{ reader->instancePush($3); }
        instance_contents ')'
	{ reader->instancePop(); }
;

instance_contents:
        /* empty */
|       instance_content
|       instance_contents instance_content
;

instance_content:
        '(' PORT ports ')'
|       '(' NET nets ')'
|       instance
;

nets:
        net
|       nets net
;

net:
        '(' ID state_durations ')'
        { reader->setNetDurations($2, $3); }
;

ports:
        port
|       ports port
;

port:
        '(' ID state_durations ')'
;

state_durations:
        '(' state UINT ')'
        { $$[static_cast<int>($2)] = $3; }
|       state_durations '(' state UINT ')'
        { $$[static_cast<int>($3)] = $4; }
;

state:
        T0
        { $$ = sta::SaifState::T0; }
|       T1
        { $$ = sta::SaifState::T1; }
|       TX
        { $$ = sta::SaifState::TX; }
|       TZ
        { $$ = sta::SaifState::TZ; }
|       TB
        { $$ = sta::SaifState::TB; }
|       TC
        { $$ = sta::SaifState::TC; }
|       IG
        { $$ = sta::SaifState::IG; }
;

%%
