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

################################################################
#
# Command helper functions.
#
################################################################

namespace eval sta {

#################################################################
#
# Argument parsing functions.
#
################################################################

# Parse multiple object type args.
# For object name args the search order is as follows:
#  clk
#  liberty_cell
#  liberty_port
#  cell
#  inst
#  port
#  pin
#  net

proc get_object_args { objects clks_var libcells_var libports_var \
			 cells_var insts_var ports_var pins_var nets_var \
			 edges_var timing_arc_sets_var } {
  if { $clks_var != {} } {
    upvar 1 $clks_var clks
  }
  if { $libcells_var != {} } {
    upvar 1 $libcells_var libcells
  }
  if { $libports_var != {} } {
    upvar 1 $libports_var libports
  }
  if { $cells_var != {} } {
    upvar 1 $cells_var cells
  }
  if { $insts_var != {} } {
    upvar 1 $insts_var insts
  }
  if { $ports_var != {} } {
    upvar 1 $ports_var ports
  }
  if { $pins_var != {} } {
    upvar 1 $pins_var pins
  }
  if { $nets_var != {} } {
    upvar 1 $nets_var nets
  }
  if { $edges_var != {} } {
    upvar 1 $edges_var edges
  }
  if { $timing_arc_sets_var != {} } {
    upvar 1 $timing_arc_sets_var timing_arc_sets
  }

  # Copy backslashes that will be removed by foreach.
  set objects [string map {\\ \\\\} $objects]
  foreach obj $objects {
    if { [llength $obj] > 1 } {
      # List arg. Recursive call without initing objects.
      get_object_args $obj $clks_var $libcells_var $libports_var $cells_var $insts_var \
	$ports_var $pins_var $nets_var $edges_var $timing_arc_sets_var
    } elseif { [is_object $obj] } {
      # Explicit object arg.
      set object_type [object_type $obj]
      if { $pins_var != {} && $object_type == "Pin" } {
	lappend pins $obj
      } elseif { $insts_var != {} && $object_type == "Instance" } {
	lappend insts $obj
      } elseif { $nets_var != {} && $object_type == "Net" } {
	lappend nets $obj
      } elseif { $ports_var != {} && $object_type == "Port" } {
	lappend ports $obj
      } elseif { $edges_var != {} && $object_type == "Edge" } {
	lappend edges $obj
      } elseif { $clks_var != {} && $object_type == "Clock" } {
	lappend clks $obj
      } elseif { $libcells_var != {} && $object_type == "LibertyCell" } {
	lappend libcells $obj
      } elseif { $libports_var != {} && $object_type == "LibertyPort" } {
	lappend libports $obj
      } elseif { $cells_var != {} && $object_type == "Cell" } {
	lappend cells $obj
      } elseif { $timing_arc_sets_var != {} \
		   && $object_type == "TimingArcSet" } {
	lappend timing_arc_sets $obj
      } else {
	sta_error 100 "unsupported object type $object_type."
      }
    } elseif { $obj != {} } {
      # Check for implicit arg.
      # Search for most general object type first.
      set matches {}
      if { $clks_var != {} } {
	set matches [get_clocks -quiet $obj]
      }
      if { $matches != {} } {
	set clks [concat $clks $matches]
      } else {
	if { $libcells_var != {} } {
	  set matches [get_lib_cells -quiet $obj]
	}
	if { $matches != {} } {
	  set libcells [concat $libcells $matches]
	} else {
	  
	  if { $libports_var != {} } {
	    set matches [get_lib_pins -quiet $obj]
	  }
	  if { $matches != {} } {
	    set libports [concat $libports $matches]
	  } else {
	    
	    if { $cells_var != {} } {
	      set matches [find_cells_matching $obj 0 0]
	    }
	    if { $matches != {} } {
	      set cells [concat $cells $matches]
	    } else {
	      
	      if { $insts_var != {} } {
		set matches [get_cells -quiet $obj]
	      }
	      if { $matches != {} } {
		set insts [concat $insts $matches]
	      } else {
		if { $ports_var != {} } {
		  set matches [get_ports -quiet $obj]
		}
		if { $matches != {} }  {
		  set ports [concat $ports $matches]
		} else {
		  if { $pins_var != {} } {
		    set matches [get_pins -quiet $obj]
		  }
		  if { $matches != {} } {
		    set pins [concat $pins $matches]
		  } else {
		    if { $nets_var != {} } {
		      set matches [get_nets -quiet $obj]
		    }
		    if { $matches != {} } {
		      set nets [concat $nets $matches]
		    } else {
		      sta_warn 101 "object '$obj' not found."
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

proc parse_clk_cell_port_args { objects clks_var cells_var ports_var } {
  upvar 1 $clks_var clks
  upvar 1 $cells_var cells
  upvar 1 $ports_var ports
  set clks {}
  set cells {}
  set ports {}
  get_object_args $objects clks {} {} cells {} ports {} {} {} {}
}

proc parse_clk_cell_port_pin_args { objects clks_var cells_var ports_var \
				      pins_arg } {
  upvar 1 $clks_var clks
  upvar 1 $cells_var cells
  upvar 1 $ports_var ports
  upvar 1 $pins_arg pins
  set clks {}
  set cells {}
  set ports {}
  set pins {}
  get_object_args $objects clks {} {} cells {} ports pins {} {} {}
}

proc parse_clk_inst_pin_arg { objects clks_var insts_var pins_var } {
  upvar 1 $clks_var clks
  upvar 1 $insts_var insts
  upvar 1 $pins_var pins
  set clks {}
  set insts {}
  set pins {}
  get_object_args $objects clks {} {} {} insts {} pins {} {} {}
}

proc parse_clk_inst_port_pin_arg { objects clks_var insts_var pins_var } {
  upvar 1 $clks_var clks
  upvar 1 $insts_var insts
  upvar 1 $pins_var pins
  set clks {}
  set insts {}
  set pins {}
  set ports {}
  get_object_args $objects clks {} {} {} insts ports pins {} {} {}
  foreach port $ports {
    lappend pins [[top_instance] find_pin [get_name $port]]
  }
}

proc parse_clk_port_pin_arg { objects clks_var pins_var } {
  upvar 1 $clks_var clks
  upvar 1 $pins_var pins
  set clks {}
  set pins {}
  set ports {}
  get_object_args $objects clks {} {} {} {} ports pins {} {} {}
  foreach port $ports {
    lappend pins [[top_instance] find_pin [get_name $port]]
  }
}

proc parse_libcell_libport_inst_port_pin_edge_timing_arc_set_arg { objects \
								     libcells_var \
								     libports_var \
								     insts_var \
								     ports_var \
								     pins_var \
								     edges_var \
								     timing_arc_sets_var } {
  upvar 1 $libcells_var libcells
  upvar 1 $libports_var libports
  upvar 1 $insts_var insts
  upvar 1 $ports_var ports
  upvar 1 $pins_var pins
  upvar 1 $edges_var edges
  upvar 1 $timing_arc_sets_var timing_arc_sets

  set libcells {}
  set libports {}
  set insts {}
  set ports {}
  set pins {}
  set edges {}
  set timing_arc_sets {}
  get_object_args $objects {} libcells libports {} insts ports pins {} \
    edges timing_arc_sets
}

proc parse_libcell_arg { objects } {
  set libcells {}
  get_object_args $objects {} libcells {} {} {} {} {} {} {} {}
  return $libcells
}

proc parse_libcell_inst_arg { objects libcells_var insts_var } {
  upvar 1 $libcells_var libcells
  upvar 1 $insts_var insts
  set libcells {}
  set insts {}
  get_object_args $objects {} libcells {} {} insts {} {} {} {} {}
}

proc parse_libcell_inst_net_arg { objects libcells_var insts_var nets_var } {
  upvar 1 $libcells_var libcells
  upvar 1 $insts_var insts
  upvar 1 $nets_var nets
  set libcells {}
  set insts {}
  set nets {}
  get_object_args $objects {} libcells {} {} insts {} {} nets {} {}
}

proc parse_cell_arg { objects } {
  set cells {}
  get_object_args $objects {} {} {} cells {} {} {} {} {} {}
  return $cells
}

proc parse_cell_port_args { objects cells_var ports_var } {
  upvar 1 $cells_var cells
  upvar 1 $ports_var ports
  set cells {}
  set ports {}
  get_object_args $objects {} {} {} cells {} ports {} {} {} {}
}

proc parse_cell_port_pin_args { objects cells_var ports_var pins_var } {
  upvar 1 $cells_var cells
  upvar 1 $ports_var ports
  upvar 1 $pins_var pins
  set cells {}
  set ports {}
  set pins {}
  get_object_args $objects {} {} {} cells {} ports pins {} {} {}
}

proc parse_inst_port_pin_arg { objects insts_var pins_var } {
  upvar 1 $insts_var insts
  upvar 1 $pins_var pins
  set insts {}
  set pins {}
  set ports {}
  get_object_args $objects {} {} {} {} insts ports pins {} {} {}
  foreach port $ports {
    lappend pins [[top_instance] find_pin [get_name $port]]
  }
}

proc parse_inst_pin_arg { objects insts_var pins_var } {
  upvar 1 $insts_var insts
  upvar 1 $pins_var pins
  set insts {}
  set pins {}
  get_object_args $objects {} {} {} {} insts {} pins {} {} {}
}

proc parse_inst_port_pin_net_arg { objects insts_var pins_var nets_var } {
  upvar 1 $insts_var insts
  upvar 1 $pins_var pins
  upvar 1 $nets_var nets
  set insts {}
  set ports {}
  set pins {}
  set nets {}
  get_object_args $objects {} {} {} {} insts ports pins nets {} {}
  foreach port $ports {
    lappend pins [[top_instance] find_pin [get_name $port]]
  }
}

proc parse_inst_net_arg { objects insts_var nets_var } {
  upvar 1 $insts_var insts
  upvar 1 $nets_var nets
  set insts {}
  set nets {}
  get_object_args $objects {} {} {} {} insts {} {} nets {} {}
}

proc parse_port_pin_net_arg { objects pins_var nets_var } {
  upvar 1 $pins_var pins
  upvar 1 $nets_var nets
  set ports {}
  set pins {}
  set nets {}
  get_object_args $objects {} {} {} {} {} ports pins nets {} {}

  foreach port $ports {
    lappend pins [[top_instance] find_pin [get_name $port]]
  }
}

proc parse_port_net_args { objects ports_var nets_var } {
  upvar 1 $ports_var ports
  upvar 1 $nets_var nets
  set ports {}
  set nets {}
  get_object_args $objects {} {} {} {} {} ports {} nets {} {}
}

proc parse_pin_net_args { objects pins_var nets_var } {
  upvar 1 $pins_var pins
  upvar 1 $nets_var nets
  set pins {}
  set nets {}
  get_object_args $objects {} {} {} {} {} {} pins nets {} {}
}

proc get_ports_or_pins { pattern } {
  set matches [find_port_pins_matching $pattern 0 0]
  if { $matches != {} } {
    return $matches
  } else {
    return [find_pins_matching $pattern 0 0]
  }
}

################################################################

# -corner keyword is optional.
# If -corner keyword is missing:
#  one corner: return default
#  multiple corners: error
proc parse_corner { keys_var } {
  upvar 1 $keys_var keys

  if { [info exists keys(-corner)] } {
    set corner_arg $keys(-corner)
    if { [is_object $corner_arg] } {
      set object_type [object_type $corner_arg]
      if { $object_type == "Corner" } {
        return $corner_arg
      } else {
        sta_error 144 "corner object type '$object_type' is not a corner."
      }
    } else {
      set corner [find_corner $corner_arg]
      if { $corner == "NULL" } {
        sta_error 102 "$corner_arg is not the name of process corner."
      } else {
        return $corner
      }
    }
  } elseif { [multi_corner] } {
    sta_error 103 "-corner keyword required with multi-corner analysis."
  } else {
    return [cmd_corner]
  }
}

# -corner keyword is required.
proc parse_corner_required { keys_var } {
  upvar 1 $keys_var keys

  if { [info exists keys(-corner)] } {
    set corner_name $keys(-corner)
    set corner [find_corner $corner_name]
    if { $corner == "NULL" } {
      sta_error 104 "$corner_name is not the name of process corner."
    } else {
      return $corner
    }
  } else {
    sta_error 105 "missing -corner arg."
  }
}

proc parse_corner_or_default { keys_var } {
  upvar 1 $keys_var keys

  if { [info exists keys(-corner)] } {
    set corner_name $keys(-corner)
    set corner [find_corner $corner_name]
    if { $corner == "NULL" } {
      sta_error 106 "$corner_name is not the name of process corner."
    } else {
      return $corner
    }
  } else {
    return [cmd_corner]
  }
}

# Return NULL for all.
proc parse_corner_or_all { keys_var } {
  upvar 1 $keys_var keys

  if { [info exists keys(-corner)] } {
    set corner_name $keys(-corner)
    set corner [find_corner $corner_name]
    if { $corner == "NULL" } {
      sta_error 107 "$corner_name is not the name of process corner."
    } else {
      return $corner
    }
  } else {
    return "NULL"
  }
}

################################################################

proc parse_rise_fall_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-rise)] && ![info exists flags(-fall)] } {
    return "rise"
  } elseif { [info exists flags(-fall)] && ![info exists flags(-rise)] } {
    return "fall"
  } else {
    return "rise_fall"
  }
}

proc parse_rise_fall_arg { arg } {
  if { $arg eq "r" || $arg eq "^" || $arg eq "rise" } {
    return "rise"
  } elseif { $arg eq "f" || $arg eq "v" || $arg eq "fall" } {
    return "fall"
  } else {
    error "unknown rise/fall transition name."
  }
}

proc parse_min_max_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-min)] && [info exists flags(-max)] } {
    sta_error 108 "both -min and -max specified."
  } elseif { [info exists flags(-min)] && ![info exists flags(-max)] } {
    return "min"
  } elseif { [info exists flags(-max)] && ![info exists flags(-min)] } {
    return "max"
  } else {
    # Default.
    return "max"
  }
}

proc parse_min_max_all_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-min)] && [info exists flags(-max)] } {
    sta_error 109 "both -min and -max specified."
  } elseif { [info exists flags(-min)] && ![info exists flags(-max)] } {
    return "min"
  } elseif { [info exists flags(-max)] && ![info exists flags(-min)] } {
    return "max"
  } else {
    return "min_max"
  }
}

