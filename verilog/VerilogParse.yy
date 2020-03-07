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

#include <stdlib.h>
#include "Machine.hh"
#include "PortDirection.hh"
#include "VerilogReaderPvt.hh"
#include "VerilogReader.hh"

int VerilogLex_lex();
#define VerilogParse_lex VerilogLex_lex
// Use yacc generated parser errors.
#define YYERROR_VERBOSE

%}

%union{
  int ival;
  const char *string;
  const char *constant;
  sta::VerilogModule *module;
  sta::VerilogStmt *stmt;
  sta::VerilogStmtSeq *stmt_seq;
  sta::PortDirection *port_type;
  sta::VerilogDclArgSeq *dcl_arg_seq;
  sta::VerilogDclArg *dcl_arg;
  sta::VerilogAssign *assign;
  sta::VerilogInst *inst;
  sta::VerilogNet *net;
  sta::VerilogNetBitSelect *net_bit;
  sta::VerilogNetSeq *nets;
}

%token INT CONSTANT ID STRING MODULE ENDMODULE ASSIGN PARAMETER DEFPARAM
%token WIRE WAND WOR TRI INPUT OUTPUT INOUT SUPPLY1 SUPPLY0 REG

%left '-' '+'
%left '*' '/'
%left NEG     /* negation--unary minus */

%type <string> ID STRING
%type <ival> WIRE WAND WOR TRI INPUT OUTPUT INOUT SUPPLY1 SUPPLY0
%type <ival> INT parameter_exprs parameter_expr module_begin
%type <constant> CONSTANT
%type <port_type> dcl_type port_dcl_type
%type <stmt> stmt declaration instance parameter parameter_dcls parameter_dcl
%type <stmt> defparam param_values param_value port_dcl
%type <stmt_seq> stmts stmt_seq net_assignments continuous_assign port_dcls
%type <assign> net_assignment
%type <dcl_arg> dcl_arg
%type <dcl_arg_seq> dcl_args
%type <net> port net_scalar net_bit_select net_part_select
%type <net> net_constant net_expr port_ref port_expr named_pin_net_expr
%type <net> inst_named_pin net_named net_expr_concat
%type <nets> port_list port_refs inst_ordered_pins
%type <nets> inst_named_pins net_exprs inst_pins

%start file

%{
%}

%%

file:
	modules
	;

modules:
	// empty
|	modules module
	;

module_begin:
	MODULE { $<ival>$ = sta::verilog_reader->line(); }
	{ $$ = $<ival>2; }
	;

module:
	module_begin ID ';' stmts ENDMODULE
	{ sta::verilog_reader->makeModule($2, new sta::VerilogNetSeq,$4,$1);}
|	module_begin ID '(' ')' ';' stmts ENDMODULE
	{ sta::verilog_reader->makeModule($2, new sta::VerilogNetSeq,$6,$1);}
|	module_begin ID '(' port_list ')' ';' stmts ENDMODULE
	{ sta::verilog_reader->makeModule($2, $4, $7, $1); }
|	module_begin ID '(' port_dcls ')' ';' stmts ENDMODULE
	{ sta::verilog_reader->makeModule($2, $4, $7, $1); }
	;

port_list:
	port
	{ $$ = new sta::VerilogNetSeq;
	  $$->push_back($1);
	}
|	port_list ',' port
	{ $1->push_back($3); }
	;

port:
	port_expr
|	'.' ID '(' ')'
	{ $$=sta::verilog_reader->makeNetNamedPortRefScalar($2, NULL);}
|	'.' ID '(' port_expr ')'
	{ $$=sta::verilog_reader->makeNetNamedPortRefScalar($2, $4);}
	;

port_expr:
	port_ref
|	'{' port_refs '}'
	{ $$ = sta::verilog_reader->makeNetConcat($2); }	;

port_refs:
	port_ref
	{ $$ = new sta::VerilogNetSeq;
	  $$->push_back($1);
	}
|	port_refs ',' port_ref
	{ $1->push_back($3); }
	;

port_ref:
	net_scalar
|	net_bit_select
|	net_part_select
	;

port_dcls:
	port_dcl
	{ $$ = new sta::VerilogStmtSeq;
	  $$->push_back($1);
	}
|	port_dcls ',' port_dcl
	{ $$ = $1;
	  $1->push_back($3);
	}
|	port_dcls ',' dcl_arg
	{
	  sta::VerilogDcl *dcl = dynamic_cast<sta::VerilogDcl*>($1->back());
	  dcl->appendArg($3);
	  $$ = $1;
	}
	;

port_dcl:
	port_dcl_type { $<ival>$ = sta::verilog_reader->line(); } dcl_arg
	{ $$ = sta::verilog_reader->makeDcl($1, $3, $<ival>2); }
