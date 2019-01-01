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

#include "Machine.hh"
#include "StringUtil.hh"
#include "SpfReaderPvt.hh"

int SpfLex_lex();
#define SpfParse_lex SpfLex_lex
// Use yacc generated parser errors.
#define YYERROR_VERBOSE

%}

%union {
  char ch;
  const char *string;
  int integer;
  float number;
}

%token RSPF DSPF DESIGN DATE VENDOR PROGRAM PVERSION DIVIDER DELIMITER BUSBIT
%token GROUND_NET NET PINDEF INSTPIN SUBNODE DRIVER LOAD
%token SUBCKT ENDS SUBCKT_CALL CAPACITOR RESISTOR VCVS
%token END
%token NUMBER ENUMBER INUMBER CVALUE QSTRING ID PATH

%type <integer> INUMBER
%type <number> NUMBER ENUMBER CVALUE
%type <string> QSTRING ID PATH

%type <number> net_cap pin_cap cvalue rvalue vvalue
%type <number> rc_pair rc_pairs coord coords
%type <string> inst_name net_name subnode_name instpin_name pin_name
%type <string> pin_type element_name element_node id_or_inumber
%type <ch> hchar

%start file

%{
%}

%%

file:
	dspf_file
|	rspf_file
;

////////////////////////////////////////////////////////////////
//
// DSPF
//
////////////////////////////////////////////////////////////////

dspf_file:
	header dspf_subckt end
;

dspf_subckt:
	subckt_def
	gnet_defs
	dspf_blocks
	end_subckt
;

dspf_blocks:
        dspf_block
|       dspf_blocks dspf_block
;

dspf_block:
        dspf_net_block
|       inst_block
;

dspf_net_block:
	net_def
	net_node_defs
	rc_defs
	{ sta::spf_reader->dspfNetFinish(); }
;

net_node_defs:
	net_node_def
|	net_node_defs net_node_def
;

net_node_def:
	pin_def
|	instpin_def
|	subnode_def
;

pin_def:
	PINDEF pin_elements
;

pin_elements:
	pin_element
|	pin_elements pin_element
;

pin_element:
	'(' pin_name pin_type pin_cap coords ')'
	{ sta::spf_reader->dspfPinDef($2, $3); }
;

instpin_def:
	INSTPIN instpin_elements
;

instpin_elements:
	instpin_element
|	instpin_elements instpin_element
;

instpin_element:
	'(' instpin_name inst_name pin_name pin_type pin_cap coords ')'
	{ sta::spf_reader->dspfInstPinDef($2, $3, $4, $5); }
;

////////////////////////////////////////////////////////////////
//
// RSPF
//
////////////////////////////////////////////////////////////////

rspf_file:
	header rspf_subckt end
;

rspf_subckt:
	subckt_def
	gnet_defs
	rspf_net_blocks
	inst_blocks
	end_subckt
;

rspf_net_blocks:
	rspf_net_block
|	rspf_net_blocks rspf_net_block
;

rspf_net_block:
	net_def
	dl_blocks
	{ sta::spf_reader->rspfNetFinish(); }
;

dl_blocks:
	/* empty */
|	dl_block
;

dl_block:
	driver_block
	load_blocks
;

load_blocks:
	/* empty */
|	load_blocks load_block
;

driver_block:
	driver_def rc_defs
	{ sta::spf_reader->rspfDrvrFinish(); }
|	driver_def subnode_def rc_defs
	{ sta::spf_reader->rspfDrvrFinish(); }
;

driver_def:
	DRIVER pin_name
	{ sta::spf_reader->rspfDrvrBegin($2); }
|	DRIVER instpin_name inst_name pin_name
	{ sta::spf_reader->rspfDrvrBegin($2, $3, $4); }
;

load_block:
	load_def rcv_defs
	{ sta::spf_reader->rspfLoadFinish(); }
|	load_def subnode_def rcv_defs
	{ sta::spf_reader->rspfLoadFinish(); }
;

load_def:
	LOAD pin_name
	{ sta::spf_reader->rspfLoadBegin($2); }
|	LOAD instpin_name inst_name pin_name
	{ sta::spf_reader->rspfLoadBegin($2, $3, $4); }
;

////////////////////////////////////////////////////////////////
//
// RSPF/DSPF common
//
////////////////////////////////////////////////////////////////

header:
	design_name
	date
	vendor
	program_name
	program_version
	hierarchy_divider
	name_delimiter
	busbit_chars
;

design_name:
	/* empty */
|	DESIGN QSTRING
	{ sta::stringDelete($2); }
|	DESIGN ID
	{ sta::stringDelete($2); }
;

date:
	/* empty */