# parse_min_max_all_flags and require analysis type to be min/max.
proc parse_min_max_all_check_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-min)] && [info exists flags(-max)] } {
    return "min_max"
  } elseif { [info exists flags(-min)] && ![info exists flags(-max)] } {
    return "min"
  } elseif { [info exists flags(-max)] && ![info exists flags(-min)] } {
    return "max"
  } else {
    return "min_max"
  }
}

proc parse_early_late_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-early)] && [info exists flags(-late)] } {
    sta_error 110 "only one of -early and -late can be specified."
  } elseif { [info exists flags(-early)] } {
    return "min"
  } elseif { [info exists flags(-late)] } {
    return "max"
  } else {
    sta_error 111 "-early or -late must be specified."
  }
}

proc parse_early_late_all_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-early)] && [info exists flags(-late)] } {
    sta_error 112 "both -early and -late specified."
  } elseif { [info exists flags(-early)] && ![info exists flags(-late)] } {
    return "min"
  } elseif { [info exists flags(-late)] && ![info exists flags(-early)] } {
    return "max"
  } else {
    return "min_max"
  }
}

################################################################

proc get_liberty_error { arg_name arg } {
  set lib "NULL"
  if {[llength $arg] > 1} {
    sta_error 113 "$arg_name must be a single library."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "LibertyLibrary" } {
      set lib $arg
    } else {
      sta_error 114 "$arg_name type '$object_type' is not a library."
    }
  } else {
    set lib [find_liberty $arg]
    if { $lib == "NULL" } {
      sta_error 115 "library '$arg' not found."
    }
  }
  return $lib
}