|	port_dcl_type { $<ival>$ = sta::verilog_reader->line(); }
	'[' INT ':' INT ']' dcl_arg
	{ $$ = sta::verilog_reader->makeDclBus($1, $4, $6, $8, $<ival>2); }
	;

port_dcl_type:
	INPUT { $$ = sta::PortDirection::input(); }
|	INPUT WIRE { $$ = sta::PortDirection::input(); }
|	INOUT { $$ = sta::PortDirection::bidirect(); }
|	INOUT REG { $$ = sta::PortDirection::bidirect(); }
|	INOUT WIRE { $$ = sta::PortDirection::bidirect(); }
|	OUTPUT { $$ = sta::PortDirection::output(); }
|	OUTPUT WIRE { $$ = sta::PortDirection::output(); }
|	OUTPUT REG { $$ = sta::PortDirection::output(); }
	;

stmts:
	// empty
	{ $$ = new sta::VerilogStmtSeq; }
|	stmts stmt
	{ if ($2) $1->push_back($2); }
|	stmts stmt_seq
	// Append stmt_seq to stmts.
	{ sta::VerilogStmtSeq::Iterator iter($2);
	  while (iter.hasNext())
	    $1->push_back(iter.next());
	  delete $2;
	}
	;

stmt:
	parameter
|	defparam
|	declaration
|	instance
|	error ';'
	{ yyerrok; $$ = NULL; }
	;

stmt_seq:
	continuous_assign
	;

/* Parameters are parsed and ignored. */
parameter:
	PARAMETER parameter_dcls ';'
	{ $$ = NULL; }
|	PARAMETER '[' INT ':' INT ']' parameter_dcls ';'
	{ $$ = NULL; }
	;

parameter_dcls:
	parameter_dcl
	{ $$ = NULL; }
|	parameter_dcls ',' parameter_dcl
	{ $$ = NULL; }
	;

parameter_dcl:
	ID '=' parameter_expr
	{ sta::stringDelete($1);
	  $$ = NULL;
	}
|	ID '=' STRING
	{ sta::stringDelete($1);
	  sta::stringDelete($3);
	  $$ = NULL;
	}
;

parameter_expr:
	ID
	{ sta::stringDelete($1);
	  $$ = 0;
	}
|	'`' ID
	{ sta::stringDelete($2);
	  $$ = 0;
	}
|	CONSTANT
	{ sta::stringDelete($1);
	  $$ = 0;
	}
|	INT
|	'-' parameter_expr %prec NEG
	{ $$ = - $2; }
|	parameter_expr '+' parameter_expr
	{ $$ = $1 + $3; }
|	parameter_expr '-' parameter_expr
	{ $$ = $1 - $3; }
|	parameter_expr '*' parameter_expr
	{ $$ = $1 * $3; }
|	parameter_expr '/' parameter_expr
	{ $$ = $1 / $3; }
|	'(' parameter_expr ')'
	{ $$ = $2; }
	;

defparam:
	DEFPARAM param_values ';'
	{ $$ = NULL; }
	;

param_values:
	param_value
	{ $$ = NULL; }
|	param_values ',' param_value
	{ $$ = NULL; }
	;

param_value:
	ID '=' parameter_expr
	{ sta::stringDelete($1);
	  $$ = NULL;
	}
|	ID '=' STRING
	{ sta::stringDelete($1);
	  sta::stringDelete($3);
	  $$ = NULL;
	}
	;

declaration:
	dcl_type { $<ival>$ = sta::verilog_reader->line(); } dcl_args ';'
	{ $$ = sta::verilog_reader->makeDcl($1, $3, $<ival>2); }
|	dcl_type { $<ival>$ = sta::verilog_reader->line(); }
	'[' INT ':' INT ']' dcl_args ';'
	{ $$ = sta::verilog_reader->makeDclBus($1, $4, $6, $8, $<ival>2); }
	;

dcl_type:
	INPUT { $$ = sta::PortDirection::input(); }
|	INOUT { $$ = sta::PortDirection::bidirect(); }
|	OUTPUT { $$ = sta::PortDirection::output(); }
|	SUPPLY0 { $$ = sta::PortDirection::ground(); }
|	SUPPLY1 { $$ = sta::PortDirection::power(); }
|	TRI { $$ = sta::PortDirection::tristate(); }
|	WAND { $$ = sta::PortDirection::internal(); }
|	WIRE { $$ = sta::PortDirection::internal(); }
|	WOR { $$ = sta::PortDirection::internal(); }
	;

dcl_args:
	dcl_arg
	{ $$ = new sta::VerilogDclArgSeq;
	  $$->push_back($1);
	}
|	dcl_args ',' dcl_arg
	{ $1->push_back($3);
	  $$ = $1;
	}
	;

