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

#include <cctype>

#include "StringUtil.hh"
#include "power/SaifReaderPvt.hh"

int SaifLex_lex();
#define SaifParse_lex SaifLex_lex
// use yacc generated parser errors
#define YYERROR_VERBOSE

#define YYDEBUG 1

%}

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
|	'(' DIVIDER hchar ')' { sta::saif_reader->setDivider($3); }
|	'(' TIMESCALE UINT ID ')' { sta::saif_reader->setTimescale($3, $4); }
|	'(' DURATION UINT ')' { sta::saif_reader->setDuration($3); }
;

hchar:
	'/'
	{ $$ = '/'; }
|	'.'
	{ $$ = '.'; }
;

instance:
	'(' INSTANCE ID
	{ sta::saif_reader->instancePush($3); }
        instance_contents ')'
	{ sta::saif_reader->instancePop(); }
|	'(' INSTANCE QSTRING ID
	{ sta::saif_reader->instancePush($3); }
        instance_contents ')'
	{ sta::saif_reader->instancePop(); }
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
        { sta::saif_reader->setNetDurations($2, $3); }
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