proc get_lib_cell_warn { arg_name arg } {
  return [get_lib_cell_arg $arg_name $arg sta_warn]
}

proc get_lib_cell_error { arg_name arg } {
  return [get_lib_cell_arg $arg_name $arg sta_error]
}

proc get_lib_cell_arg { arg_name arg error_proc } {
  set lib_cell "NULL"
  if { [llength $arg] > 1 } {
    sta_error 116 "$arg_name must be a single lib cell."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "LibertyCell" } {
      set lib_cell $arg
    } else {
      $error_proc 116 "$arg_name type '$object_type' is not a liberty cell."
    }
    # Parse library_name/cell_name.
  } elseif {[regexp [cell_regexp] $arg ignore lib_name cell_name]} {
    set library [find_liberty $lib_name]
    if { $library != "NULL" } {
      set lib_cell [$library find_liberty_cell $cell_name]
      if { $lib_cell == "NULL" } {
	$error_proc 117 "liberty cell '$arg' not found."
      }
    } else {
      $error_proc 118  "library '$lib_name' not found."
    }
  } else {
    set lib_cell [find_liberty_cell $arg]
    if { $lib_cell == "NULL" } {
      $error_proc 119 "liberty cell '$arg' not found."
    }
  }
  return $lib_cell
}

# Not used by OpenSTA
proc get_lib_cells_arg { arg_name arglist error_proc } {
  set lib_cells {}
  # Copy backslashes that will be removed by foreach.
  set arglist [string map {\\ \\\\} $arglist]
  foreach arg $arglist {
    if {[llength $arg] > 1} {
      # Embedded list.
      set lib_cells [concat $lib_cells [get_lib_cells_arg $arg_name $arg $error_proc]]
    } elseif { [is_object $arg] } {
      set object_type [object_type $arg]
      if { $object_type == "LibertyCell" } {
	lappend lib_cells $arg
      } else {
	$error_proc 120 "unsupported object type $object_type."
      }
    } elseif { $arg != {} } {
      set arg_lib_cells [get_lib_cells1 $arg $error_proc]
      if { $arg_lib_cells != {} } {
	set lib_cells [concat $lib_cells $arg_lib_cells]
      }
    }
  }
  return $lib_cells
}

