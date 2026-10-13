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

define_cmd_args "get_property" \
  {[-object_type library|liberty_library|cell|liberty_cell|instance|pin|net|port|clock|timing_arc] object property} \
  -help {The `get_property` command returns a property of an object. Properties for each object type are shown below.

| Object type | Properties |
| --- | --- |
| cell (SDC lib_cell) | `base_name`, `filename`, `full_name`, `library`, `name` |
| clock | `full_name`, `is_generated`, `is_propagated`, `is_virtual`, `name`, `period`, `sources` |
| edge | `delay_max_fall`, `delay_min_fall`, `delay_max_rise`, `delay_min_rise`, `full_name`, `from_pin`, `sense`, `to_pin` |
| instance (SDC cell) | `cell`, `full_name`, `is_buffer`, `is_clock_gate`, `is_hierarchical`, `is_inverter`, `is_macro`, `is_memory`, `liberty_cell`, `name`, `ref_name` |
| liberty_cell (SDC lib_cell) | `area`, `base_name`, `dont_use`, `filename`, `full_name`, `is_buffer`, `is_inverter`, `is_memory`, `library`, `name` |
| liberty_port (SDC lib_pin) | `capacitance`, `direction`, `drive_resistance`, `drive_resistance_max_fall`, `drive_resistance_max_rise`, `drive_resistance_min_fall`, `drive_resistance_min_rise`, `full_name`, `intrinsic_delay`, `intrinsic_delay_max_fall`, `intrinsic_delay_max_rise`, `intrinsic_delay_min_fall`, `intrinsic_delay_min_rise`, `is_register_clock`, `lib_cell`, `name` |
| library | `filename` (Liberty library only), `name`, `full_name` |
| mode | `name`, `full_name` |
| net | `full_name`, `name` |
| path (PathEnd) | `endpoint`, `endpoint_clock`, `endpoint_clock_pin`, `slack`, `startpoint`, `startpoint_clock`, `points` |
| pin | `activity`, `slew_max_fall`, `slew_max_rise`, `slew_min_fall`, `slew_min_rise`, `clocks`, `clock_domains`, `direction`, `full_name`, `is_hierarchical`, `is_port`, `is_register_clock`, `lib_pin_name`, `name`, `slack_max`, `slack_max_fall`, `slack_max_rise`, `slack_min`, `slack_min_fall`, `slack_min_rise` |
| point (PathRef) | `arrival`, `pin`, `required`, `slack` |
| port | `activity`, `slew_max_fall`, `slew_max_rise`, `slew_min_fall`, `slew_min_rise`, `direction`, `full_name`, `liberty_port`, `name`, `slack_max`, `slack_max_fall`, `slack_max_rise`, `slack_min`, `slack_min_fall`, `slack_min_rise` |
| scene | `name`, `full_name` |

The pin `activity` property is a list of activity (transitions per second), duty cycle, and origin. Origin is one of `global` (`set_power_activity -global`), `input` (`set_power_activity -input`), `user` (`set_power_activity -input_ports`/`-pins`), `vcd` (`read_vcd`), `saif` (`read_saif`), `propagated`, `clock` (`create_clock`/`create_generated_clock`), or `constant` (Verilog tie high/low, `set_case_analysis`, `set_logic_one`/`zero`/`dc`).} \
  -arg_help {
    -object_type {`object_type`: The type of object when it is specified as a name.
cell|pin|net|port|clock|library|library_cell|library_pin|timing_arc}
    object {An object returned by a `get_*` command, or an object name. `-object_type` is required if object is a name.}
    property {A property name.}
  }

proc get_property { args } {
  parse_key_args "get_property" args keys {-object_type} flags {-quiet}
  check_argc_eq2 "get_property" $args
  set quiet [info exists flags(-quiet)]
  set object [lindex $args 0]
  if { $object == "" } {
    sta_error 2200 "get_property object is null."
  } elseif { ![is_object $object] } {
    if [info exists keys(-object_type)] {
      set object_type $keys(-object_type)
    } else {
      sta_error 2201 "get_property -object_type must be specified with object name argument."
    }
    set object [get_property_object_type $object_type $object $quiet]
  }
  set prop [lindex $args 1]
  return [get_object_property $object $prop]
}

