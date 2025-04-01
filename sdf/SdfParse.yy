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

#include "sdf/SdfReaderPvt.hh"
#include "sdf/SdfScanner.hh"

#undef yylex
#define yylex scanner->lex

// warning: variable 'yynerrs_' set but not used
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

void
sta::SdfParse::error(const location_type &loc,
                     const string &msg)
{
  reader->report()->fileError(164,reader->filename().c_str(),
                              loc.begin.line,"%s",msg.c_str());
}
%}

%require  "3.2"
%skeleton "lalr1.cc"
%debug
%define api.namespace {sta}
%locations
%define api.location.file "SdfLocation.hh"
%define parse.assert
%parse-param { SdfScanner *scanner }
%parse-param { SdfReader *reader }
%define api.parser.class {SdfParse}

// expected shift/reduce conflicts
%expect 4

%union {
  char character;
  std::string *string;
  float number;
  float *number_ptr;
  int integer;
  sta::SdfTriple *triple;
  sta::SdfTripleSeq *delval_list;
  sta::SdfPortSpec *port_spec;
  const sta::Transition *transition;
}

%token DELAYFILE SDFVERSION DESIGN DATE VENDOR PROGRAM PVERSION
%token DIVIDER VOLTAGE PROCESS TEMPERATURE TIMESCALE
%token CELL CELLTYPE INSTANCE DELAY ABSOLUTE INCREMENTAL
%token INTERCONNECT PORT DEVICE RETAIN
%token IOPATH TIMINGCHECK
%token SETUP HOLD SETUPHOLD RECOVERY REMOVAL RECREM WIDTH PERIOD SKEW NOCHANGE
%token POSEDGE NEGEDGE COND CONDELSE
%token QSTRING ID FNUMBER DNUMBER EXPR_OPEN_IOPATH EXPR_OPEN EXPR_ID_CLOSE

%type <number> FNUMBER NUMBER
%type <integer> DNUMBER
%type <number_ptr> number_opt
%type <triple> value triple
%type <delval_list> delval_list
%type <string> QSTRING ID path port port_instance
%type <string> EXPR_OPEN_IOPATH EXPR_OPEN EXPR_ID_CLOSE
%type <port_spec> port_spec port_tchk
%type <transition> port_transition
%type <character> hchar

// Used by error recovery.
%destructor { delete $$; } QSTRING

%start file

%%

file:
	'(' DELAYFILE  header cells ')' {}
;

header:
	header_stmt
|	header header_stmt
;

// technically the ordering of these statements is fixed by the spec
header_stmt:
	'(' SDFVERSION QSTRING ')' { delete $3; }
|	'(' DESIGN QSTRING ')' { delete $3; }
|	'(' DATE QSTRING ')' { delete $3; }
|	'(' VENDOR QSTRING ')' { delete $3; }
|	'(' PROGRAM QSTRING ')' { delete $3; }
|	'(' PVERSION QSTRING ')' { delete $3; }
|	'(' DIVIDER hchar ')' { reader->setDivider($3); }
|	'(' VOLTAGE triple ')' { reader->deleteTriple($3); }
|	'(' VOLTAGE NUMBER ')'
|	'(' VOLTAGE ')'  // Illegal SDF (from OC).
|	'(' PROCESS QSTRING ')' { delete $3; }
|	'(' PROCESS ')'  // Illegal SDF (from OC).
|	'(' TEMPERATURE NUMBER ')'
|	'(' TEMPERATURE triple ')' { reader->deleteTriple($3); }
|	'(' TEMPERATURE ')' // Illegal SDF (from OC).
|	'(' TIMESCALE NUMBER ID ')' { reader->setTimescale($3, $4); }
;

hchar:
	'/'
	{ $$ = '/'; }
|	'.'
	{ $$ = '.'; }
;

number_opt: { $$ = nullptr; }
|	NUMBER	{ $$ = new float($1); }
;

cells:
	cell
|	cells cell
;

cell:
	'(' CELL celltype cell_instance timing_specs ')'
	{ reader->cellFinish(); }
;

celltype:
	'(' CELLTYPE QSTRING ')'
	{ reader->setCell($3); }
;

cell_instance:
	'(' INSTANCE ')'
        { reader->setInstance(nullptr); }
|	'(' INSTANCE '*' ')'
        { reader->setInstanceWildcard(); }
|	'(' INSTANCE path ')'
	{ reader->setInstance($3); }
;

timing_specs:
	/* empty */
|	timing_specs timing_spec
;

timing_spec:
	'(' DELAY deltypes ')'
|	'(' TIMINGCHECK tchk_defs ')'
;

deltypes:
|       deltypes deltype
;

deltype:
	'(' ABSOLUTE
	{ reader->setInIncremental(false); }
	del_defs ')'
|	'(' INCREMENTAL
	{ reader->setInIncremental(true); }
	del_defs ')'
;

del_defs:
|	del_defs del_def
;

path:
	ID
        { $$ = reader->unescaped($1); }
|	path hchar ID
        { $$ = reader->makePath($1, reader->unescaped($3)); }
;

del_def:
	'(' IOPATH port_spec port_instance retains delval_list ')'
	{ reader->iopath($3, $4, $6, nullptr, false); }