# Based on get_lib_cells
proc get_lib_cells1 { patterns error_proc } {
  global hierarchy_separator

  set cells {}
  set cell_regexp [cell_regexp_hsc $hierarchy_separator]
  foreach pattern $patterns {
    if { ![regexp $cell_regexp $pattern ignore lib_name cell_pattern]} {
      set lib_name "*"
      set cell_pattern $pattern
    }
    # Allow wildcards in the library name (incompatible).
    set libs [get_libs -quiet $lib_name]
    if { $libs == {} } {
      $error_proc 121 "library '$lib_name' not found."
    } else {
      foreach lib $libs {
        set matches [$lib find_liberty_cells_matching $cell_pattern 0 0]
        if {$matches != {}} {
          set cells [concat $cells $matches]
        }
      }
      if { $cells == {} } {
        $error_proc 122 "cell '$cell_pattern' not found."
      }
    }
  }
  return $cells
}

proc get_instance_error { arg_name arg } {
  set inst "NULL"
  if {[llength $arg] > 1} {
    sta_error 123 "$arg_name must be a single instance."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Instance" } {
      set inst $arg
    } else {
      sta_error 124 "$arg_name type '$object_type' is not an instance."
    }
  } else {
    set inst [find_instance $arg]
    if { $inst == "NULL" } {
      sta_error 125 "instance '$arg' not found."
    }
  }
  return $inst
}