dcl_arg:
	ID
	{ $$ = sta::verilog_reader->makeDclArg($1); }
|	net_assignment
	{ $$ = sta::verilog_reader->makeDclArg($1); }
	;

continuous_assign:
	ASSIGN net_assignments ';'
	{ $$ = $2; }
	;

net_assignments:
	net_assignment
	{ $$ = new sta::VerilogStmtSeq();
	  $$->push_back($1);
	}
|	net_assignments ',' net_assignment
	{ $1->push_back($3); }
	;

net_assignment:
	net_named { $<ival>$ = sta::verilog_reader->line(); } '=' net_expr
	{ $$ = sta::verilog_reader->makeAssign($1, $4, $<ival>2); }
	;

instance:
	ID { $<ival>$ = sta::verilog_reader->line(); } ID '(' inst_pins ')' ';'
	{ $$ = sta::verilog_reader->makeModuleInst($1, $3, $5, $<ival>2); }
|	ID { $<ival>$ = sta::verilog_reader->line(); } parameter_values
	   ID '(' inst_pins ')' ';'
	{ $$ = sta::verilog_reader->makeModuleInst($1, $4, $6, $<ival>2); }
	;

parameter_values:
	'#' '(' parameter_exprs ')'
	;

parameter_exprs:
	parameter_expr
|	'{' parameter_exprs '}'
	{ $$ = $2; }
|	parameter_exprs ',' parameter_expr
	;

inst_pins:
	// empty
	{ $$ = NULL; }
|	inst_ordered_pins
|	inst_named_pins
	;

// Positional pin connections.
inst_ordered_pins:
	net_expr
	{ $$ = new sta::VerilogNetSeq;
	  $$->push_back($1);
	}
|	inst_ordered_pins ',' net_expr
	{ $1->push_back($3); }
	;

// Named pin connections.
inst_named_pins:
	inst_named_pin
	{ $$ = new sta::VerilogNetSeq;
	  $$->push_back($1);
	}
|	inst_named_pins ',' inst_named_pin
	{ $1->push_back($3); }
	;

// The port reference is split out into cases to special case
// the most frequent case of .port_scalar(net_scalar).
inst_named_pin:
//      Scalar port.
	'.' ID '(' ')'
	{ $$ = sta::verilog_reader->makeNetNamedPortRefScalarNet($2, NULL); }
|	'.' ID '(' ID ')'
	{ $$ = sta::verilog_reader->makeNetNamedPortRefScalarNet($2, $4); }
|	'.' ID '(' ID '[' INT ']' ')'
	{ $$ = sta::verilog_reader->makeNetNamedPortRefBitSelect($2, $4, $6); }
|	'.' ID '(' named_pin_net_expr ')'
	{ $$ = sta::verilog_reader->makeNetNamedPortRefScalar($2, $4); }
//      Bus port bit select.
|	'.' ID '[' INT ']' '(' ')'
	{ $$ = sta::verilog_reader->makeNetNamedPortRefBit($2, $4, NULL); }
|	'.' ID '[' INT ']' '(' net_expr ')'
	{ $$ = sta::verilog_reader->makeNetNamedPortRefBit($2, $4, $7); }
//      Bus port part select.
|	'.'  ID '[' INT ':' INT ']' '(' ')'
	{ $$ = sta::verilog_reader->makeNetNamedPortRefPart($2, $4, $6, NULL); }
|	'.'  ID '[' INT ':' INT ']' '(' net_expr ')'
	{ $$ = sta::verilog_reader->makeNetNamedPortRefPart($2, $4, $6, $9); }
	;

named_pin_net_expr:
	net_part_select
|	net_constant
|	net_expr_concat
	;

net_named:
	net_scalar
|	net_bit_select
|	net_part_select
	;

net_scalar:
	ID
	{ $$ = sta::verilog_reader->makeNetScalar($1); }
	;

net_bit_select:
	ID '[' INT ']'
	{ $$ = sta::verilog_reader->makeNetBitSelect($1, $3); }
	;

net_part_select:
	ID '[' INT ':' INT ']'
	{ $$ = sta::verilog_reader->makeNetPartSelect($1, $3, $5); }
	;

net_constant:
	CONSTANT
	{ $$ = sta::verilog_reader->makeNetConstant($1); }
	;

net_expr_concat:
	'{' net_exprs '}'
	{ $$ = sta::verilog_reader->makeNetConcat($2); }
	;

net_exprs:
	net_expr
	{ $$ = new sta::VerilogNetSeq;
	  $$->push_back($1);
	}
|	net_exprs ',' net_expr
	{ $$->push_back($3); }
	;

net_expr:
	net_scalar
|	net_bit_select
|	net_part_select
|	net_constant
|	net_expr_concat
	;

%%