proc get_object_property { object prop } {
  if { [is_object $object] } {
    set object_type [object_type $object]
    if { $object_type == "Instance" } {
      return [instance_property $object $prop]
    } elseif { $object_type == "Pin" } {
      return [pin_property $object $prop]
    } elseif { $object_type == "Net" } {
      return [net_property $object $prop]
    } elseif { $object_type == "Clock" } {
      return [clock_property $object $prop]
    } elseif { $object_type == "Scene" } {
      return [scene_property $object $prop]
    } elseif { $object_type == "Mode" } {
      return [mode_property $object $prop]
    } elseif { $object_type == "Port" } {
      return [port_property $object $prop]
    } elseif { $object_type == "LibertyPort" } {
      return [liberty_port_property $object $prop]
    } elseif { $object_type == "LibertyCell" } {
      return [liberty_cell_property $object $prop]
    } elseif { $object_type == "Cell" } {
      return [cell_property $object $prop]
    } elseif { $object_type == "Library" } {
      return [library_property $object $prop]
    } elseif { $object_type == "LibertyLibrary" } {
      return [liberty_library_property $object $prop]
    } elseif { $object_type == "Edge" } {
      return [edge_property $object $prop]
    } elseif { $object_type == "PathEnd" } {
      return [path_end_property $object $prop]
    } elseif { $object_type == "Path" } {
      return [path_property $object $prop]
    } elseif { $object_type == "TimingArcSet" } {
      return [timing_arc_property $object $prop]
    } else {
      sta_error 2203 "get_property unsupported object type $object_type."
    }
  } else {
    sta_error 2204 "get_property $object is not an object."
  }
}

proc get_property_object_type { object_type object_name quiet } {
  set object "NULL"
  if { $object_type == "instance" \
       || $object_type == "cell"} {
    set object [get_cells -quiet $object_name]
  } elseif { $object_type == "pin" } {
    set object [get_pins -quiet $object_name]
  } elseif { $object_type == "net" } {
    set object [get_nets -quiet $object_name]
  } elseif { $object_type == "port" } {
    set object [get_ports -quiet $object_name]
  } elseif { $object_type == "clock" } {
    set object [get_clocks -quiet $object_name]
  } elseif { $object_type == "liberty_cell" \
               || $object_type == "lib_cell"} {
    set object [get_lib_cells -quiet $object_name]
  } elseif { $object_type == "liberty_port" \
               || $object_type == "lib_pin" } {
    set object [get_lib_pins -quiet $object_name]
  } elseif { $object_type == "library" \
             || $object_type == "lib"} {
    set object [get_libs -quiet $object_name]
  } else {
    sta_error 2205 "$object_type not supported."
  }
  if { $object == "NULL" && !$quiet } {
    sta_error 2206 "$object_type '$object_name' not found."
  }
  return [lindex $object 0]
}

define_cmd_args "define_property" \
  {-object_type scene|mode|library|liberty_library|cell|liberty_cell|port|liberty_port|instance|pin|net|clock -type bool|float|string property} \
  -help {The `define_property` command defines a user property that can be set with `set_property` and read with `get_property`. User properties can also be used in `-filter` expressions.} \
  -arg_help {
    -object_type {Object type the property applies to.}
    -type {`bool`: Boolean value. `float`: Floating point value. `string`: String value.}
    property {The property name.}
  }

proc define_property { args } {
  parse_key_args "define_property" args keys {-object_type -type} flags {}
  check_argc_eq1 "define_property" $args
  if { ![info exists keys(-object_type)] } {
    sta_error 2207 "define_property -object_type must be specified."
  }
  if { ![info exists keys(-type)] } {
    sta_error 2208 "define_property -type must be specified."
  }
  define_property_cmd $keys(-object_type) [lindex $args 0] $keys(-type)
}

define_cmd_args "set_property" {object property value} \
  -help {The `set_property` command sets a user property defined with `define_property` on an object. Use `get_property` to read the value.} \
  -arg_help {
    object {An object returned by a `get_*` command.}
    property {A property name defined with `define_property`.}
    value {The property value.}
  }

proc set_property { args } {
  check_argc_eq3 "set_property" $args
  set object [lindex $args 0]
  set prop [lindex $args 1]
  set value [lindex $args 2]
  if { ![is_object $object] } {
    sta_error 2213 "set_property $object is not an object."
  }
  set_property_cmd $object [object_type $object] $prop $value
}

# sta namespace end.
}
