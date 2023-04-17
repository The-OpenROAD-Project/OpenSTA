# OpenSTA, Static Timing Analyzer
# Copyright (c) 2023, Parallax Software, Inc.
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

################################################################
#  
# Utility commands
#
################################################################

define_cmd_args "help" {[pattern]}

proc_redirect help {
  variable cmd_args

  set arg_count [llength $args]
  if { $arg_count == 0 } {
    set pattern "*"
  } elseif { $arg_count == 1 } {
    set pattern [lindex $args 0]
  } else {
    cmd_usage_error "help"
  }
  set matches [array names cmd_args $pattern]
  if { $matches != {} } {
    foreach cmd [lsort $matches] {
      show_cmd_args $cmd
    }
  } else {
    sta_warn 300 "no commands match '$pattern'."
  }
}

proc show_cmd_args { cmd } {
  variable cmd_args

  set max_col 80
  set indent 2
  set indent_str "  "
  set line $cmd
  set col [string length $cmd]
  set arglist $cmd_args($cmd)
  # Break the arglist up into max_col length lines.
  while {1} {
    if {[regexp {(^[\n ]*)([a-zA-Z0-9_\\\|\-]+|\[[^\[]+\])(.*)} \
	   $arglist ignore space arg rest]} {
      set arg_length [string length $arg]
      if { $col + $arg_length < $max_col } {
	set line "$line $arg"
	set col [expr $col + $arg_length + 1]
      } else {
        report_line $line
	set line "$indent_str $arg"
	set col [expr $indent + $arg_length + 1]
      }
      set arglist $rest
    } else {
      report_line $line
      break
    }
  }
}

# This is used in lieu of command completion to make sdc commands
# like get_ports be abbreviated get_port.
proc define_cmd_alias { alias cmd } {
  eval "proc $alias { args } { eval [concat $cmd \$args] }"
  namespace export $alias
}

proc cmd_usage_error { cmd } {
  variable cmd_args

  if [info exists cmd_args($cmd)] {
    sta_error 404 "Usage: $cmd $cmd_args($cmd)"
  } else {
    sta_error 405 "Usage: $cmd argument error"
  }
}

################################################################

define_cmd_args "with_output_to_variable" { var { cmds }}

# with_output_to_variable variable { command args... }
proc with_output_to_variable { var_name args } {
  upvar 1 $var_name var

  set body [lindex $args 0]
  sta::redirect_string_begin;
  catch $body ret
  set var [sta::redirect_string_end]
  return $ret
}

################################################################

define_cmd_args "report_units" {}

proc report_units { args } {
  check_argc_eq0 "report_units" $args
  foreach unit {"time" "capacitance" "resistance" "voltage" "current" "power" "distance"} {
    report_line " $unit 1[unit_scaled_suffix $unit]"
  }
}

################################################################

define_cmd_args "set_cmd_units" \
  {[-capacitance cap_unit] [-resistance res_unit] [-time time_unit]\
     [-voltage voltage_unit] [-current current_unit] [-power power_unit]\
     [-distance distance_unit]}

proc set_cmd_units { args } {
  parse_key_args "set_cmd_units" args \
    keys {-capacitance -resistance -time -voltage -current -power \
	    -distance -digits -suffix} \
    flags {}

  check_argc_eq0 "set_cmd_units" $args
  set_unit_values "capacitance" -capacitance "f" keys
  set_unit_values "time" -time "s" keys
  set_unit_values "voltage" -voltage "v" keys
  set_unit_values "current" -current "A" keys
  set_unit_values "resistance" -resistance "ohm" keys
  set_unit_values "distance" -distance "m" keys
}

proc set_unit_values { unit key unit_name key_var } {
  upvar 1 $key_var keys
  if { [info exists keys($key)] } {
    set value $keys($key)
    if { [string equal -nocase $value $unit_name] } {
      set_cmd_unit_scale $unit 1.0
    } else {
      set prefix [string index $value 0]
      set suffix [string range $value 1 end]
      # unit includes "1" prefix
      if { [string is digit $prefix] } {
	set prefix [string index $value 1]
	set suffix [string range $value 2 end]
      }
      if { [string equal -nocase $suffix $unit_name] } {
	set scale [unit_prefix_scale $unit $prefix]
	set_cmd_unit_scale $unit $scale
      } else {
	sta_error 515 "unknown $unit unit '$suffix'."
      }
    }
    if [info exists keys(-digits)] {
      set_cmd_unit_digits $unit $keys(-digits)
    }
    if [info exists keys(-suffix)] {
      set_cmd_unit_suffix $unit $keys(-suffix)
    }
  }
}

################################################################

define_cmd_args "delete_from_list" {list objs}

proc delete_from_list { list objects } {
  delete_objects_from_list_cmd $list $objects
}

proc delete_objects_from_list_cmd { list objects } {
  set list0 [lindex $list 0]
  set list_is_object [is_object $list0]
  set list_type [object_type $list0]
  foreach obj $objects {
    # If the list is a collection of tcl objects (returned by get_*),
    # convert the obj to be removed from a name to an object of the same
    # type.
    if {$list_is_object && ![is_object $obj]} {
      if {$list_type == "Clock"} {
	set obj [find_clock $obj]
      } elseif {$list_type == "Port"} {
	set top_instance [top_instance]
	set top_cell [$top_instance cell]
	set obj [$top_cell find_port $obj]
      } elseif {$list_type == "Pin"} {
	set obj [find_pin $obj]
      } elseif {$list_type == "Instance"} {
	set obj [find_instance $obj]
      } elseif {$list_type == "Net"} {
	set obj [find_net $obj]
      } elseif {$list_type == "LibertyLibrary"} {
	set obj [find_liberty $obj]
      } elseif {$list_type == "LibertyCell"} {
	set obj [find_liberty_cell $obj]
      } elseif {$list_type == "LibertyPort"} {
	set obj [get_lib_pins $obj]
      } else {
	sta_error 439 "unsupported object type $list_type."
      }
    }
    set index [lsearch $list $obj]
    if { $index != -1 } {
      set list [lreplace $list $index $index]
    }
  }
  return $list
}

################################################################

proc set_cmd_namespace { namespc } {
  if { $namespc == "sdc" || $namespc == "sta" } {
    set_cmd_namespace_cmd $namespc
  } else {
    sta_error 589 "unknown namespace $namespc."
  }
}

################################################################

define_cmd_args "report_object_full_names" {objects}

proc report_object_full_names { objects } {
  foreach obj [sort_by_full_name $objects] {
    report_line [get_full_name $obj]
  }
}

define_cmd_args "report_object_names" {objects}

proc report_object_names { objects } {
  foreach obj [sort_by_name $objects] {
    report_line [get_name $obj]
  }
}

################################################################

define_cmd_args "get_name" {object}
define_cmd_args "get_full_name" {object}

################################################################

proc get_name { object } {
  return [get_object_property $object "name"]
}

proc get_full_name { object } {
  return [get_object_property $object "full_name"]
}

proc sort_by_name { objects } {
  return [lsort -command name_cmp $objects]
}

proc name_cmp { obj1 obj2 } {
  return [string compare [get_name $obj1] [get_name $obj2]]
}

proc sort_by_full_name { objects } {
  return [lsort -command full_name_cmp $objects]
}

proc full_name_cmp { obj1 obj2 } {
  return [string compare [get_full_name $obj1] [get_full_name $obj2]]
}

proc get_object_type { obj } {
  set object_type [object_type $obj]
  if { $object_type == "Clock" } {
    return "clock"
  } elseif { $object_type == "LibertyCell" } {
    return "lib_cell"
  } elseif { $object_type == "LibertyPort" } {
    return "lib_pin"
  } elseif { $object_type == "Cell" } {
    return "cell"
  } elseif { $object_type == "Instance" } {
    return "instance"
  } elseif { $object_type == "Port" } {
    return "port"
  } elseif { $object_type == "Pin" } {
    return "pin"
  } elseif { $object_type == "Net" } {
    return "net"
  } elseif { $object_type == "Edge" } {
    return "timing_arc"
  } elseif { $object_type == "TimingArcSet" } {
    return "timing_arc"
  } else {
    return "?"
  }
}

# sta namespace end.
}
