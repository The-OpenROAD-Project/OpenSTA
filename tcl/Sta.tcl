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

namespace eval sta {

################################################################
#  
# Non-SDC commands
#
################################################################

define_cmd_args "get_fanin" \
  {-to sink_list [-flat] [-only_cells] [-startpoints_only]\
     [-levels level_count] [-pin_levels pin_count]\
     [-trace_arcs timing|enabled|all]}

proc get_fanin { args } {
  parse_key_args "get_fanin" args \
    keys {-to -levels -pin_levels -trace_arcs} \
    flags {-flat -only_cells -startpoints_only}
  if { [llength $args] != 0 } {
    cmd_usage_error "get_fanin"
  }
  if { ![info exists keys(-to)] } {
    cmd_usage_error "get_fanin"
  }
  parse_port_pin_net_arg $keys(-to) pins nets
  foreach net $nets {
    lappend pins [net_driver_pins $net]
  }
  set flat [info exists flags(-flat)]
  set only_insts [info exists flags(-only_cells)]
  set startpoints_only [info exists flags(-startpoints_only)]
  set inst_levels 0
  if { [info exists keys(-levels)] } {
    set inst_levels $keys(-levels)
  }
  set pin_levels 0
  if { [info exists keys(-pin_levels)] } {
    set pin_levels $keys(-pin_levels)
  }

  set thru_disabled 0
  set thru_constants 0
  if { [info exists keys(-trace_arcs)] } {
    set trace_arcs $keys(-trace_arcs)
    if { $trace_arcs == "timing" } {
      set thru_disabled 0
      set thru_constants 0
    } elseif { $trace_arcs == "enabled" } {
      set thru_disabled 0
      set thru_constants 0
    } elseif { $trace_arcs == "all" } {
      set thru_disabled 1
      set thru_constants 1
    } else {
      cmd_usage_error "get_fanin"
    }
  }
  if { $only_insts } {
    return [find_fanin_insts $pins $flat $startpoints_only \
	      $inst_levels $pin_levels $thru_disabled $thru_constants]
 } else {
    return [find_fanin_pins $pins $flat $startpoints_only \
	      $inst_levels $pin_levels $thru_disabled $thru_constants]
  }
}

################################################################

define_cmd_args "get_fanout" \
  {-from source_list [-flat] [-only_cells] [-endpoints_only]\
     [-levels level_count] [-pin_levels pin_count]\
     [-trace_arcs timing|enabled|all]}

proc get_fanout { args } {
  parse_key_args "get_fanout" args \
    keys {-from -levels -pin_levels -trace_arcs} \
    flags {-flat -only_cells -endpoints_only}
  if { [llength $args] != 0 } {
    cmd_usage_error "get_fanout"
  }
  parse_port_pin_net_arg $keys(-from) pins nets
  foreach net $nets {
    lappend pins [net_load_pins $net]
  }
  set flat [info exists flags(-flat)]
  set only_insts [info exists flags(-only_cells)]
  set endpoints_only [info exists flags(-endpoints_only)]

  set inst_levels 0
  if { [info exists keys(-levels)] } {
    set inst_levels $keys(-levels)
  }

  set pin_levels 0
  if { [info exists keys(-pin_levels)] } {
    set pin_levels $keys(-pin_levels)
  }

  set thru_disabled 0
  set thru_constants 0
  if { [info exists keys(-trace_arcs)] } {
    set trace_arcs $keys(-trace_arcs)
    if { $trace_arcs == "timing" } {
      set thru_disabled 0
      set thru_constants 0
    } elseif { $trace_arcs == "enabled" } {
      set thru_disabled 0
      set thru_constants 0
    } elseif { $trace_arcs == "all" } {
      set thru_disabled 1
      set thru_constants 1
    } else {
      cmd_usage_error "get_fanout"
    }
  }
  if { $only_insts } {
    return [find_fanout_insts $pins $flat $endpoints_only \
	      $inst_levels $pin_levels $thru_disabled $thru_constants]
  } else {
    return [find_fanout_pins $pins $flat $endpoints_only \
	      $inst_levels $pin_levels $thru_disabled $thru_constants]
  }
}

################################################################

define_cmd_args "get_timing_edges" \
  {[-from from_pin] [-to to_pin] [-of_objects objects] [-filter expr]}

proc get_timing_edges { args } {
  return [get_timing_edges_cmd "get_timing_edges" $args]
}

proc get_timing_edges_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-from -to -of_objects -filter} flags {}
  check_argc_eq0 $cmd $cmd_args

  set arcs {}
  if { [info exists keys(-of_objects)] } {
    if { [info exists keys(-from)] \
	   || [info exists keys(-from)] } {
      sta_error 540 "-from/-to arguments not supported with -of_objects."
    }
    set arcs [get_timing_arcs_objects $keys(-of_objects)]
  } elseif { [info exists keys(-from)] \
	   && [info exists keys(-to)] } {
    set arcs [get_timing_arcs_from_to \
	      [get_port_pin_error "from" $keys(-from)] \
	      [get_port_pin_error "to" $keys(-to)]]
  } elseif { [info exists keys(-from)] } {
    set arcs [get_timing_arcs_from $keys(-from)]
  } elseif { [info exists keys(-to)] } {
    set arcs [get_timing_arcs_to $keys(-to)]
  } else {
    cmd_usage_error $cmd
  }
  if [info exists keys(-filter)] {
    set arcs [filter_objs $keys(-filter) $arcs filter_timing_arcs "timing arc"]
  }
  return $arcs
}

