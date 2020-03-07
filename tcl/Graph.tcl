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

# Graph utilities

namespace eval sta {

define_cmd_args "report_edges" {[-from from_pin] [-to to_pin]}

proc report_edges { args } {
  parse_key_args "report_edges" args keys {-from -to} flags {}
  check_argc_eq0 "report_edges" $args

  if { [info exists keys(-from)] && [info exists keys(-to)] } {
    set from_pin [get_port_pin_error "from_pin" $keys(-from)]
    set to_pin [get_port_pin_error "to_pin" $keys(-to)]
    foreach from_vertex [$from_pin vertices] {
      foreach to_vertex [$to_pin vertices] {
	report_edges_between_ $from_vertex $to_vertex
      }
    }
  } elseif [info exists keys(-from)] {
    set from_pin [get_port_pin_error "from_pin" $keys(-from)]
    foreach from_vertex [$from_pin vertices] {
      report_edges_ $from_vertex out_edge_iterator \
	vertex_port_name vertex_path_name
    }
  } elseif [info exists keys(-to)] {
    set to_pin [get_port_pin_error "to_pin" $keys(-to)]
    foreach to_vertex [$to_pin vertices] {
      report_edges_ $to_vertex in_edge_iterator \
	vertex_path_name vertex_port_name
    }
  }
}

proc report_edges_between_ { from_vertex to_vertex } {
  set iter [$from_vertex out_edge_iterator]
  while {[$iter has_next]} {
    set edge [$iter next]
    if { [$edge to] == $to_vertex } {
      if { [$edge role] == "wire" } {
	report_edge_ $edge vertex_path_name vertex_path_name
      } else {
	report_edge_ $edge vertex_port_name vertex_port_name
      }
    }
  }
  $iter finish
}

proc report_edges_ { vertex iter_proc wire_from_name_proc wire_to_name_proc } {
  # First report edges internal to the device.
  set device_header 0
  set iter [$vertex $iter_proc]
  while {[$iter has_next]} {
    set edge [$iter next]
    if { [$edge role] != "wire" } {
      if { !$device_header } {
	set pin [$vertex pin]
	if { ![$pin is_top_level_port] } {
	  set inst [$pin instance]
	}
	set device_header 1
      }
      report_edge_ $edge vertex_port_name vertex_port_name
    }
  }
  $iter finish

  # Then wires. 
  set iter [$vertex $iter_proc]
  while {[$iter has_next]} {
    set edge [$iter next]
    if { [$edge role] == "wire" } {
      report_edge_ $edge $wire_from_name_proc $wire_to_name_proc
    }
  }
  $iter finish
}

proc report_edge_ { edge vertex_from_name_proc vertex_to_name_proc } {
  global sta_report_default_digits

  puts -nonewline "[$vertex_from_name_proc [$edge from]] -> [$vertex_to_name_proc [$edge to]] [$edge role]"
  set latch_enable [$edge latch_d_to_q_en]
  if { $latch_enable != "" } {
    puts " enable $latch_enable"
  } else {
    puts ""
  }

  set disables [edge_disable_reason $edge]
  if { $disables != "" } {
    puts "  Disabled by $disables"
  }

  set cond [$edge cond]
  if { $cond != "" } {
    puts "  Condition: $cond"
  }

  set mode_name [$edge mode_name]
  if { $mode_name != "" } {
    puts "  Mode: $mode_name [$edge mode_value]"
  }

  set iter [$edge timing_arc_iterator]
  while {[$iter has_next]} {
    set arc [$iter next]
    set delays [$edge arc_delay_strings $arc $sta_report_default_digits]
    set delays_fmt [format_delays $delays]
    set disable_reason ""
    if { [timing_arc_disabled $edge $arc] } {
      set disable_reason " disabled"
    }
    puts "  [$arc from_trans] -> [$arc to_trans] $delays_fmt$disable_reason"
  }
  $iter finish
}

# Separate list elements with colons.
proc format_times { values digits } {
  set result ""
  foreach value $values {
    if { $result != "" } {
      append result ":"
    }
    append result [format_time $value $digits]
  }
  return $result
}

# Separate delay list elements with colons.
proc format_delays { values } {
  set result ""
  foreach value $values {
    if { $result != "" } {
      append result ":"
    }
    append result $value
  }
  return $result
}

proc edge_disable_reason { edge } {
  set disables ""
  if [$edge is_disabled_constraint] {
    append disables "constraint"
  }
  if [$edge is_disabled_constant] {
    if { $disables != "" } { append disables ", " }
    append disables "constant"
  }
  if [$edge is_disabled_cond_default] {
    if { $disables != "" } { append disables ", " }
    append disables "cond_default"
  }
  if [$edge is_disabled_loop] {
    if { $disables != "" } { append disables ", " }
    append disables "loop"
  }
  if [$edge is_disabled_bidirect_inst_path] {
    if { $disables != "" } { append disables ", " }
    append disables "bidirect instance path"
  }
  if [$edge is_disabled_bidirect_net_path] {
    if { $disables != "" } { append disables ", " }
    append disables "bidirect net path"
  }
  if { [$edge is_disabled_preset_clear] } {
    if { $disables != "" } { append disables ", " }
    append disables "timing_enable_preset_clear_arcs"
  }
  return $disables
}

################################################################

define_cmd_args "report_constant" {pin|instance|net}

proc report_constant { obj } {
  parse_inst_port_pin_net_arg $obj insts pins nets
  foreach pin $pins {
    report_pin_constant $pin
  }
  foreach inst $insts {
    set pin_iter [$inst pin_iterator]
    while {[$pin_iter has_next]} {
      set pin [$pin_iter next]
      report_pin_constant $pin
    }
    $pin_iter finish
  }
  foreach net $nets {
    set pin_iter [$net pin_iterator]
    while {[$pin_iter has_next]} {
      set pin [$pin_iter next]
      report_pin_constant $pin
    }
    $pin_iter finish
  }
}

proc report_pin_constant { pin } {
  set sim_value [pin_sim_logic_value $pin]
  puts -nonewline "[pin_property $pin lib_pin_name] $sim_value"
  set case_value [pin_case_logic_value $pin]
  if { $case_value != "X" } {
    puts -nonewline " case=$case_value"
  }
  set logic_value [pin_logic_value $pin]
  if { $logic_value != "X" } {
    puts -nonewline " logic=$logic_value"
  }
  puts ""
}

################################################################

proc report_disabled_edges {} {
  foreach edge [disabled_edges_sorted] {
    if { [$edge role] == "wire" } {
      set from_pin_name [get_full_name [[$edge from] pin]]
      set to_pin_name [get_full_name [[$edge to] pin]]
      puts -nonewline "$from_pin_name $to_pin_name"
    } else {
      set from_pin [[$edge from] pin]
      set to_pin [[$edge to] pin]
      set inst_name [get_full_name [$from_pin instance]]
      set from_port_name [get_name [$from_pin port]]
      set to_port_name [get_name [$to_pin port]]
      puts -nonewline "$inst_name $from_port_name $to_port_name"
      set cond [$edge cond]
      if { $cond != "" } {
	puts -nonewline " when: $cond"
      }
    }
    set reason [edge_disable_reason_verbose $edge]
    puts " $reason"
  }
}

proc edge_disable_reason_verbose { edge } {
  set disables ""
  if { [$edge is_disabled_constraint] } {
    append disables "constraint"
  }
  if { [$edge is_disabled_constant] } {
    if { $disables != "" } { append disables ", " }
    append disables "constant"
    set sense [$edge sim_timing_sense]
    if { $sense == "positive_unate" || $sense == "negative_unate" } {
      append disables " $sense"
    }
    set const_pins [$edge disabled_constant_pins]
    foreach pin [sort_by_full_name $const_pins] {
      set port_name [pin_property $pin lib_pin_name]
      set value [pin_sim_logic_value $pin]
      append disables " $port_name=$value"
    }
  }
  if { [$edge is_disabled_cond_default] } {
    if { $disables != "" } { append disables ", " }
    append disables "cond_default"
  }
  if { [$edge is_disabled_loop] } {
    if { $disables != "" } { append disables ", " }
    append disables "loop"
  }
  if { [$edge is_disabled_preset_clear] } {
    if { $disables != "" } { append disables ", " }
    append disables "timing_enable_preset_clear_arcs"
  }
  return $disables
}

################################################################

define_cmd_args "report_slews" { pin }

proc report_slews { pin } {
  global sta_report_default_digits

  set pin1 [get_port_pin_error "pin" $pin]
  foreach vertex [$pin1 vertices] {
    puts "[vertex_path_name $vertex] [rise_short_name] [format_times [$vertex slews rise] $sta_report_default_digits] [fall_short_name] [format_times [$vertex slews fall] $sta_report_default_digits]"
  }
}

proc vertex_path_name { vertex } {
  set pin [$vertex pin]
  set pin_name [get_full_name $pin]
  return [vertex_name_ $vertex $pin $pin_name]
}

proc vertex_port_name { vertex } {
  set pin [$vertex pin]
  set pin_name [pin_property $pin lib_pin_name]
  return [vertex_name_ $vertex $pin $pin_name]
}

# Append driver/load for bidirect pin vertices.
proc vertex_name_ { vertex pin pin_name } {
  if { [pin_direction $pin] == "bidirect" } {
    if [$vertex is_bidirect_driver] {
      return "$pin_name driver"
    } else {
      return "$pin_name load  "
    }
  } else {
    return $pin_name
  }
}

# Return a list of hierarchical pins crossed by a graph edge.
proc hier_pins_crossed_by_edge { edge } {
  set from_pins [hier_pins_above [[$edge from] pin]]
  set to_pins [hier_pins_above [[$edge to] pin]]
  foreach p $to_pins { puts [$p path_name] }
  while { [llength $from_pins] > 0 \
	    && [llength $to_pins] > 0 \
	      && [lindex $from_pins 0] == [lindex $to_pins 0] } {
    set from_pins [lrange $from_pins 1 end]
    set to_pins [lrange $to_pins 1 end]
  }
  return [concat $from_pins $to_pins]
}

proc hier_pins_above { pin } {
  set pins_above {}
  set inst [$pin instance]
  set net [$pin net]
  set parent [$inst parent]
  while { $parent != "NULL" } {
    set found 0
    set parent_pin_iter [$parent pin_iterator]
    while {[$parent_pin_iter has_next]} {
      set parent_pin [$parent_pin_iter next]
      if {[$parent_pin net] == $net} {
	set pins_above [concat [list $parent_pin] $pins_above]
	set found 1
	break
      }
    }
    $parent_pin_iter finish
    if { !$found} {
      break
    }
    set parent [$parent parent]
  }
  return $pins_above
}

proc report_level { pin } {
  set pin1 [get_port_pin_error "pin" $pin]
  foreach vertex [$pin1 vertices] {
    if { $vertex != "NULL" } {
      puts "[vertex_path_name $vertex] level = [$vertex level]"
    }
  }
}

# Show how many pins are at each level from an input.
proc report_level_distribution {} {
  set max_level 0
  set iter [vertex_iterator]
  while {[$iter has_next]} {
    set vertex [$iter next]
    set level [$vertex level]
    if { $level > $max_level } {
      set max_level $level
    }
    if [info exists count($level)] {
      incr count($level)
    } else {
      set count($level) 1
    }
  }
  $iter finish

  puts "level  pin count"
  for { set level 0 } { $level < $max_level } { incr level } {
    puts "  $level      $count($level)"
  }
}

# sta namespace end
}
