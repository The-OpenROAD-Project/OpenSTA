# OpenSTA, Static Timing Analyzer
# Copyright (c) 2019, Parallax Software, Inc.
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

# Network reporting commands.

namespace eval sta {

proc set_cmd_namespace { namespc } {
  if { $namespc == "sdc" || $namespc == "sta" } {
    set_cmd_namespace_cmd $namespc
  } else {
    sta_error "unknown namespace $namespc."
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
    sta_error "instance $instance_path not found."
  }
}

proc report_instance1 { instance connections verbose } {
  if { $instance == [top_instance] } {
    set inst_name "top"
  } else {
    set inst_name [$instance path_name]
  }
  puts "Instance $inst_name"
  puts " Cell: [[$instance cell] name]"
  puts " Library: [[[$instance cell] library] name]"
  puts " Path cells: [instance_cell_path $instance]"
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
    puts $header
    set header_shown 1
  }
  set iter [$instance pin_iterator]
  while {[$iter has_next]} {
    set pin [$iter next]
    set dir [$pin direction]
    if { [lsearch $dirs $dir] !=  -1 } {
      if { !$header_shown } {
	puts $header
	set header_shown 1
      }
      report_instance_pin $pin $verbose
    }
  }
  $iter finish
}

proc report_instance_pin { pin verbose } {
  puts -nonewline "  [$pin port_name] [$pin direction]"
  set net [$pin net]
  if { $net == "NULL" } {
    puts " (unconnected)"
  } else {
    puts " [[$net highest_connected_net] path_name]"
    if { $verbose } {
      set pins [net_connected_pins_sorted $net]
      foreach pin $pins {
	if [$pin is_load] {
	  if [$pin is_top_level_port] {
	    puts "   [$pin path_name] [$pin direction] port"
	  } else {
	    puts "   [$pin path_name] [$pin direction]"
	  }
	}
      }
    }
  }
}

# Concatenate the cell names of the instance parents.
proc instance_cell_path { instance } {
  set cell_path "[[$instance cell] name]"
  set parent [$instance parent]
  set top_instance [top_instance]
  while { $parent != "NULL" && $parent != $top_instance } {
    set cell_path "[[$parent cell] name]/$cell_path"
    set parent [$parent parent]
  }
  return $cell_path
}

proc report_instance_children_ { instance } {
  set children [instance_sorted_children $instance]
  if { $children != {} } {
    puts " Children:"
    foreach child $children {
      puts "  [$child name] ([[$child cell] name])"
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
  return [lsort -command path_name_cmp $children]
}

proc path_name_cmp { obj1 obj2 } {
  return [string compare [$obj1 path_name] [$obj2 path_name]]
}

proc name_cmp { obj1 obj2 } {
  return [string compare [$obj1 name] [$obj2 name]]
}

################################################################

define_cmd_args "report_lib_cell" {cell_name [> filename] [>> filename]}

proc_redirect report_lib_cell {
  check_argc_eq1 "report_lib_cell" $args
  set arg [lindex $args 0]
  set cell [get_lib_cell_warn "lib_cell" $arg]
  if { $cell != "NULL" } {
    report_lib_cell_ $cell
  }
}

proc report_lib_cell_ { cell } {
  global sta_report_default_digits

  set lib [$cell liberty_library]
  puts "Cell [$cell name]"
  puts "Library [$lib name]"
  set filename [$cell filename]
  if { $filename != "" } {
    puts "File $filename"
  }
  set iter [$cell liberty_port_iterator]
  while {[$iter has_next]} {
    set port [$iter next]
    if { [$port is_bus] } {
      puts -nonewline " [$port bus_name] [$port direction]"
    } else {
      puts -nonewline " [$port name] [$port direction]"
    }
    set enable [$port tristate_enable]
    if { $enable != "" } {
      puts -nonewline " enable=$enable"
    }
    set func [$port function]
    if { $func != "" } {
      puts -nonewline " function=$func"
    }
    puts [port_capacitance_str $port $sta_report_default_digits]
  }
  $iter finish
}

proc report_cell_ { cell } {
  set lib [$cell library]
  puts "Cell [$cell name]"
  puts "Library [$lib name]"
  set filename [$cell filename]
  if { $filename != "" } {
    puts "File $filename"
  }
  set iter [$cell port_iterator]
  while {[$iter has_next]} {
    set port [$iter next]
    if { [$port is_bus] } {
      puts " [$port bus_name] [$port direction]"
    } else {
      puts " [$port name] [$port direction]"
    }
  }
  $iter finish
}

define_cmd_args "report_pin" {pin_path [> filename] [>> filename]}

proc_redirect report_pin {
  check_argc_eq1 "report_pin" $args
  set pin_path [lindex $args 0]
  set pin [get_pin_warn "pin" $pin_path]
  if { $pin != "NULL" } {
    report_pin_ $pin
  }
}

################################################################

define_cmd_args "report_net" \
  {[-connections] [-verbose] [-digits digits] [-hier_pins]\
     net_path [> filename] [>> filename]}

# -hpins to show hierarchical pins
proc_redirect report_net {
  global sta_report_default_digits

  parse_key_args "report_net" args keys {-corner -digits -significant_digits} \
    flags {-connections -verbose -hier_pins}
  check_argc_eq1 "report_net" $args

  set corner [parse_corner keys]
  set digits $sta_report_default_digits
  if { [info exists keys(-digits)] } {
      set digits $keys(-digits)
  }
  if { [info exists keys(-significant_digits)] } {
      set digits $keys(-significant_digits)
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
	sta_error "net $net_path not found."
      }
    } else {
      sta_error "net $net_path not found."
    }
  }
}

