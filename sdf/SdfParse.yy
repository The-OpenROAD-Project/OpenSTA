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

#include <ctype.h>
#include "Machine.hh"
#include "Sdf.hh"

int SdfLex_lex();
#define SdfParse_lex SdfLex_lex
// use yacc generated parser errors
#define YYERROR_VERBOSE

%}

// expected shift/reduce conflicts
%expect 4

%union {
  char character;
  char *string;
  float number;
  float *number_ptr;
  sta::SdfTriple *triple;
  sta::SdfTripleSeq *delval_list;
  sta::SdfPortSpec *port_spec;
  sta::Transition *transition;
}

%token DELAYFILE SDFVERSION DESIGN DATE VENDOR PROGRAM PVERSION
%token DIVIDER VOLTAGE PROCESS TEMPERATURE TIMESCALE
%token CELL CELLTYPE INSTANCE DELAY ABSOLUTE INCREMENTAL
%token INTERCONNECT PORT DEVICE RETAIN
%token IOPATH TIMINGCHECK
%token SETUP HOLD SETUPHOLD RECOVERY REMOVAL RECREM WIDTH PERIOD SKEW NOCHANGE
%token POSEDGE NEGEDGE COND CONDELSE
%token QSTRING ID PATH NUMBER EXPR_OPEN_IOPATH EXPR_OPEN EXPR_ID_CLOSE

%type <number> NUMBER
%type <number_ptr> number_opt
%type <triple> value triple
%type <delval_list> delval_list
%type <string> QSTRING ID PATH path port_instance
%type <string> EXPR_OPEN_IOPATH EXPR_OPEN EXPR_ID_CLOSE
%type <port_spec> port_spec port_tchk
%type <transition> port_transition
%type <character> hchar

%start file

%{
%}

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
	'(' SDFVERSION QSTRING ')' { sta::stringDelete($3); }
|	'(' DESIGN QSTRING ')' { sta::stringDelete($3); }
|	'(' DATE QSTRING ')' { sta::stringDelete($3); }
|	'(' VENDOR QSTRING ')' { sta::stringDelete($3); }
|	'(' PROGRAM QSTRING ')' { sta::stringDelete($3); }
|	'(' PVERSION QSTRING ')' { sta::stringDelete($3); }
|	'(' DIVIDER hchar ')' { sta::sdf_reader->setDivider($3); }
|	'(' VOLTAGE triple ')' { sta::sdf_reader->deleteTriple($3); }
|	'(' VOLTAGE NUMBER ')'
|	'(' VOLTAGE ')'  // Illegal SDF (from OC).
|	'(' PROCESS QSTRING ')' { sta::stringDelete($3); }
|	'(' PROCESS ')'  // Illegal SDF (from OC).
|	'(' TEMPERATURE NUMBER ')'
|	'(' TEMPERATURE triple ')' { sta::sdf_reader->deleteTriple($3); }
|	'(' TEMPERATURE ')' // Illegal SDF (from OC).
|	'(' TIMESCALE NUMBER ID ')'
	{ sta::sdf_reader->setTimescale($3, $4); }
;

hchar:
	'/'
	{ $$ = '/'; }
|	'.'
	{ $$ = '.'; }
;

number_opt: { $$ = NULL; }
|	NUMBER	{ $$ = new float($1); }
;

cells:
	cell
|	cells cell
;

cell:
	'(' CELL celltype cell_instance timing_specs ')'
	{ sta::sdf_reader->cellFinish(); }
;

celltype:
	'(' CELLTYPE QSTRING ')'
	{ sta::sdf_reader->setCell($3); }
;

cell_instance:
	'(' INSTANCE ')'	{ sta::sdf_reader->setInstance(NULL); }
|	'(' INSTANCE '*' ')'	{ sta::sdf_reader->setInstanceWildcard(); }
|	'(' INSTANCE path ')'
	{ sta::sdf_reader->setInstance($3); }
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
	{ sta::sdf_reader->setInIncremental(false); }
	del_defs ')'
|	'(' INCREMENTAL
	{ sta::sdf_reader->setInIncremental(true); }
	del_defs ')'
;

del_defs:
|	del_defs del_def
;

path:
	ID
|	PATH
;

del_def:
	'(' IOPATH port_spec port_instance retains delval_list ')'
	{ sta::sdf_reader->iopath($3, $4, $6, NULL, false); }
|	'(' CONDELSE '(' IOPATH port_spec port_instance
            retains delval_list ')' ')'
	{ sta::sdf_reader->iopath($5, $6, $8, NULL, true); }
|	'(' COND EXPR_OPEN_IOPATH port_spec port_instance
            retains delval_list ')' ')'
	{ sta::sdf_reader->iopath($4, $5, $7, $3, false); }
|	'(' INTERCONNECT port_instance port_instance delval_list ')'
	{ sta::sdf_reader->interconnect($3, $4, $5); }