proc get_timing_arcs_objects { object_arg } {
  parse_libcell_inst_arg $object_arg libcells insts
  if { $insts != {} } {
    set edges {}
    foreach inst $insts {
      lappend edges [instance_edges $inst]
    }
    return $edges
  } elseif { $libcells != {} } {
    set arc_sets {}
    foreach libcell $libcells {
      lappend arc_sets [$libcell timing_arc_sets]
    }
    return $arc_sets
  }
}

proc instance_edges { inst } {
  set edges {}
  set pin_iter [$inst pin_iterator]
  while {[$pin_iter has_next]} {
    set pin [$pin_iter next]
    foreach vertex [$pin vertices] {
      set edge_iter [$vertex out_edge_iterator]
      while {[$edge_iter has_next]} {
	set edge [$edge_iter next]
	if { [$edge role] != "wire" } {
	  lappend edges $edge
	}
      }
      $edge_iter finish
    }
  }
  $pin_iter finish
  return $edges
}

proc get_timing_arcs_from_to { from_pin_arg to_pin_arg } {
  set edges {}
  set from_pin [get_port_pin_error "from" $from_pin_arg]
  set to_pin [get_port_pin_error "to" $to_pin_arg]
  foreach from_vertex [$from_pin vertices] {
    foreach to_vertex [$to_pin vertices] {
      set edge_iter [$from_vertex out_edge_iterator]
      while {[$edge_iter has_next]} {
	set edge [$edge_iter next]
	if { [$edge to] == $to_vertex } {
	  lappend edges $edge
	}
      }
      $edge_iter finish
    }
  }
  return $edges
}

proc get_timing_arcs_from { from_pin_arg } {
  set from_pin [get_port_pin_error "from" $from_pin_arg]
  set edges {}
  foreach from_vertex [$from_pin vertices] {
    set edge_iter [$from_vertex out_edge_iterator]
    while {[$edge_iter has_next]} {
      set edge [$edge_iter next]
      lappend edges $edge
    }
    $edge_iter finish
  }
  return $edges
}

proc get_timing_arcs_to { to_pin_arg } {
  set to_pin [get_port_pin_error "to" $to_pin_arg]
  set edges {}
  foreach to_vertex [$to_pin vertices] {
    set edge_iter [$to_vertex in_edge_iterator]
    while {[$edge_iter has_next]} {
      set edge [$edge_iter next]
      lappend edges $edge
    }
    $edge_iter finish
  }
  return $edges
}

################################################################

define_cmd_args "report_clock_properties" {[clocks]}

proc_redirect report_clock_properties {
  check_argc_eq0or1 "report_clock_properties" $args
  update_generated_clks
  report_line "Clock                   Period          Waveform"
  report_line "----------------------------------------------------"
  if { [llength $args] == 0 } {
    foreach clk [all_clocks] {
      report_clock1 $clk
    }
  } else {
    foreach clk [get_clocks_warn "clock_name" [lindex $args 0]] {
      report_clock1 $clk
    }
  }
}

proc report_clock1 { clk } {
  global sta_report_default_digits

  if { [$clk waveform_valid] } {
    set digits $sta_report_default_digits
    set waveform [$clk waveform]
    if { $waveform == {} } {
      set wave "                    "
    } else {
      set wave ""
      foreach edge $waveform {
	set wave "$wave[format "%10s" [format_time $edge $digits]]"
      }
    }
    if {[$clk is_generated]} {
      set generated " (generated)"
    } else {
      set generated ""
    }
    report_line "[format %-20s [get_name $clk]][format %10s [format_time [$clk period] $digits]]  $wave$generated"
  }
}

# sta namespace end.
}
