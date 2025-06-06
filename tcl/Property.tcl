# OpenSTA, Static Timing Analyzer
# Copyright (c) 2025, Parallax Software, Inc.
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
  {[-object_type library|liberty_library|cell|liberty_cell|instance|pin|net|port|clock|timing_arc] object property}

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

# sta namespace end.
}