|	'(' CONDELSE '(' IOPATH port_spec port_instance
            retains delval_list ')' ')'
	{ reader->iopath($5, $6, $8, nullptr, true); }
|	'(' COND EXPR_OPEN_IOPATH port_spec port_instance
            retains delval_list ')' ')'
	{ reader->iopath($4, $5, $7, $3, false); }
|	'(' INTERCONNECT port_instance port_instance delval_list ')'
	{ reader->interconnect($3, $4, $5); }
|	'(' PORT port_instance delval_list ')'
	{ reader->port($3, $4); }
|	'(' DEVICE delval_list ')'
	{ reader->device($3); }
|	'(' DEVICE port_instance delval_list ')'
	{ reader->device($3, $4); }
;

retains:
	/* empty */
|	retains retain
;

retain:
	'(' RETAIN delval_list ')'
	{ reader->deleteTripleSeq($3); }
;

delval_list:
	value
	{ $$ = reader->makeTripleSeq(); $$->push_back($1); }
|  	delval_list value
	{ $1->push_back($2); $$ = $1; }
;

tchk_defs:
|	tchk_defs tchk_def
;

tchk_def:
	'(' SETUP { reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	{ reader->timingCheck(sta::TimingRole::setup(), $4, $5, $6);
	  reader->setInTimingCheck(false);
	}
|	'(' HOLD { reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	{ reader->timingCheck(sta::TimingRole::hold(), $4, $5, $6);
	  reader->setInTimingCheck(false);
	}
|	'(' SETUPHOLD { reader->setInTimingCheck(true); }
	port_tchk port_tchk value value ')'
	{ reader->timingCheckSetupHold($4, $5, $6, $7);
	  reader->setInTimingCheck(false);
	}
|	'(' RECOVERY { reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	{ reader->timingCheck(sta::TimingRole::recovery(),$4,$5,$6);
	  reader->setInTimingCheck(false);
	}
|	'(' REMOVAL { reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	{ reader->timingCheck(sta::TimingRole::removal(),$4,$5,$6);
	  reader->setInTimingCheck(false);
	}
|	'(' RECREM { reader->setInTimingCheck(true); }
	port_tchk port_tchk value value ')'
	{ reader->timingCheckRecRem($4, $5, $6, $7);
	  reader->setInTimingCheck(false);
	}
|	'(' SKEW { reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	// Sdf skew clk/ref are reversed from liberty.
	{ reader->timingCheck(sta::TimingRole::skew(),$5,$4,$6);
	  reader->setInTimingCheck(false);
	}
|	'(' WIDTH { reader->setInTimingCheck(true); }
	port_tchk value ')'
	{ reader->timingCheckWidth($4, $5);
	  reader->setInTimingCheck(false);
	}
|	'(' PERIOD { reader->setInTimingCheck(true); }
	port_tchk value ')'	
	{ reader->timingCheckPeriod($4, $5);
	  reader->setInTimingCheck(false);
	}
|	'(' NOCHANGE { reader->setInTimingCheck(true); }
	port_tchk port_tchk value value ')'
	{ reader->timingCheckNochange($4, $5, $6, $7);
	  reader->setInTimingCheck(false);
	}
;

port:
	ID
        { $$ = reader->unescaped($1); }
        | ID '[' DNUMBER ']'
        { $$ = reader->makeBusName($1, $3); }
;

port_instance:
	port
|	path hchar port
        { $$ = reader->makePath($1, $3); }
;

port_spec:
	port_instance
	{ $$=reader->makePortSpec(sta::Transition::riseFall(),$1,nullptr); }
|	'(' port_transition port_instance ')'
	{ $$ = reader->makePortSpec($2, $3, nullptr); }
;

port_transition:
	POSEDGE	{ $$ = sta::Transition::rise(); }
|	NEGEDGE	{ $$ = sta::Transition::fall(); }
;

port_tchk:
	port_spec
|	'(' COND EXPR_ID_CLOSE
	{ $$ = reader->makeCondPortSpec($3); }
|	'(' COND EXPR_OPEN port_transition port_instance ')' ')'
        { $$ = reader->makePortSpec($4, $5, $3); }
;

value:
	'(' ')'
	{
	  $$ = reader->makeTriple();
	}
|	'(' NUMBER ')'
	{
	  $$ = reader->makeTriple($2);
	}
|	'(' triple ')' { $$ = $2; }
;

triple:
	NUMBER ':' number_opt ':' number_opt
	{
	  float *fp = new float($1);
	  $$ = reader->makeTriple(fp, $3, $5);
	}
|	number_opt ':' NUMBER ':' number_opt
	{
	  float *fp = new float($3);
	  $$ = reader->makeTriple($1, fp, $5);
	}
|	number_opt ':' number_opt ':' NUMBER
	{
	  float *fp = new float($5);
	  $$ = reader->makeTriple($1, $3, fp);
	}
;

NUMBER:
        FNUMBER
|       DNUMBER
        { $$ = static_cast<float>($1); }
|       '-' DNUMBER
        { $$ = static_cast<float>(-$2); }
;

%%
