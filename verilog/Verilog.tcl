# OpenSTA, Static Timing Analyzer
# Copyright (c) 2024, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

namespace eval sta {

# Defined by SWIG interface Verilog.i.
define_cmd_args "read_verilog" {filename}

proc_redirect read_verilog {
  read_verilog_cmd [file nativename [lindex $args 0]]
}

define_cmd_args "write_verilog" {[-sort] [-include_pwr_gnd]\
				   [-remove_cells cells] filename}

proc write_verilog { args } {
  parse_key_args "write_verilog" args keys {-remove_cells} \
    flags {-sort -include_pwr_gnd}

  set remove_cells {}
  if { [info exists keys(-remove_cells)] } {
    set remove_cells [parse_cell_arg $keys(-remove_cells)]
  }
  set sort [info exists flags(-sort)]
  set include_pwr_gnd [info exists flags(-include_pwr_gnd)]
  check_argc_eq1 "write_verilog" $args
  set filename [file nativename [lindex $args 0]]
  write_verilog_cmd $filename $sort $include_pwr_gnd $remove_cells
}

# sta namespace end
}
