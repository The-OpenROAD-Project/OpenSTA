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

# Network editing commands.

namespace eval sta {

proc connect_pins { net pins } {
  # Visit the pins to make sure command will succeed.
  set insts_ports [parse_connect_pins $pins]
  if { $insts_ports == 0 } {
    return 0
  }
  set net [get_net_warn "net" $net]
  if { $net == "NULL" } {
    return 0
  }
  foreach {inst port} $insts_ports {
    connect_pin_cmd $inst $port $net
  }
  return 1
}

proc parse_connect_pins { arg } {
  set path_regexp [path_regexp]
  set insts_ports {}
  # Copy backslashes that will be removed by foreach.
  set arg [string map {\\ \\\\} $arg]
  foreach obj $arg {
    if { [is_object $obj] } {
      set object_type [object_type $obj]
      if { $object_type == "Pin" } {
	set pin $obj
	set inst [$pin instance]
	set port [$pin port]
      } elseif { $object_type == "Port" } {
	# Explicit port arg - convert to pin.
	set pin [find_pin [get_name $obj]]
	set inst [$pin instance]
	set port [$pin port]
      } else {
	sta_error "unsupported object type $object_type."
      }
    } else {
      if {[regexp $path_regexp $obj ignore path_name port_name]} {
	set inst [find_instance $path_name]
	if { $inst == "NULL" } {
	  return 0
	}
      } else {
	set inst [top_instance]
	set port_name $obj
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
    lappend insts_ports $inst $port
  }
  return $insts_ports
}

################################################################

proc delete_instance { instances } {
  # Copy backslashes that will be removed by foreach.
  set instances1 [string map {\\ \\\\} $instances]
  foreach obj $instances1 {
    if { [is_object $obj] } {
      set object_type [object_type $obj]
      if { $object_type == "Instance" } {
	set inst $obj
      } else {
	sta_error "unsupported object type $object_type."
      }
    } else {
      set inst [find_instance $obj]
    }
    if { $inst != "NULL" } {
      delete_instance_cmd $inst
    }
  }
}

################################################################

proc delete_net { net_list } {
  # Copy backslashes that will be removed by foreach.
  set net_list [string map {\\ \\\\} $net_list]
  foreach obj $net_list {
    if { [is_object $obj] } {
      set object_type [object_type $obj]
      if { $object_type == "Net" } {
	set net $obj
      } else {
	sta_error "unsupported object type $object_type."
      }
    } else {
      set net [find_net $obj]
    }
    if { $net != "NULL" } {
      delete_net_cmd $net
    }
  }
}

################################################################

proc disconnect_pins { net pins } {
  set net [get_net_warn "net" $net]
  if { $net == "NULL" } {
    return 0
  }
  if { $pins == "-all" } {
    set iter [$net connected_pin_iterator]
    while {[$iter has_next]} {
      set pin [$iter next]
      disconnect_pin_cmd $pin
    }
    $iter finish
    return 1
  } else {
    # Copy backslashes that will be removed by foreach.
    set pins [string map {\\ \\\\} $pins]
    # Visit the pins to make sure command will succeed.
    foreach pin $pins {
      set pin1 [get_port_pin_warn "pin" $pin]
      if { $pin1 == "NULL" } {
	return 0
      }
    }
    foreach pin $pins {
      set pin1 [get_port_pin_warn "pin" $pin]
      disconnect_pin_cmd $pin1
    }
    return 1
  }
}

################################################################

proc make_instance { inst_names lib_cell } {
  set lib_cell [get_lib_cell_warn "lib_cell" $lib_cell]
  if { $lib_cell != "NULL" } {
    set path_regexp [path_regexp]
    foreach inst_path $inst_names {
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
      make_instance_cmd $inst_name $lib_cell $parent
    }
    return 1
  } else {
    return 0
  }
}

################################################################

proc make_net { net_list } {
  # Visit the net names to make sure command will succeed.
  set path_regexp [path_regexp]
  foreach net $net_list {
    if {[regexp $path_regexp $net ignore path_name net_name]} {
      set parent [find_instance $path_name]
      if { $parent == "NULL" } {
	return 0
      }
    }
  }
  # Copy backslashes that will be removed by foreach.
  set net_list [string map {\\ \\\\} $net_list]
  foreach net $net_list {
    if {[regexp $path_regexp $net ignore path_name net_name]} {
      set parent [find_instance $path_name]
    } else {
      set net_name $net
      set parent [top_instance]
    }
    make_net_cmd $net_name $parent
  }
  return 1
}

################################################################

proc replace_cell { instances lib_cell } {
  set cell [get_lib_cell_warn "lib_cell" $lib_cell]
  if { $cell != "NULL" } {
    set insts [get_instances_error "instances" $instances]
    foreach inst $insts {
      set inst_cell [$inst liberty_cell]
      if { $inst_cell != "NULL" \
	     && ![cells_equiv_ports $inst_cell $cell] } {
	return 0
      }
    }
    foreach inst $insts {
      replace_cell_cmd $inst $cell
    }
    return 1
  } else {
    return 0
  }
}

# sta namespace end.
}
