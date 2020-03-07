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

# Network editing commands.

namespace eval sta {

proc connect_pin { net pin } {
  set insts_port [parse_connect_pin $pin]
  if { $insts_port == 0 } {
    return 0
  }
  set net [get_net_warn "net" $net]
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
      sta_error "unsupported object type $object_type."
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

proc connect_pins { net pins } {
  sta_warn "connect_pins is deprecated.  Use connect_pin."
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
  set inst_ports {}
  # Copy backslashes that will be removed by foreach.
  set arg [string map {\\ \\\\} $arg]
  foreach obj $arg {
    set inst_port [parse_connect_pin $obj]
    if { $inst_port == 0 } {
      return 0
    }
    set inst_ports [concat $inst_ports $inst_port]
  }
  return $inst_ports
}

################################################################

proc delete_instance { instance } {
  if { [is_object $instance] } {
    set object_type [object_type $instance]
    if { $object_type == "Instance" } {
      set inst $obj
    } else {
      sta_error "unsupported object type $object_type."
    }
  } else {
    set inst [find_instance $instance]
  }
  if { $inst != "NULL" } {
    delete_instance_cmd $inst
  }
}

################################################################

proc delete_net { net } {
  if { [is_object $net] } {
    set object_type [object_type $net]
    if { $object_type != "Net" } {
      sta_error "unsupported object type $object_type."
    }
  } else {
    set net [find_net $net]
  }
  if { $net != "NULL" } {
    delete_net_cmd $net
  }
}

################################################################

proc disconnect_pin { net pin } {
  set net [get_net_warn "net" $net]
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

proc disconnect_pins { net pins } {
  sta_warn "disconnect_pins is deprecated.  Use disconnect_pin."
  foreach pin $pins {
    disconnect_pin $net $pins
  }
}

################################################################

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

proc insert_buffer { buffer_name buffer_cell net load_pins buffer_out_net_name } {
  set buffer_cell [sta::get_lib_cell_warn "buffer_cell" $buffer_cell]
  set net [sta::get_net_warn "net" $net]
  
  if { [get_cells -quiet $buffer_name] != "" } {
    sta_error "instance $buffer_name already exists."
  }
  if { [get_nets -quiet $buffer_out_net_name] != "" } {
    sta_error "net $buffer_out_net_name already exists."
  }
  # Copy backslashes that will be removed by foreach.
  set load_pins1 [string map {\\ \\\\} $load_pins]
  set load_pins {}
  foreach pin $load_pins1 {
    set pin1 [get_port_pin_warn "pin" $pin]
    if { $pin1 != "NULL" } {
      lappend load_pins $pin1
    }
  }
  if { $buffer_cell != "NULL" \
	 &&  $net  != "NULL" } {
    insert_buffer_cmd $buffer_name $buffer_cell $net $load_pins $buffer_out_net_name
  }
}

# sta namespace end.
}