proc get_instances_error { arg_name arglist } {
  set insts {}
  # Copy backslashes that will be removed by foreach.
  set arglist [string map {\\ \\\\} $arglist]
  foreach arg $arglist {
    if {[llength $arg] > 1} {
      # Embedded list.
      set insts [concat $insts [get_instances_error $arg_name $arg]]
    } elseif { [is_object $arg] } {
      set object_type [object_type $arg]
      if { $object_type == "Instance" } {
        lappend insts $arg
      } else {
        sta_error 126 "$arg_name type '$object_type' is not an instance."
      }
    } elseif { $arg != {} } {
      set arg_insts [get_cells -quiet $arg]
      if { $arg_insts != {} } {
        set insts [concat $insts $arg_insts]
      } else {
        sta_error 127 "instance '$arg' not found."
      }
    }
  }
  return $insts
}

proc get_port_pin_warn { arg_name arg } {
  return [get_port_pin_arg $arg_name $arg "warn"]
}

proc get_port_pin_error { arg_name arg } {
  return [get_port_pin_arg $arg_name $arg "error"]
}

proc get_port_pin_arg { arg_name arg warn_error } {
  set pin "NULL"
  if {[llength $arg] > 1} {
    sta_warn_error 128 $warn_error "$arg_name must be a single port or pin."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Pin" } {
      set pin $arg
    } elseif { $object_type == "Port" } {
      # Explicit port arg - convert to pin.
      set pin [find_pin [get_name $arg]]
    } else {
      sta_warn_error 129 $warn_error "$arg_name type '$object_type' is not a pin or port."
    }
  } else {
    set top_instance [top_instance]
    set top_cell [$top_instance cell]
    set port [$top_cell find_port $arg]
    if { $port == "NULL" } {
      set pin [find_pin $arg]
    } else {
      set pin [$top_instance find_pin [get_name $port]]
    }
    if { $pin == "NULL" } {
      sta_warn_error 130 $warn_error "pin $arg not found."
    }
  }
  return $pin
}

