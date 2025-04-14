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
#include <cstdlib>
#include <string>

#include "Report.hh"
#include "PortDirection.hh"
#include "VerilogReader.hh"
#include "verilog/VerilogReaderPvt.hh"
#include "verilog/VerilogScanner.hh"

#undef yylex
#define yylex scanner->lex

// warning: variable 'yynerrs_' set but not used
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define loc_line(loc) loc.begin.line

void
sta::VerilogParse::error(const location_type &loc,
                         const std::string &msg)
{
  reader->report()->fileError(164,reader->filename(),loc.begin.line,
                              "%s",msg.c_str());
}
%}

%require  "3.2"
%skeleton "lalr1.cc"
%debug
%define api.namespace {sta}
%locations
%define api.location.file "VerilogLocation.hh"
%define parse.assert
%parse-param { VerilogScanner *scanner }
%parse-param { VerilogReader *reader }
%define api.parser.class {VerilogParse}

%union {
  int ival;
  std::string *string;
  std::string *constant;
  std::string *attr_spec_value;
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
  sta::VerilogAttrEntry *attr_entry;
  sta::VerilogAttrEntrySeq *attr_seq;
  sta::VerilogAttrStmt *attr_stmt;
  sta::VerilogAttrStmtSeq *attr_stmt_seq;
}

%token INT CONSTANT ID STRING MODULE ENDMODULE ASSIGN PARAMETER DEFPARAM
%token WIRE WAND WOR TRI INPUT OUTPUT INOUT SUPPLY1 SUPPLY0 REG
%token ATTR_OPEN ATTR_CLOSED

%left '-' '+'
%left '*' '/'
%left NEG     /* negation--unary minus */

%type <string> ID STRING CONSTANT
%type <ival> WIRE WAND WOR TRI INPUT OUTPUT INOUT SUPPLY1 SUPPLY0
%type <ival> ATTR_OPEN ATTR_CLOSED
%type <ival> INT parameter_exprs parameter_expr
%type <string> attr_spec_value
%type <port_type> dcl_type port_dcl_type
%type <stmt> stmt declaration instance parameter parameter_dcls parameter_dcl
%type <stmt> defparam param_values param_value port_dcl
%type <stmt_seq> stmts stmt_seq net_assignments continuous_assign port_dcls
%type <assign> net_assignment
%type <dcl_arg> dcl_arg
%type <dcl_arg_seq> dcl_args
%type <net> port net_scalar net_bit_select net_part_select net_assign_lhs
%type <net> net_constant net_expr port_ref port_expr named_pin_net_expr
%type <net> inst_named_pin net_named net_expr_concat
%type <nets> port_list port_refs inst_ordered_pins
%type <nets> inst_named_pins net_exprs inst_pins
%type <attr_entry> attr_spec
%type <attr_seq> attr_specs
%type <attr_stmt> attr_instance
%type <attr_stmt_seq> attr_instance_seq

// Used by error recovery.
%destructor { delete $$; } STRING
%destructor { delete $$; } CONSTANT
%destructor { delete $$; } attr_spec_value

%start file

%%

file:
	modules
	;

modules:
	// empty
|	modules module
	;

module:
	attr_instance_seq MODULE ID ';' stmts ENDMODULE
	{ reader->makeModule($3, new sta::VerilogNetSeq,$5, $1, loc_line(@2));}
|	attr_instance_seq MODULE ID '(' ')' ';' stmts ENDMODULE
	{ reader->makeModule($3, new sta::VerilogNetSeq,$7, $1, loc_line(@2));}
|	attr_instance_seq MODULE ID '(' port_list ')' ';' stmts ENDMODULE
	{ reader->makeModule($3, $5, $8, $1, loc_line(@2)); }
|	attr_instance_seq MODULE ID '(' port_dcls ')' ';' stmts ENDMODULE
	{ reader->makeModule($3, $5, $8, $1, loc_line(@2)); }
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
	{ $$ = reader->makeNetNamedPortRefScalar($2, nullptr);}
|	'.' ID '(' port_expr ')'
	{ $$ = reader->makeNetNamedPortRefScalar($2, $4);}
	;

port_expr:
	port_ref
|	'{' port_refs '}'
	{ $$ = reader->makeNetConcat($2); }	;

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
	attr_instance_seq port_dcl_type dcl_arg
	{ $$ = reader->makeDcl($2, $3, $1, loc_line(@2)); }
|	attr_instance_seq port_dcl_type '[' INT ':' INT ']' dcl_arg
	{ $$ = reader->makeDclBus($2, $4, $6, $8, $1, loc_line(@2)); }
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
	{ yyerrok; $$ = nullptr; }
	;

stmt_seq:
	continuous_assign
	;

/* Parameters are parsed and ignored. */
parameter:
	PARAMETER parameter_dcls ';'
	{ $$ = nullptr; }
|	PARAMETER '[' INT ':' INT ']' parameter_dcls ';'
	{ $$ = nullptr; }
	;

