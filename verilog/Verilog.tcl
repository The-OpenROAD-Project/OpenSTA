# OpenSTA, Static Timing Analyzer
# Copyright (c) 2026, Parallax Software, Inc.
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
# 
# The origin of this software must not be misrepresented; you must not
# claim that you wrote the original software.
# 
# Altered source versions must be plainly marked as such, and must not be
# misrepresented as being the original software.
# 
# This notice may not be removed or altered from any source distribution.

namespace eval sta {

# Defined by SWIG interface Verilog.i.
define_cmd_args "read_verilog" {filename} \
  -help {The `read_verilog` command reads a gate level verilog netlist. After all verilog netlist and Liberty libraries are read the design must be linked with the `link_design` command.

Verilog 2001 module port declaratations are supported. An example is shown below.

```
module top (input in1, in2, clk1, clk2, clk3,
            output out);
```

Files compressed with gzip are automatically uncompressed.} \
  -arg_help {
    filename {The name of the verilog file to read.}
  }

proc_redirect read_verilog {
  read_verilog_cmd [file nativename [lindex $args 0]]
}

define_cmd_args "write_verilog" {[-include_pwr_gnd]\
                                   [-remove_cells cells] filename} \
  -help {The `write_verilog` command writes a Verilog netlist to filename. Use `-sort` to sort the instances so the results are reproducible across operating systems. Use `-remove_cells` to remove instances of lib_cells from the netlist.} \
  -arg_help {
    -include_pwr_gnd {Include power and ground pins on instances.}
    -remove_cells {`lib_cells`: Liberty cells to remove from the Verilog netlist. Use `get_lib_cells`, a list of cells names, or a cell name with wildcards.}
    filename {Filename for the liberty library.}
  }

proc write_verilog { args } {
  # -sort deprecated 12/12/2025
  parse_key_args "write_verilog" args keys {-remove_cells} \
    flags {-sort -include_pwr_gnd}

  if { [info exists flags(-sort)] } {
    sta_warn 1338 "The -sort flag is ignored."
  }
  set remove_cells {}
  if { [info exists keys(-remove_cells)] } {
    set remove_cells [parse_cell_arg $keys(-remove_cells)]
  }
  set include_pwr_gnd [info exists flags(-include_pwr_gnd)]
  check_argc_eq1 "write_verilog" $args
  set filename [file nativename [lindex $args 0]]
  write_verilog_cmd $filename $include_pwr_gnd $remove_cells
}

# sta namespace end
}