proc get_port_pins_error { arg_name arglist } {
  set pins {}
  # Copy backslashes that will be removed by foreach.
  set arglilst [string map {\\ \\\\} $arglist]
  foreach arg $arglist {
    if {[llength $arg] > 1} {
      # Embedded list.
      set pins [concat $pins [get_port_pins_error $arg_name $arg]]
    } elseif { [is_object $arg] } {
      set object_type [object_type $arg]
      if { $object_type == "Pin" } {
        lappend pins $arg
      } elseif { $object_type == "Port" } {
        # Convert port to pin.
        lappend pins [find_pin [get_name $arg]]
      } else {
        sta_error 131 "$arg_name type '$object_type' is not a pin or port."
      }
    } elseif { $arg != {} } {
      set arg_pins [get_ports_or_pins $arg]
      if { $arg_pins != {} } {
        set pins [concat $pins $arg_pins]
      } else {
        sta_error 132 "pin '$arg' not found."
      }
    }
  }
  return $pins
}

proc get_ports_error { arg_name arglist } {
  set ports {}
  # Copy backslashes that will be removed by foreach.
  set arglist [string map {\\ \\\\} $arglist]
  foreach arg $arglist {
    if {[llength $arg] > 1} {
      # Embedded list.
      set ports [concat $ports [get_ports_error $arg_name $arg]]
    } elseif { [is_object $arg] } {
      set object_type [object_type $arg]
      if { $object_type == "Port" } {
        lappend ports $arg
      } else {
        sta_error 133 "$arg_name type '$object_type' is not a port."
      }
    } elseif { $arg != {} } {
      set arg_ports [get_ports $arg]
      if { $arg_ports != {} } {
        set ports [concat $ports $arg_ports]
      }
    }
  }
  return $ports
}

