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

#include <string.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "StringSeq.hh"
#include "SpefReaderPvt.hh"

int SpefLex_lex();
#define SpefParse_lex SpefLex_lex
// use yacc generated parser errors
#define YYERROR_VERBOSE

%}

%union {
  char ch;
  char *string;
  int integer;
  float number;
  sta::StringSeq *string_seq;
  sta::PortDirection *port_dir;
  sta::SpefRspfPi *pi;
  sta::SpefTriple *triple;
  sta::Pin *pin;
  sta::Net *net;
}

%token SPEF DESIGN DATE VENDOR PROGRAM DESIGN_FLOW
%token PVERSION DIVIDER DELIMITER
%token BUS_DELIMITER T_UNIT C_UNIT R_UNIT L_UNIT NAME_MAP
%token POWER_NETS GROUND_NETS KW_C KW_L KW_S KW_D KW_V
%token PORTS PHYSICAL_PORTS DEFINE PDEFINE
%token D_NET D_PNET R_NET R_PNET END
%token CONN CAP RES INDUC KW_P KW_I KW_N DRIVER CELL C2_R1_C1 LOADS
%token RC KW_Q KW_K

%token INTEGER FLOAT QSTRING INDEX IDENT NAME

%type <integer> INTEGER
%type <number> FLOAT
%type <string> QSTRING INDEX IDENT NAME

%type <integer> conf cap_id res_id induc_id cap_elem cap_elems
%type <integer> res_elem res_elems induc_elem induc_elems
%type <integer> pos_integer

%type <number> number pos_number threshold
%type <number> real_component imaginary_component pole poles
%type <number> residue residues complex_par_value cnumber

%type <string> name_or_index net_name net_names inst_name
%type <string> name_map_entries name_map_entry mapped_item
%type <string> physical_inst port_name pport_name port_entry pport_entry
%type <string> port_entries pport_entries pport
%type <string> entity external_connection
%type <string> cell_type
%type <string> driver_cell pnet_ref
%type <string> pexternal_connection internal_pdspf_node
%type <string> parasitic_node internal_parasitic_node

%type <ch> hchar suffix_bus_delim prefix_bus_delim

%type <string_seq> qstrings
%type<port_dir> direction

%type<triple> par_value total_cap

%type<pi> pi_model

%type<pin> pin_name driver_pair internal_connection

%type<net> net

%start file

%{
%}

%%

file:
	header_def
	name_map
	power_def
	external_def
	define_def
	internal_def
;

/****************************************************************/

prefix_bus_delim:
	'['
	{ $$ = '['; }
|	'{'
	{ $$ = '}'; }
|	'('
	{ $$ = ')'; }
|	'<'
	{ $$ = '<'; }
;

suffix_bus_delim:
	']'
	{ $$ = ']'; }
|	'}'
	{ $$ = '}'; }
|	')'
	{ $$ = ')'; }
|	'>'
	{ $$ = '>'; }
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
;

/****************************************************************/

header_def:
	spef_version
	design_name
	date
	vendor
	program_name
	program_version
	design_flow
	hierarchy_div_def
	pin_delim_def
	bus_delim_def
	unit_def
;

spef_version:
	SPEF QSTRING
	{ sta::stringDelete($2); }
;

design_name:
	DESIGN QSTRING
	{ sta::stringDelete($2); }
;

date:
	DATE QSTRING
	{ sta::stringDelete($2); }
;

program_name:
	PROGRAM QSTRING
	{ sta::stringDelete($2); }
;

program_version:
	PVERSION QSTRING
	{ sta::stringDelete($2); }
;

vendor:
	VENDOR QSTRING
	{ sta::stringDelete($2); }
;

design_flow:
	DESIGN_FLOW qstrings
	{ sta::spef_reader->setDesignFlow($2); }
;

qstrings:
	QSTRING
	{ $$ = new sta::StringSeq;
	  $$->push_back($1);
	}
|	qstrings QSTRING
	{ $$->push_back($2); }
;

hierarchy_div_def:
	DIVIDER hchar
	{ sta::spef_reader->setDivider($2); }
;

pin_delim_def:
	DELIMITER hchar
	{ sta::spef_reader->setDelimiter($2); }
;

bus_delim_def:
	BUS_DELIMITER prefix_bus_delim
	{ sta::spef_reader->setBusBrackets($2, '\0'); }
|	BUS_DELIMITER prefix_bus_delim suffix_bus_delim
	{ sta::spef_reader->setBusBrackets($2, $3); }
;

/****************************************************************/

unit_def:
	time_scale
	cap_scale
	res_scale
	induc_scale
;

time_scale:
	T_UNIT pos_number IDENT
	{ sta::spef_reader->setTimeScale($2, $3); }
;

cap_scale:
	C_UNIT pos_number IDENT
	{ sta::spef_reader->setCapScale($2, $3); }
;

res_scale:
	R_UNIT pos_number IDENT
	{ sta::spef_reader->setResScale($2, $3); }
;

