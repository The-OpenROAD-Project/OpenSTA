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

# Network editing commands.

namespace eval sta {

define_cmd_args "make_instance" {inst_path lib_cell}

proc make_instance { inst_path lib_cell } {
  set lib_cell [get_lib_cell_warn "lib_cell" $lib_cell]
  if { $lib_cell != "NULL" } {
    set path_regexp [path_regexp]
    if {[regexp $path_regexp $inst_path ignore path_name inst_name]} {
      set parent [find_instance $path_name]
      if { $parent == "NULL" } {
	# Parent instance not found.  This could be a typo, but since
	# SDC does not escape hierarchy dividers it can also be
	# an escaped name.
	set inst_name $inst_path
	set parent [top_instance]
      }
    } else {
      set inst_name $inst_path
      set parent [top_instance]
    }
    return [make_instance_cmd $inst_name $lib_cell $parent]
  } else {
    return 0
  }
}

################################################################

define_cmd_args "make_net" {net_path}

proc make_net { net_path } {
  # Copy backslashes that will be removed by foreach.
  set net_path [string map {\\ \\\\} $net_path]
  set path_regexp [path_regexp]
  if {[regexp $path_regexp $net_path ignore path_name net_name]} {
    set parent [find_instance $path_name]
    if { $parent == "NULL" } {
      return 0
    }
  } else {
    set parent [top_instance]
    set net_name $net_path
  }
  return [make_net_cmd $net_name $parent]
}

################################################################

define_cmd_args "make_port" {port_name direction}

proc make_port { port_name direction } {
  make_port_pin_cmd $port_name $direction
}

################################################################

define_cmd_args "connect_pin" {net pin}

proc connect_pin { net pin } {
  set insts_port [parse_connect_pin $pin]
  if { $insts_port == 0 } {
    return 0
  }
  set net [get_net_arg "net" $net]
  if { $net == "NULL" } {
    return 0
  }
  lassign $insts_port inst port
  connect_pin_cmd $inst $port $net
  return 1
}

proc parse_connect_pin { arg } {
  set path_regexp [path_regexp]
  set insts_port {}
  if { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Pin" } {
      set pin $arg
      set inst [$pin instance]
      set port [$pin port]
    } elseif { $object_type == "Port" } {
      # Explicit port arg - convert to pin.
      set pin [find_pin [get_name $arg]]
      set inst [$pin instance]
      set port [$pin port]
    } else {
      sta_error 250 "unsupported object type $object_type."
    }
  } else {
    if {[regexp $path_regexp $arg ignore path_name port_name]} {
      set inst [find_instance $path_name]
      if { $inst == "NULL" } {
	return 0
      }
    } else {
      set inst [top_instance]
      set port_name $arg
    }
    set cell [$inst cell]
    set port [$cell find_port $port_name]
    if { $port == "NULL" } {
      return 0
    }
    set pin [$inst find_pin $port_name]
  }
  
  # Make sure the pin is not currently connected to a net.
  if { $pin != "NULL" \
	 && ![$pin is_hierarchical] \
	 && [$pin net] != "NULL" } {
    return 0
  }
  return [list $inst $port]
}

################################################################

define_cmd_args "disconnect_pin" {net -all|pin}

proc disconnect_pin { net pin } {
  set net [get_net_arg "net" $net]
  if { $net == "NULL" } {
    return 0
  }
  if { $pin == "-all" } {
    set iter [$net connected_pin_iterator]
    while {[$iter has_next]} {
      set pin [$iter next]
      disconnect_pin_cmd $pin
    }
    $iter finish
    return 1
  } else {
    set pin1 [get_port_pin_warn "pin" $pin]
    if { $pin1 == "NULL" } {
      return 0
    } else {
      disconnect_pin_cmd $pin1
      return 1
    }
  }
}

################################################################

define_cmd_args "delete_instance" {inst}

proc delete_instance { instance } {
  if { [is_object $instance] } {
    set object_type [object_type $instance]
    if { $object_type == "Instance" } {
      set inst $instance
    } else {
      sta_error 252 "unsupported object type $object_type."
    }
  } else {
    set inst [find_instance $instance]
  }
  if { $inst != "NULL" } {
    delete_instance_cmd $inst
  }
}

################################################################

define_cmd_args "delete_net" {net}

proc delete_net { net } {
  if { [is_object $net] } {
    set object_type [object_type $net]
    if { $object_type != "Net" } {
      sta_error 253 "unsupported object type $object_type."
    }
  } else {
    set net [find_net $net]
  }
  if { $net != "NULL" } {
    delete_net_cmd $net
  }
}

################################################################

define_cmd_args "replace_cell" {instance lib_cell}

proc replace_cell { instance lib_cell } {
  set cell [get_lib_cell_warn "lib_cell" $lib_cell]
  if { $cell != "NULL" } {
    set inst [get_instance_error "instance" $instance]
    set inst_cell [$inst liberty_cell]
    if { $inst_cell == "NULL" \
	   || ![equiv_cell_ports $inst_cell $cell] } {
      return 0
    }
    replace_cell_cmd $inst $cell
    return 1
  } else {
    return 0
  }
}

################################################################

proc path_regexp {} {
  global hierarchy_separator
  set id_regexp "\[^${hierarchy_separator}\]+"
  set prefix_regexp "${id_regexp}(?:${hierarchy_separator}${id_regexp})*"
  return "^(${prefix_regexp})${hierarchy_separator}(${id_regexp})$"
}

# sta namespace end.
}
