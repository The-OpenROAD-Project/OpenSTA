# OpenSTA, Static Timing Analyzer
# Copyright (c) 2022, Parallax Software, Inc.
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

# Network reporting commands.

namespace eval sta {

proc set_cmd_namespace { namespc } {
  if { $namespc == "sdc" || $namespc == "sta" } {
    set_cmd_namespace_cmd $namespc
  } else {
    sta_error 589 "unknown namespace $namespc."
  }
}

################################################################

define_cmd_args "report_cell" \
  {[-connections] [-verbose] instance_path [> filename] [>> filename]}

# Alias for report_instance.
proc_redirect report_cell {
  eval report_instance $args
}

################################################################

define_cmd_args "report_instance" \
  {[-connections] [-verbose] instance_path [> filename] [>> filename]}

proc_redirect report_instance {
  parse_key_args "report_instance" args keys {} flags {-connections -verbose}
  check_argc_eq1 "report_instance" $args

  set connections [info exists flags(-connections)]
  set verbose [info exists flags(-verbose)]
  set instance_path [lindex $args 0]
  set instance [find_instance $instance_path]
  if { $instance != "NULL" } {
    report_instance1 $instance $connections $verbose
  } else {
    sta_error 590 "instance $instance_path not found."
  }
}

proc report_instance1 { instance connections verbose } {
  if { $instance == [top_instance] } {
    set inst_name "top"
  } else {
    set inst_name [get_full_name $instance]
  }
  report_line "Instance $inst_name"
  set cell [instance_property $instance "cell"]
  report_line " Cell: [get_name $cell]"
  report_line " Library: [get_name [$cell library]]"
  report_line " Path cells: [instance_cell_path $instance]"
  if { $connections } {
    report_instance_pins $instance $verbose
  }
  if { $verbose } {
    report_instance_children_ $instance
  }
}

proc report_instance_pins { instance verbose } {
  report_instance_pins1 $instance $verbose \
    " Input pins:" 0 {"input" "bidirect"}
  report_instance_pins1 $instance $verbose \
    " Output pins:" 0 {"output" "bidirect"}
  report_instance_pins1 $instance $verbose \
    " Other pins:" 1 {"internal" "power" "ground" "unknown"}
}

proc report_instance_pins1 {instance verbose header header_optional dirs} {
  set header_shown 0
  if { !$header_optional } {
    report_line $header
    set header_shown 1
  }
  set iter [$instance pin_iterator]
  while {[$iter has_next]} {
    set pin [$iter next]
    set dir [pin_direction $pin]
    if { [lsearch $dirs $dir] !=  -1 } {
      if { !$header_shown } {
	report_line $header
	set header_shown 1
      }
      report_instance_pin $pin $verbose
    }
  }
  $iter finish
}

proc report_instance_pin { pin verbose } {
  set net [$pin net]
  if { $net == "NULL" } {
    set net_name "(unconnected)"
  } else {
    set net_name [get_full_name [$net highest_connected_net]]
  }
  report_line "  [$pin port_name] [pin_direction $pin] $net_name"
  if { $verbose && $net != "NULL" } {
    set pins [net_connected_pins_sorted $net]
    foreach pin $pins {
      if [$pin is_load] {
        if [$pin is_top_level_port] {
          report_line "   [get_full_name $pin] [pin_direction $pin] port"
        } else {
          report_line "   [get_full_name $pin] [pin_direction $pin]"
        }
      }
    }
  }
}

# Concatenate the cell names of the instance parents.
proc instance_cell_path { instance } {
  set cell_path "[get_name [instance_property $instance "cell"]]"
  set parent [$instance parent]
  set top_instance [top_instance]
  while { $parent != "NULL" && $parent != $top_instance } {
    set cell_path "[get_name [$parent cell]]/$cell_path"
    set parent [$parent parent]
  }
  return $cell_path
}

proc report_instance_children_ { instance } {
  set children [instance_sorted_children $instance]
  if { $children != {} } {
    report_line " Children:"
    foreach child $children {
      report_line "  [get_name $child] ([instance_property $child ref_name])"
    }
  }
}

proc instance_sorted_children { instance } {
  set children {}
  set iter [$instance child_iterator]
  while {[$iter has_next]} {
    lappend children [$iter next]
  }
  $iter finish
  return [sort_by_full_name $children]
}

################################################################

define_cmd_args "report_lib_cell" {cell_name [> filename] [>> filename]}

proc_redirect report_lib_cell {
  check_argc_eq1 "report_lib_cell" $args
  set arg [lindex $args 0]
  set cell [get_lib_cell_warn "lib_cell" $arg]
  set corner [cmd_corner]
  if { $cell != "NULL" } {
    report_lib_cell_ $cell $corner
  }
}

proc report_lib_cell_ { cell corner } {
  global sta_report_default_digits

  set lib [$cell liberty_library]
  report_line "Cell [get_name $cell]"
  report_line "Library [get_name $lib]"
  set filename [liberty_cell_property $cell "filename"]
  if { $filename != "" } {
    report_line "File $filename"
  }
  set iter [$cell liberty_port_iterator]
  while {[$iter has_next]} {
    set port [$iter next]
    if { [$port is_bus] } {
      set port_name [$port bus_name]
    } else {
      set port_name [get_name $port]
    }
    set enable [$port tristate_enable]
    if { $enable != "" } {
      set enable " enable=$enable"
    }
    set func [$port function]
    if { $func != "" } {
      set func " function=$func"
    }
    report_line " $port_name [liberty_port_direction $port]$enable$func[port_capacitance_str $port $corner $sta_report_default_digits]"
  }
  $iter finish
}

proc report_cell_ { cell } {
  set lib [$cell library]
  report_line "Cell [get_name $cell]"
  report_line "Library [get_name $lib]"
  set filename [liberty_cell_property $cell "filename"]
  if { $filename != "" } {
    report_line "File $filename"
  }
  set iter [$cell port_iterator]
  while {[$iter has_next]} {
    set port [$iter next]
    if { [$port is_bus] } {
      report_line " [$port bus_name] [port_direction $port]"
    } else {
      report_line " [get_name $port] [port_direction $port]"
    }
  }
  $iter finish
}

define_cmd_args "report_pin" {[-corner corner] [-digits digits] pin\
                                [> filename] [>> filename]}

proc_redirect report_pin {
  global sta_report_default_digits

  parse_key_args "report_pin" args keys {-corner -digits} \
    flags {-connections -verbose -hier_pins}
  set corner [parse_corner_or_all keys]
  set digits $sta_report_default_digits
  if { [info exists keys(-digits)] } {
      set digits $keys(-digits)
  }
  check_argc_eq1 "report_pin" $args
  set pin_path [lindex $args 0]
  set pin [get_pin_warn "pin" $pin_path]

  if { $pin != "NULL" } {
    report_pin_ $pin $corner $digits
  }
}

################################################################

define_cmd_args "report_net" \
  {[-connections] [-verbose] [-corner corner] [-digits digits] [-hier_pins]\
     net_path [> filename] [>> filename]}

# -hpins to show hierarchical pins
proc_redirect report_net {
  global sta_report_default_digits

  parse_key_args "report_net" args keys {-corner -digits} \
    flags {-connections -verbose -hier_pins}
  check_argc_eq1 "report_net" $args

  set corner [parse_corner_or_all keys]
  set digits $sta_report_default_digits
  if { [info exists keys(-digits)] } {
      set digits $keys(-digits)
  }

  set connections [info exists flags(-connections)]
  set verbose [info exists flags(-verbose)]
  set hier_pins [info exists flags(-hier_pins)]
  set net_path [lindex $args 0]
  set net [find_net $net_path]
  if { $net != "NULL" } {
    report_net1 $net $connections $verbose $hier_pins $corner $digits
  } else {
    set pin [find_pin $net_path]
    if { $pin != "NULL" } {
      set net [$pin net]
      if { $net != "NULL" } {
	report_net1 $net $connections $verbose $hier_pins $corner $digits
      } else {
	sta_error 591 "net $net_path not found."
      }
    } else {
      sta_error 592 "net $net_path not found."
    }
  }
}

proc report_net1 { net connections verbose hier_pins corner digits } {
  report_line "Net [get_full_name $net]"
  if {$connections} {
    set pins [net_connected_pins_sorted $net]
    if {$verbose} {
      report_net_caps $net $pins $corner $digits
    }
    report_net_pins $pins "Driver pins" "is_driver" $verbose $corner $digits
    report_net_pins $pins "Load pins" "is_load" $verbose $corner $digits
    if {$hier_pins} {
      report_net_pins $pins "Hierarchical pins" "is_hierarchical" \
        $verbose $corner $digits
    }
    report_net_other_pins $pins $verbose $corner $digits
  }
}

proc net_connected_pins_sorted { net } {
  set pins {}
  set iter [$net connected_pin_iterator]
  while {[$iter has_next]} {
    set pin [$iter next]
    lappend pins $pin
  }
  $iter finish
  set pins [sort_by_full_name $pins]
  return $pins
}

proc report_net_caps { net pins corner digits } {
  report_net_cap $net "Pin" "pin_capacitance" $corner $digits
  report_net_cap $net "Wire" "wire_capacitance" $corner $digits
  report_net_cap $net "Total" "capacitance" $corner $digits

  set pin_count 0
  set driver_count 0
  set load_count 0
  foreach pin $pins {
    if {![$pin is_hierarchical]} {
      incr pin_count
    }
    if [$pin is_driver] {
      incr driver_count
    }
    if [$pin is_load] {
      incr load_count
    }
  }
  report_line " Number of drivers: $driver_count"
  report_line " Number of loads: $load_count"
  report_line " Number of pins: $pin_count"
  report_line ""
}

proc report_net_cap { net caption cap_msg corner digits } {
  set cap_min [$net $cap_msg $corner "min"]
  set cap_max [$net $cap_msg $corner "max"]
  report_line " $caption capacitance: [capacitance_range_str $cap_min $cap_max $digits]"
}

proc report_net_pins { pins header pin_pred verbose corner digits } {
  set found 0
  foreach pin $pins {
    if {[$pin $pin_pred]} {
      if { !$found } {
        report_line $header
        set found 1
      }
      report_net_pin $pin $verbose $corner $digits
    }
  }
  if { $found } {
    report_line ""
  }
}

proc report_net_other_pins { pins verbose corner digits } {
  set header 0
  foreach pin $pins {
    if { !([$pin is_driver] || [$pin is_load] || [$pin is_hierarchical]) } {
      if { !$header } {
	report_line ""
	report_line "Other pins"
	set header 1
      }
      report_net_pin $pin $verbose $corner $digits
    }
  }
}

proc report_net_pin { pin verbose corner digits } {
  if [$pin is_leaf] {
    set cell_name [get_name [[$pin instance] cell]]
    set cap ""
    if { $verbose } {
      set liberty_port [$pin liberty_port]
      if { $liberty_port != "NULL" } {
	set cap [port_capacitance_str $liberty_port $corner $digits]
      }
    }
    report_line " [get_full_name $pin] [pin_direction $pin] ($cell_name)$cap[pin_location_str $pin]"
  } elseif [$pin is_top_level_port] {
    set wire_cap ""
    set pin_cap ""
    if { $verbose } {
      set port [$pin port]
      set cap_min [port_ext_wire_cap $port "min"]
      set cap_max [port_ext_wire_cap $port "max"]
      if { $cap_min > 0 || $cap_max > 0 } {
	set wire_cap " wire [capacitance_range_str $cap_min $cap_max $digits]"
      }

      set cap_min [port_ext_pin_cap $port "min"]
      set cap_max [port_ext_pin_cap $port "max"]
      if { $cap_min > 0 || $cap_max > 0} {
	set pin_cap " pin [capacitance_range_str $cap_min $cap_max $digits]"
      }
    }
    report_line " [get_full_name $pin] [pin_direction $pin] port$wire_cap$pin_cap[pin_location_str $pin]"
  } elseif [$pin is_hierarchical] {
    report_line " [get_full_name $pin] [pin_direction $pin]"
  }
}

# Used by report_net
proc pin_location_str { pin } {
  set loc [pin_location $pin]
  if { $loc != "" } {
    lassign $loc x y
    return " ([format_distance $x 0], [format_distance $y 0])"
  } else {
    return ""
  }
}

################################################################

proc report_pin_ { pin corner digits } {
  set liberty_port [$pin liberty_port]
  if { $liberty_port != "NULL" } {
    set cap [port_capacitance_str $liberty_port $corner $digits]
  } else {
    set cap ""
  }

  if { [$pin is_top_level_port] } {
    set term [$pin term]
    if { $term == "NULL" } {
      set net "NULL"
    } else {
      set net [$term net]
    }
  } else {
    set net [$pin net]
  }
  if { $net == "NULL" } {
    set net_name "(unconnected)"
  } else {
    set net_name [get_full_name [$net highest_connected_net]]
  }
  report_line "Pin [get_full_name $pin] [pin_direction_desc $pin]$cap $net_name"
}

proc pin_direction_desc { pin } {
  if [$pin is_hierarchical] {
    return "hierarchical [pin_direction $pin]"
  } elseif [$pin is_top_level_port] {
    return "[pin_direction $pin] port"
  } else {
    return [pin_direction $pin]
  }
}

# Do not preceed this field by a space in the caller.
proc port_capacitance_str { liberty_port corner digits } {
  set cap_min [$liberty_port capacitance $corner "min"]
  set cap_max [$liberty_port capacitance $corner "max"]
  if { $cap_min > 0 || $cap_max > 0 } {
    return " [capacitance_range_str $cap_min $cap_max $digits]"
  } else {
    return ""
  }
}

proc capacitance_range_str { cap_min cap_max digits } {
  if { $cap_min == $cap_max } {
    return "[format_capacitance $cap_max $digits]"
  } else {
    return "[format_capacitance $cap_min $digits]-[format_capacitance $cap_max $digits]"
  }
}

proc capacitances_str { cap_r_min cap_r_max cap_f_min cap_f_max digits } {
  if { $cap_r_min == $cap_r_max \
	 && $cap_f_min == $cap_f_max \
	 && $cap_r_max == $cap_f_max } {
    # All 4 values are the same.
    set cap $cap_r_min
    return "[format_capacitance $cap $digits]"
  } elseif { $cap_r_min == $cap_r_max \
	       && $cap_f_min == $cap_f_max } {
    # Mins equal maxes.
    return "r [format_capacitance $cap_r_min $digits] f [format_capacitance $cap_f_min $digits]"
  } else {
    return "r [format_capacitance $cap_r_min $digits]:[format_capacitance $cap_r_max $digits] f [format_capacitance $cap_f_min $digits]:[format_capacitance $cap_f_max $digits]"
  }
}

################################################################
#
# Debugging functions
#
################################################################

proc_redirect report_network {
  report_hierarchy [top_instance]
}

proc report_hierarchy { inst } {
  report_instance1 $inst 1 1
  foreach child [instance_sorted_children $inst] {
    report_hierarchy $child
  }
}

proc port_direction_any_input { dir } {
  return [expr { $dir == "input" || $dir == "bidirect" } ]
}

proc port_direction_any_output { dir } {
  return [expr { $dir == "output" \
		   || $dir == "bidirect" \
		   || $dir == "tristate" } ]
}

# collect instance pins into list
proc instance_pins { instance } {
  set pins {}
  set iter [$instance pin_iterator]
  while {[$iter has_next]} {
    lappend pins [$iter next]
  }
  $iter finish
  return $pins
}

# collect ports into a list
proc cell_ports { cell } {
  set ports {}
  set iter [$cell port_iterator]
  while {[$iter has_next]} {
    lappend ports [$iter next]
  }
  $iter finish
  return $ports
}

# sta namespace end
}