proc get_pin_error { arg_name arg } {
  return [get_pin_arg $arg_name $arg "error"]
}

proc get_pin_warn { arg_name arg } {
  return [get_pin_arg $arg_name $arg "warn"]
}

proc get_pin_arg { arg_name arg warn_error } {
  set pin "NULL"
  if {[llength $arg] > 1} {
    sta_warn_error 134 $warn_error "$arg_name must be a single pin."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Pin" } {
      set pin $arg
    } else {
      sta_warn_error 135 $warn_error "$arg_name type '$object_type' is not a pin."
    }
  } else {
    set pin [find_pin $arg]
    if { $pin == "NULL" } {
      sta_warn_error 136 $warn_error "$arg_name pin $arg not found."
    }
  }
  return $pin
}

proc get_clock_warn { arg_name arg } {
  return [get_clock_arg $arg_name $arg sta_warn]
}

proc get_clock_error { arg_name arg } {
  return [get_clock_arg $arg_name $arg sta_error]
}

proc get_clock_arg { arg_name arg error_proc } {
  set clk "NULL"
  if {[llength $arg] > 1} {
    $error_proc 137 "$arg_name arg must be a single clock, not a list."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Clock" } {
      set clk $arg
    } else {
      $error_proc 138 "$arg_name arg value is a $object_type, not a clock."
    }
  } elseif { $arg != {} } {
    set clk [find_clock $arg]
    if { $clk == "NULL" } {
      $error_proc 138 "$arg_name arg '$arg' clock not found."
    }
  }
  return $clk
}

proc get_clocks_warn { arg_name arglist } {
  set clks {}
  # Copy backslashes that will be removed by foreach.
  set arglist [string map {\\ \\\\} $arglist]
  foreach arg $arglist {
    if {[llength $arg] > 1} {
      # Embedded list.
      set clks [concat $clks [get_clocks_warn $arg_name $arg]]
    } elseif { [is_object $arg] } {
      set object_type [object_type $arg]
      if { $object_type == "Clock" } {
        lappend clks $arg
      } else {
        sta_warn 139 "unsupported object type $object_type."
      }
    } elseif { $arg != {} } {
      set arg_clocks [get_clocks $arg]
      if { $arg_clocks != {} } {
        set clks [concat $clks $arg_clocks]
      }
    }
  }
  return $clks
}

proc get_net_arg { arg_name arg } {
  set net "NULL"
  if {[llength $arg] > 1} {
    sta_warn  140 "$arg_name must be a single net."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Net" } {
      set net $arg
    } else {
      sta_warn 141 "$arg_name '$object_type' is not a net."
    }
  } else {
    set net [find_net $arg]
    if { $net == "NULL" } {
      sta_warn 143 "$arg_name '$arg' not found."
    }
  }
  return $net
}

proc get_nets_arg { arg_name arglist } {
  set nets {}
  # Copy backslashes that will be removed by foreach.
  set arglist [string map {\\ \\\\} $arglist]
  foreach arg $arglist {
    if {[llength $arg] > 1} {
      # Embedded list.
      set nets [concat $nets [get_nets_arg $arg_name $arg]]
    } elseif { [is_object $arg] } {
      set object_type [object_type $arg]
      if { $object_type == "Net" } {
        lappend nets $arg
      } else {
        sta_warn 142 "unsupported object type $object_type."
      }
    } elseif { $arg != {} } {
      set arg_nets [get_nets $arg]
      if { $arg_nets != {} } {
        set nets [concat $nets $arg_nets]
      }
    }
  }
  return $nets
}

# sta namespace end.
}