induc_scale:
	L_UNIT pos_number IDENT
	{ sta::spef_reader->setInductScale($2, $3); }
;

/****************************************************************/

name_map:
	/* empty */
|	NAME_MAP name_map_entries
;

name_map_entries:
	name_map_entry
|	name_map_entries name_map_entry
;

name_map_entry:
	INDEX mapped_item
	{ sta::spef_reader->makeNameMapEntry($1, $2);
	  sta::stringDelete($1);
	}
;

mapped_item:
	IDENT
|	NAME
|	QSTRING
;

/****************************************************************/

power_def:
	/* empty */
|	power_net_def
|	ground_net_def
|	power_net_def ground_net_def
;

power_net_def:
	POWER_NETS net_names
;

ground_net_def:
	GROUND_NETS net_names
;

net_names:
	net_name
|	net_names net_name
;

net_name:
	name_or_index
	{ sta::stringDelete($1); }
;

/****************************************************************/

external_def:
	/* empty */
|	port_def
|	physical_port_def
|	port_def physical_port_def
;

port_def:
	PORTS port_entries
;

port_entries:
	port_entry
|	port_entries port_entry
;

port_entry:
	port_name direction conn_attrs
	{ sta::stringDelete($1); }
;

direction:
	IDENT
	{ $$ = sta::spef_reader->portDirection($1);
          sta::stringDelete($1);
	}
;

port_name:
	name_or_index
;

inst_name:
	name_or_index
;

physical_port_def:
	PHYSICAL_PORTS pport_entries
;

pport_entries:
	pport_entry
|	pport_entries pport_entry
;

pport_entry:
	pport_name IDENT conn_attrs
;

pport_name:
	name_or_index
	{ sta::stringDelete($1); }
|	physical_inst ':' pport
	{ sta::stringDelete($1);
	  sta::stringDelete($3);
	}
;

pport:
	name_or_index
;

physical_inst:
	name_or_index
;

/****************************************************************/

conn_attrs:
	/* empty */
|	conn_attrs conn_attr
;

conn_attr:
	coordinates
|	cap_load
|	slews
|	driving_cell
;

coordinates:
	KW_C number number
;

cap_load:
	KW_L par_value
	{ delete $2; }
;

par_value:
	number
	{ $$ = new sta::SpefTriple($1); }
|	number ':' number ':' number
	{ $$ = new sta::SpefTriple($1, $3, $5); }
;

slews:
	KW_S par_value par_value
	{ delete $2;
	  delete $3;
	}
|	KW_S par_value par_value threshold threshold
	{ delete $2;
	  delete $3;
	}
;

threshold:
	pos_number
|	pos_number ':' pos_number ':' pos_number
;

driving_cell:
	KW_D cell_type
	{ sta::stringDelete($2); }
;

cell_type:
	IDENT
|	INDEX
;

/****************************************************************/

define_def:
	/* empty */
|	define_def define_entry
;

define_entry:
	DEFINE inst_name entity
	{ sta::stringDelete($2);
	  sta::stringDelete($3);
	}
|	DEFINE inst_name inst_name entity
	{ sta::stringDelete($2);
	  sta::stringDelete($3);
	  sta::stringDelete($4);
	}
|	PDEFINE physical_inst entity
	{ sta::stringDelete($2);
	  sta::stringDelete($3);
	}
;

entity:
	QSTRING
;

/****************************************************************/

internal_def:
	nets
|	internal_def nets
;

nets:
	d_net
|	r_net
|	d_pnet
|	r_pnet
;

/****************************************************************/

d_net:
	D_NET net total_cap
	{ sta::spef_reader->dspfBegin($2, $3); }
	routing_conf conn_sec cap_sec res_sec induc_sec END
	{ sta::spef_reader->dspfFinish(); }
;

net:
	name_or_index
	{ $$ = sta::spef_reader->findNet($1);
	  sta::stringDelete($1);
	}
;

total_cap:
	par_value
;

routing_conf:
	/* empty */
|	KW_V conf
;

conf:
	pos_integer
;

/****************************************************************/

conn_sec:
	/* empty */
|	CONN conn_defs internal_node_coords
;

conn_defs:
	conn_def
|	conn_defs conn_def
;

conn_def:
	KW_P external_connection direction conn_attrs
|	KW_I internal_connection direction conn_attrs
;

external_connection:
	name_or_index
	{ sta::stringDelete($1); }
|	physical_inst ':' pport
	{ sta::stringDelete($1);
	  sta::stringDelete($3);
	}
;

internal_connection:
	pin_name
;

pin_name:
	name_or_index
	{ $$ = sta::spef_reader->findPin($1);
	  sta::stringDelete($1);
	}
;

internal_node_coords:
	/* empty */
|	internal_node_coords internal_node_coord
;

internal_node_coord:
	KW_N internal_parasitic_node coordinates
;

internal_parasitic_node:
	name_or_index
	{ sta::stringDelete($1); }
;

/****************************************************************/

cap_sec:
	/* empty */
|	CAP cap_elems
;

cap_elems:
	/* empty */
	{ $$ = 0; }