proc report_net_ { net } {
  global sta_report_default_digits
  report_net1 $net 1 1 1 $corner $sta_report_default_digits
}

proc report_net1 { net connections verbose hier_pins corner digits } {
  puts "Net [$net path_name]"
  if {$connections} {
    set pins [net_connected_pins_sorted $net]
    if {$verbose} {
      report_net_caps $net $pins $corner $digits
    }
    puts "Driver pins"
    report_net_pins $pins "is_driver" $verbose $corner $digits
    puts ""
    puts "Load pins"
    report_net_pins $pins "is_load" $verbose $corner $digits
    if {$hier_pins} {
      puts ""
      puts "Hierarchical pins"
      report_net_pins $pins "is_hierarchical" $verbose $corner $digits
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
  set pins [lsort -command path_name_cmp $pins]
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
    } elseif [$pin is_load] {
      incr load_count
    }
  }
  puts " Number of drivers: $driver_count"
  puts " Number of loads: $load_count"
  puts " Number of pins: $pin_count"
  puts ""
}

proc report_net_cap { net caption cap_msg corner digits } {
  set cap_r_min [$net $cap_msg "rise" $corner "min"]
  set cap_r_max [$net $cap_msg "rise" $corner "max"]
  set cap_f_min [$net $cap_msg "fall" $corner "min"]
  set cap_f_max [$net $cap_msg "fall" $corner "max"]
  puts " $caption capacitance: [capacitances_str $cap_r_min $cap_r_max $cap_f_min $cap_f_max $digits]"
}

proc report_net_pins { pins pin_pred verbose corner digits } {
  foreach pin $pins {
    if {[$pin $pin_pred]} {
      report_net_pin $pin $verbose $corner $digits
    }
  }
}

proc report_net_other_pins { pins verbose corner digits } {
  set header 0
  foreach pin $pins {
    if { !([$pin is_driver] || [$pin is_load] || [$pin is_hierarchical]) } {
      if { !$header } {
	puts ""
	puts "Other pins"
	set header 1
      }
      report_net_pin $pin $verbose $corner $digits
    }
  }
}

proc report_net_pin { pin verbose corner digits } {
  if [$pin is_leaf] {
    set cell_name [[[$pin instance] cell] name]
    puts -nonewline " [$pin path_name] [$pin direction] ($cell_name)"
    if { $verbose } {
      set liberty_port [$pin liberty_port]
      if { $liberty_port != "NULL" } {
	puts -nonewline [port_capacitance_str $liberty_port $digits]
      }
    }
    puts ""
  } elseif [$pin is_top_level_port] {
    puts -nonewline " [$pin path_name] [$pin direction] port"
    if { $verbose } {
      set port [$pin port]
      set cap_r_min [port_ext_wire_cap $port "rise" "min"]
      set cap_r_max [port_ext_wire_cap $port "rise" "max"]
      set cap_f_min [port_ext_wire_cap $port "fall" "min"]
      set cap_f_max [port_ext_wire_cap $port "fall" "max"]
      if { $cap_r_min > 0 || $cap_r_max > 0 || $cap_f_min > 0 || $cap_r_max > 0 } {
	puts -nonewline " wire [capacitances_str $cap_r_min $cap_r_max $cap_f_min $cap_f_max $digits]"
      }

      set port [$pin port]
      set cap_r_min [port_ext_pin_cap $port "rise" "min"]
      set cap_r_max [port_ext_pin_cap $port "rise" "max"]
      set cap_f_min [port_ext_pin_cap $port "fall" "min"]
      set cap_f_max [port_ext_pin_cap $port "fall" "max"]
      if { $cap_r_min > 0 || $cap_r_max > 0 || $cap_f_min > 0 || $cap_r_max > 0} {
	puts -nonewline " pin [capacitances_str $cap_r_min $cap_r_max $cap_f_min $cap_f_max $digits]"
      }
    }
    puts ""
  } elseif [$pin is_hierarchical] {
    puts " [$pin path_name] [$pin direction]"
  }
}

################################################################

proc report_pin_ { pin } {
  global sta_report_default_digits

  puts -nonewline "Pin [$pin path_name] [pin_direction_desc $pin]"

  set liberty_port [$pin liberty_port]
  if { $liberty_port != "NULL" } {
    puts -nonewline [port_capacitance_str $liberty_port $sta_report_default_digits]
  }

  if { [$pin is_top_level_port] } {
    set net [[$pin term] net]
  } else {
    set net [$pin net]
  }
  if { $net == "NULL" } {
    puts " (unconnected)"
  } else {
    puts " [[$net highest_connected_net] path_name]"
  }
}

proc pin_direction_desc { pin } {
  if [$pin is_hierarchical] {
    return "hierarchical [$pin direction]"
  } elseif [$pin is_top_level_port] {
    return "[$pin direction] port"
  } else {
    return [$pin direction]
  }
}

# Do not preceed this field by a space in the caller.
proc port_capacitance_str { liberty_port digits } {
  set cap_r_min [$liberty_port capacitance "rise" "min"]
  set cap_r_max [$liberty_port capacitance "rise" "max"]
  set cap_f_min [$liberty_port capacitance "fall" "min"]
  set cap_f_max [$liberty_port capacitance "fall" "max"]
  if { $cap_r_min > 0 || $cap_r_max > 0 || $cap_f_min > 0 || $cap_r_max > 0 } {
    return " [capacitances_str $cap_r_min $cap_r_max $cap_f_min $cap_f_max $digits]"
  } else {
    return ""
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