|	'(' PORT port_instance delval_list ')'
	{ sta::sdf_reader->port($3, $4); }
|	'(' DEVICE delval_list ')'
	{ sta::sdf_reader->device($3); }
|	'(' DEVICE port_instance delval_list ')'
	{ sta::sdf_reader->device($3, $4); }
;

retains:
	/* empty */
|	retains retain
;

retain:
	'(' RETAIN delval_list ')'
	{ sta::sdf_reader->deleteTripleSeq($3); }
;

delval_list:
	value
	{ $$ = sta::sdf_reader->makeTripleSeq(); $$->push_back($1); }
|  	delval_list value
	{ $1->push_back($2); $$ = $1; }
;

tchk_defs:
|	tchk_defs tchk_def
;

tchk_def:
	'(' SETUP { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	{ sta::sdf_reader->timingCheck(sta::TimingRole::setup(), $4, $5, $6);
	  sta::sdf_reader->setInTimingCheck(false);
	}
|	'(' HOLD { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	{ sta::sdf_reader->timingCheck(sta::TimingRole::hold(), $4, $5, $6);
	  sta::sdf_reader->setInTimingCheck(false);
	}
|	'(' SETUPHOLD { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk port_tchk value value ')'
	{ sta::sdf_reader->timingCheckSetupHold($4, $5, $6, $7);
	  sta::sdf_reader->setInTimingCheck(false);
	}
//|	'(' SETUPHOLD port_spec port_spec value value scond? ccond? ')'
|	'(' RECOVERY { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	{ sta::sdf_reader->timingCheck(sta::TimingRole::recovery(),$4,$5,$6);
	  sta::sdf_reader->setInTimingCheck(false);
	}
|	'(' REMOVAL { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	{ sta::sdf_reader->timingCheck(sta::TimingRole::removal(),$4,$5,$6);
	  sta::sdf_reader->setInTimingCheck(false);
	}
|	'(' RECREM { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk port_tchk value value ')'
	{ sta::sdf_reader->timingCheckRecRem($4, $5, $6, $7);
	  sta::sdf_reader->setInTimingCheck(false);
	}
//|	'(' RECREM port_spec port_spec value value scond? ccond? ')'
|	'(' SKEW { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk port_tchk value ')'
	// Sdf skew clk/ref are reversed from liberty.
	{ sta::sdf_reader->timingCheck(sta::TimingRole::skew(),$5,$4,$6);
	  sta::sdf_reader->setInTimingCheck(false);
	}
|	'(' WIDTH { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk value ')'
	{ sta::sdf_reader->timingCheckWidth($4, $5);
	  sta::sdf_reader->setInTimingCheck(false);
	}
|	'(' PERIOD { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk value ')'	
	{ sta::sdf_reader->timingCheckPeriod($4, $5);
	  sta::sdf_reader->setInTimingCheck(false);
	}
|	'(' NOCHANGE { sta::sdf_reader->setInTimingCheck(true); }
	port_tchk port_tchk value value ')'
	{ sta::sdf_reader->timingCheckNochange($4, $5, $6, $7);
	  sta::sdf_reader->setInTimingCheck(false);
	}
;

port_instance:
	ID
|	PATH
;

port_spec:
	ID
	{ $$=sta::sdf_reader->makePortSpec(sta::Transition::riseFall(),$1,NULL); }
|	'(' port_transition ID ')'
	{ $$ = sta::sdf_reader->makePortSpec($2, $3, NULL); }
;

port_transition:
	POSEDGE	{ $$ = sta::Transition::rise(); }
|	NEGEDGE	{ $$ = sta::Transition::fall(); }
;

port_tchk:
	port_spec
|	'(' COND EXPR_ID_CLOSE
	{ $$ = sta::sdf_reader->makeCondPortSpec($3); }
|	'(' COND EXPR_OPEN port_transition ID ')' ')'
	{ $$ = sta::sdf_reader->makePortSpec($4, $5, $3); }
;

value:
	'(' ')'
	{
	  $$ = sta::sdf_reader->makeTriple();
	}
|	'(' NUMBER ')'
	{
	  $$ = sta::sdf_reader->makeTriple($2);
	}
|	'(' triple ')' { $$ = $2; }
;

triple:
	NUMBER ':' number_opt ':' number_opt
	{
	  float *fp = new float($1);
	  $$ = sta::sdf_reader->makeTriple(fp, $3, $5);
	}
|	number_opt ':' NUMBER ':' number_opt
	{
	  float *fp = new float($3);
	  $$ = sta::sdf_reader->makeTriple($1, fp, $5);
	}
|	number_opt ':' number_opt ':' NUMBER
	{
	  float *fp = new float($5);
	  $$ = sta::sdf_reader->makeTriple($1, $3, fp);
	}
;

%%