parameter_dcls:
	parameter_dcl
	{ $$ = nullptr; }
|	parameter_dcls ',' parameter_dcl
	{ $$ = nullptr; }
	;

parameter_dcl:
	ID '=' parameter_expr
	{ delete $1; $$ = nullptr; }
|	ID '=' STRING
	{ delete $1; delete $3; $$ = nullptr; }
;

parameter_expr:
	ID
	{ delete $1; $$ = 0; }
|	'`' ID
	{ delete $2; $$ = 0; }
|	CONSTANT
	{ delete $1; $$ = 0; }
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
	{ $$ = nullptr; }
	;

param_values:
	param_value
	{ $$ = nullptr; }
|	param_values ',' param_value
	{ $$ = nullptr; }
	;

param_value:
	ID '=' parameter_expr
	{ delete $1; $$ = nullptr; }
|	ID '=' STRING
	{ delete $1; delete $3; $$ = nullptr; }
	;

declaration:
	attr_instance_seq dcl_type dcl_args ';'
	{ $$ = reader->makeDcl($2, $3, $1, loc_line(@2)); }
|	attr_instance_seq dcl_type '[' INT ':' INT ']' dcl_args ';'
	{ $$ = reader->makeDclBus($2, $4, $6, $8, $1,loc_line(@2)); }
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
	{ $$ = reader->makeDclArg($1); }
|	net_assignment
	{ $$ = reader->makeDclArg($1); }
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
	net_assign_lhs '=' net_expr
	{ $$ = reader->makeAssign($1, $3, loc_line(@1)); }
	;

net_assign_lhs:
        net_named
        | net_expr_concat
        ;

instance:
	attr_instance_seq ID ID '(' inst_pins ')' ';'
	{ $$ = reader->makeModuleInst($2, $3, $5, $1, loc_line(@2)); }
|	attr_instance_seq ID parameter_values ID '(' inst_pins ')' ';'
	{ $$ = reader->makeModuleInst($2, $4, $6, $1, loc_line(@2)); }
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
	{ $$ = nullptr; }
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
	{ $$ = reader->makeNetNamedPortRefScalarNet($2); }
|	'.' ID '(' ID ')'
	{ $$ = reader->makeNetNamedPortRefScalarNet($2, $4); }
|	'.' ID '(' ID '[' INT ']' ')'
	{ $$ = reader->makeNetNamedPortRefBitSelect($2, $4, $6); }
|	'.' ID '(' named_pin_net_expr ')'
	{ $$ = reader->makeNetNamedPortRefScalar($2, $4); }
//      Bus port bit select.
|	'.' ID '[' INT ']' '(' ')'
	{ $$ = reader->makeNetNamedPortRefBit($2, $4, nullptr); }
|	'.' ID '[' INT ']' '(' net_expr ')'
	{ $$ = reader->makeNetNamedPortRefBit($2, $4, $7); }
//      Bus port part select.
|	'.'  ID '[' INT ':' INT ']' '(' ')'
	{ $$ = reader->makeNetNamedPortRefPart($2, $4, $6, nullptr); }
|	'.'  ID '[' INT ':' INT ']' '(' net_expr ')'
	{ $$ = reader->makeNetNamedPortRefPart($2, $4, $6, $9); }
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
	{ $$ = reader->makeNetScalar($1); }
	;

net_bit_select:
	ID '[' INT ']'
	{ $$ = reader->makeNetBitSelect($1, $3); }
	;

net_part_select:
	ID '[' INT ':' INT ']'
	{ $$ = reader->makeNetPartSelect($1, $3, $5); }
	;

net_constant:
	CONSTANT
	{ $$ = reader->makeNetConstant($1, loc_line(@1)); }
	;

net_expr_concat:
	'{' net_exprs '}'
	{ $$ = reader->makeNetConcat($2); }
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

attr_instance_seq:
	// empty
	{ $$ = new sta::VerilogAttrStmtSeq; }
|	attr_instance_seq attr_instance
	{ if ($2) $1->push_back($2); }
	;

attr_instance:
	ATTR_OPEN attr_specs ATTR_CLOSED
	{ $$ = new sta::VerilogAttrStmt($2); }
	;

attr_specs:
	attr_spec
	{ $$ = new sta::VerilogAttrEntrySeq;
	  $$->push_back($1);
	}
|       attr_specs ',' attr_spec
	{ $$->push_back($3); }
	;

attr_spec:
	ID
	{ $$ = new sta::VerilogAttrEntry(*$1, "1"); delete $1; }
| 	ID '=' attr_spec_value
	{ $$ = new sta::VerilogAttrEntry(*$1, *$3); delete $1; delete $3; }
	;

attr_spec_value:
	CONSTANT
	{ $$ = $1; }
| 	STRING
	{ $$ = $1; }
| 	INT
  	{ $$ = new std::string(std::to_string($1)); }
	;

%%
