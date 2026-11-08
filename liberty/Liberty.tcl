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

# Liberty commands.

namespace eval sta {

define_cmd_args "read_liberty" \
  {[-corner corner] [-min] [-max] [-infer_latches] filename} \
  -help {The `read_liberty` command reads a Liberty format library file. The first library that is read sets the units used by SDC/Tcl commands and reporting. The include_file attribute is supported.

Liberty `generated_clock` groups are installed as generated clocks when the design is linked with `link_design`.

Some Liberty libraries do not include latch groups for cells that describe transparent latches. In that situation the `-infer_latches` command flag can be used to infer the latches. The timing arcs required for a latch to be inferred should look like the following:

```
cell (inferred_latch) {
  pin(D) {
    direction : input ;
    timing () {
      related_pin : "E" ;
      timing_type : setup_falling ;
    }
    timing () {
      related_pin : "E" ;
      timing_type : hold_falling ;
    }
  }
  pin(E) {
    direction : input;
  }
  pin(Q) {
    direction : output ;
    timing () {
      related_pin : "D" ;
    }
    timing () {
      related_pin : "E" ;
      timing_type : rising_edge ;
    }
  }
}
```

In this example a positive level-sensitive latch is inferred.

Files compressed with gzip are automatically uncompressed.} \
  -arg_help {
    -corner {Deprecated. Use `define_scene` to assign Liberty libraries to a scene.}
    -min {Use the library for min-delay (hold) analysis.}
    -max {Use the library for max-delay (setup) analysis.}
    filename {The Liberty file name to read.}
    -infer_latches {Infer latches from timing arcs when the Liberty file has no latch groups.}
  }

proc_redirect read_liberty {
  parse_key_args "read_liberty" args keys {-corner} \
    flags {-min -max -infer_latches}
  check_argc_eq1 "read_liberty" $args

  set filename [file nativename [lindex $args 0]]
  set corner [parse_scene keys]
  set min_max [parse_min_max_all_flags flags]
  set infer_latches [info exists flags(-infer_latches)]
  read_liberty_cmd $filename $corner $min_max $infer_latches
}

# for regression testing
proc write_liberty { args } {
  check_argc_eq2 "write_liberty" $args

  set library [get_liberty_error "library" [lindex $args 0]]
  set filename [file nativename [lindex $args 1]]
  write_liberty_cmd $library $filename
}

################################################################

define_cmd_args "report_lib_cell" {cell_name [> filename] [>> filename]} \
  -help {Describe the liberty library cell cell_name.} \
  -arg_help {
    cell_name {The name of a library cell.}
  }

proc_redirect report_lib_cell {
  check_argc_eq1 "report_lib_cell" $args
  set arg [lindex $args 0]
  set cell [get_lib_cell_warn "lib_cell" $arg]
  set scene [cmd_scene]
  if { $cell != "NULL" } {
    report_lib_cell_ $cell $scene
  }
}

proc report_lib_cell_ { cell scene } {
  global sta_report_default_digits

  set lib [$cell liberty_library]
  report_line "Cell [get_name $cell]"
  report_line "Library [get_name $lib]"
  set filename [liberty_cell_property $cell "filename"]
  if { $filename != "" } {
    report_line "File $filename"
  }
  report_lib_ports $cell $scene
  report_timing_arcs $cell
}

proc report_lib_ports { cell scene } {
  set iter [$cell liberty_port_iterator]
  while {[$iter has_next]} {
    set port [$iter next]
    if { [$port is_bus] || [$port is_bundle] } {
      report_lib_port $port $scene
      set member_iter [$port member_iterator]
      while { [$member_iter has_next] } {
        set port [$member_iter next]
        report_lib_port $port $scene
      }
      $member_iter finish
    } elseif { ![$port is_bundle_member] && ![$port is_bus_bit] } {
      report_lib_port $port $scene
    }
  }
  $iter finish
}

proc report_lib_port { port scene } {
  global sta_report_default_digits

  if { [$port is_bus] } {
    set port_name [$port bus_name]
  } else {
    set port_name [get_name $port]
  }
  set indent ""
  if { [$port is_bundle_member] || [$port is_bus_bit] } {
    set indent "  "
  }
  set enable [$port tristate_enable]
  if { $enable != "" } {
    set enable " enable=$enable"
  }
  set func [$port function]
  if { $func != "" } {
    set func " function=$func"
  }
  report_line " ${indent}$port_name [liberty_port_direction $port]$enable$func[port_capacitance_str $port $scene $sta_report_default_digits]"
}

proc report_timing_arcs { cell } {
  set timing_arcs [$cell timing_arc_sets]
  if { [llength $timing_arcs] > 0 } {
    puts ""
    puts "Timing arcs"
    foreach timing_arc $timing_arcs {
      puts " [$timing_arc to_string]"
      puts "  [$timing_arc role]"
      set when [$timing_arc when]
      if { $when != "" } {
        puts "  when $when"
      }
      foreach arc [$timing_arc timing_arcs] {
        puts "  [$arc from_edge] -> [$arc to_edge]"
      }
    }
  }
}

# sta namespace end
}
