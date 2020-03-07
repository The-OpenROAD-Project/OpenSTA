# OpenSTA, Static Timing Analyzer
# Copyright (c) 2020, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

namespace eval sta {

# Defined by SWIG interface Verilog.i.
define_cmd_args "read_verilog" {filename}

define_cmd_args "write_verilog" {[-sort] filename}

proc write_verilog { args } {
  parse_key_args "write_verilog" args keys {} flags {-sort}

  set sort [info exists flags(-sort)]
  check_argc_eq1 "write_verilog" $args
  set filename $args
  write_verilog_cmd $filename $sort
}

# sta namespace end
}