|	cap_elems cap_elem
;

cap_elem:
	cap_id parasitic_node par_value
	{ sta::spef_reader->makeCapacitor($1, $2, $3); }
|	cap_id parasitic_node parasitic_node par_value
	{ sta::spef_reader->makeCapacitor($1, $2, $3, $4); }
;

cap_id:
	pos_integer
;

parasitic_node:
	name_or_index
;

/****************************************************************/

res_sec:
	/* empty */
|	RES res_elems
;

res_elems:
	/* empty */
	{ $$ = 0; }
|	res_elems res_elem
;

res_elem:
	res_id parasitic_node parasitic_node par_value
	{ sta::spef_reader->makeResistor($1, $2, $3, $4); }
;

res_id:
	pos_integer
;

/****************************************************************/

induc_sec:
	/* empty */
|	INDUC induc_elems
;

induc_elems:
	/* empty */
	{ $$ = 0; }
|	induc_elems induc_elem
;

induc_elem:
	induc_id parasitic_node parasitic_node par_value
	{ delete $4; }
;

induc_id:
	pos_integer
;

/****************************************************************/

r_net:
	R_NET net total_cap
	{ sta::spef_reader->rspfBegin($2, $3); }
	routing_conf driver_reducs END
	{ sta::spef_reader->rspfFinish(); }
;

driver_reducs:
	/* empty */
|	driver_reducs driver_reduc
;

driver_reduc:
	driver_pair driver_cell pi_model
	{ sta::spef_reader->rspfDrvrBegin($1, $3);
	  sta::stringDelete($2);
	}
	load_desc
	{ sta::spef_reader->rspfDrvrFinish(); }
;

driver_pair:
	DRIVER pin_name
	{ $$ = $2; }
;

driver_cell:
	CELL cell_type
	{ $$ = $2; }
;

pi_model:
	C2_R1_C1 par_value par_value par_value
	{ $$ = new sta::SpefRspfPi($2, $3, $4); }
;

/****************************************************************/

load_desc:
	LOADS rc_descs
;

rc_descs:
	rc_desc
|	rc_descs rc_desc
;

rc_desc:
	RC pin_name par_value
	{ sta::spef_reader->rspfLoad($2, $3); }
|	RC pin_name par_value pole_residue_desc
	{ sta::spef_reader->rspfLoad($2, $3); }
;

pole_residue_desc:
	pole_desc residue_desc
;

pole_desc:
	KW_Q pos_integer poles
;

poles:
	pole
|	poles pole
;

pole:
	complex_par_value
;

complex_par_value:
	cnumber
|	number
|	cnumber ':' cnumber ':' cnumber
|	number ':' number ':' number
;

cnumber:
	'(' real_component imaginary_component ')'
	{ $$ = $2; }
;

real_component:
	number
;

imaginary_component:
	number
;

residue_desc:
	KW_K pos_integer residues
;

residues:
	residue
|	residues residue
;

residue:
	complex_par_value
;

/****************************************************************/

d_pnet:
	D_PNET pnet_ref total_cap routing_conf pconn_sec cap_sec res_sec
               induc_sec END
	{ sta::stringDelete($2); }
;

pnet_ref:
	name_or_index
;

pconn_sec:
	CONN pconn_defs internal_pnode_coords
;

pconn_defs:
	pconn_def
|	pconn_defs pconn_def
;

pconn_def:
	KW_P pexternal_connection direction conn_attr
|	KW_I internal_connection direction conn_attr
;

pexternal_connection:
	pport_name
;

internal_pnode_coords:
	/* empty */
|	internal_pnode_coords internal_pnode_coord
;

internal_pnode_coord:
	KW_N internal_pdspf_node coordinates
;

internal_pdspf_node:
	name_or_index
	{
	  sta::stringDelete($1);
	  $$ = 0;
	}
;

name_or_index:
	IDENT
|	NAME
|	INDEX
;

/****************************************************************/

r_pnet:
	R_PNET pnet_ref total_cap routing_conf END
	{ sta::stringDelete($2); }
|	R_PNET pnet_ref total_cap routing_conf pdriver_reduc END
	{ sta::stringDelete($2); }
;

pdriver_reduc:
	pdriver_pair driver_cell pi_model load_desc
;

pdriver_pair:
	DRIVER internal_connection
;

/****************************************************************/

number:
	INTEGER
	{ $$ = static_cast<float>($1); }
|	FLOAT
;

pos_integer:
	INTEGER
	{ int value = $1;
	  if (value < 0)
	    sta::spef_reader->warn("%d is not positive.\n", value);
	  $$ = value;
	}
;

pos_number:
	INTEGER
	{ float value = static_cast<float>($1);
	  if (value < 0)
	    sta::spef_reader->warn("%.4f is not positive.\n", value);
	  $$ = value;
	}
|	FLOAT
	{ float value = static_cast<float>($1);
	  if (value < 0)
	    sta::spef_reader->warn("%.4f is not positive.\n", value);
	  $$ = value;
	}
;

%%