|	DATE QSTRING
	{ sta::stringDelete($2); }
;

vendor:
	/* empty */
|	VENDOR QSTRING
	{ sta::stringDelete($2); }
;

program_name:
	/* empty */
|	PROGRAM QSTRING
	{ sta::stringDelete($2); }
;

program_version:
	/* empty */
|	PVERSION QSTRING
	{ sta::stringDelete($2); }
;

hierarchy_divider:
	/* empty */
|	DIVIDER hchar
	{ sta::spf_reader->setDivider($2); }
;
	
name_delimiter:
	/* empty */
|	DELIMITER hchar
	{ sta::spf_reader->setDelimiter($2); }
;

busbit_chars:
	/* empty */
|	BUSBIT hchar hchar
	{ sta::spf_reader->setBusBrackets($2, $3); }
;

end:
	/* empty */
|	END
;

subckt_def:
	SUBCKT ID subckt_ports
	{ sta::stringDelete($2); }

;

subckt_ports:
	/* empty */
|	subckt_ports ID
	{ sta::stringDelete($2); }
;

gnet_defs:
	gnet_def
|	gnet_defs gnet_def
;

gnet_def:
	GROUND_NET id_or_inumber
	{ sta::spf_reader->setGroundNet($2); }
;

subnode_def:
	SUBNODE subnode_elements
;

subnode_elements:
	subnode_element
|	subnode_elements subnode_element
;

subnode_element:
	'(' subnode_name coords ')'
	{ sta::spf_reader->subnodeDef($2); }
;

element_node:
	id_or_inumber
|	PATH
;

element_name:
	id_or_inumber
;

rc_defs:
	/* empty */
|	rc_defs resistor_def
|	rc_defs capacitor_def
;

resistor_def:
	RESISTOR element_name element_node element_node rvalue
	{ sta::spf_reader->resistor($2, $3, $4, $5); }
;

capacitor_def:
	CAPACITOR element_name element_node element_node cvalue
	{ sta::spf_reader->capacitor($2, $3, $4, $5); }
;

vcvs_def:
	VCVS element_name
	element_node element_node
	element_node element_node vvalue
	{ sta::stringDelete($2);
	  sta::stringDelete($3);
	  sta::stringDelete($4);
	  sta::stringDelete($5);
	  sta::stringDelete($6);
	}
;

rcv_defs:
	/* empty */
|	rcv_defs resistor_def
|	rcv_defs capacitor_def
|	rcv_defs vcvs_def
;

inst_blocks:
	/* empty */
|	inst_blocks inst_block
;

inst_block:
	SUBCKT_CALL inst_block_nodes ID
	{ sta::stringDelete($3); }
;

inst_block_nodes:
	/* empty */
|	inst_block_nodes net_name
	{ sta::stringDelete($2); }
;

end_subckt:
	ENDS
;

net_def:
	NET net_name net_cap
	{ sta::spf_reader->netBegin($2); }
;

instpin_name:
	PATH
;

subnode_name:
	PATH
;

net_name:
	ID
|	PATH
;

inst_name:
	ID
|	PATH
;
	
net_cap:
	cvalue
;

pin_name:
	ID
;

pin_type:
	ID
;

id_or_inumber:
	ID
|	INUMBER
	{ $$ = sta::integerString($1); }
;

pin_cap:
	cvalue
|	cvalue '(' rc_pairs ')'
;

rc_pairs:
	rc_pair
|	rc_pairs rc_pair
;

rc_pair:
	rvalue cvalue
;

coords:
	/* empty */
	{ $$ = 0; }
|	coord coord
;

coord:
	INUMBER
	{ $$ = static_cast<float>($1); }
|	'-' INUMBER
	{ $$ = -static_cast<float>($2); }
|	NUMBER
|	'-' NUMBER
	{ $$ = $2; }
;

cvalue:
	INUMBER
	{ $$ = static_cast<float>($1); }
|	NUMBER
|	ENUMBER
|	CVALUE
;

rvalue:
	INUMBER
	{ $$ = static_cast<float>($1); }
|	NUMBER
|	ENUMBER
;

vvalue:
	INUMBER
	{ $$ = static_cast<float>($1); }
|	NUMBER
|	ENUMBER
;

hchar:
	'.'
	{ $$ = '.'; }
|	'/'
	{ $$ = '/'; }
|	'|'
	{ $$ = '|'; }
|	':'
	{ $$ = ':'; }
|	'['
	{ $$ = '['; }
|	']'
	{ $$ = ']'; }
|	'<'
	{ $$ = '<'; }
|	'>'
	{ $$ = '>'; }
|	'('
	{ $$ = '('; }
|	')'
	{ $$ = ')'; }
;

%%
