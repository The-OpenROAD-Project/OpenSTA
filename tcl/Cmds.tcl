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

################################################################
#
# Command helper functions.
#
################################################################

namespace eval sta {

proc report_clock1 { clk } {
  global sta_report_default_digits

  if { [$clk waveform_valid] } {
    set digits $sta_report_default_digits
    puts -nonewline [format "%-20s" [get_name $clk]]
    puts -nonewline [format "%10s" [format_time [$clk period] $digits]]
    puts -nonewline "  "
    set waveform [$clk waveform]
    if { $waveform == {} } {
      puts -nonewline "                    "
    } else {
      foreach edge $waveform {
	puts -nonewline [format "%10s" [format_time $edge $digits]]
      }
    }
    if {[$clk is_generated]} {
      puts -nonewline " (generated)"
    }
    puts ""
  }
}

proc_redirect read_parasitics {
  variable native

  if { $native } {
    sta_warn "The read_parasitics command is deprecated. Use read_spef."
  }
  eval [concat read_spef $args]
}

proc check_setup_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {} flags {-verbose} 0
  # When nothing is everything.
  if { $cmd_args == {} } {
    set unconstrained_endpoints 1
    set multiple_clock 1
    set no_clock 1
    set no_input_delay 1
    set no_output_delay 1
    set loops 1
    set generated_clocks 1
  } else {
    parse_key_args $cmd cmd_args keys {} \
      flags {-no_input_delay -no_output_delay -multiple_clock -no_clock \
	        -unconstrained_endpoints -loops -generated_clocks}
    set no_input_delay [info exists flags(-no_input_delay)]
    set no_output_delay [info exists flags(-no_output_delay)]
    set multiple_clock [info exists flags(-multiple_clock)]
    set no_clock [info exists flags(-no_clock)]
    set unconstrained_endpoints [info exists flags(-unconstrained_endpoints)]
    set loops [info exists flags(-loops)]
    set generated_clocks [info exists flags(-generated_clocks)]
  }
  set verbose [info exists flags(-verbose)]
  set errors [check_timing_cmd $no_input_delay $no_output_delay \
		$multiple_clock $no_clock \
		$unconstrained_endpoints $loops \
		$generated_clocks]
  foreach error $errors {
    # First line is the error msg.
    puts [lindex $error 0]
    if { $verbose } {
      foreach obj [lrange $error 1 end] {
	puts "  $obj"
      }
    }
  }
  # return value
  expr [llength $errors] == 0
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
	sta_error "unsupported object type $list_type."
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

proc get_timing_edges_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-from -to -of_objects -filter} flags {}
  check_argc_eq0 $cmd $cmd_args

  set arcs {}
  if { [info exists keys(-of_objects)] } {
    if { [info exists keys(-from)] \
	   || [info exists keys(-from)] } {
      sta_error "-from/-to arguments not supported with -of_objects."
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
    set arcs [filter_timing_arcs1 $keys(-filter) $arcs]
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
      lappend arc_sets [libcell_timing_arc_sets $libcell]
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

proc libcell_timing_arc_sets { libcell } {
  set arc_sets {}
  set arc_iter [$libcell timing_arc_set_iterator]
  while { [$arc_iter has_next] } {
    lappend arc_sets [$arc_iter next]
  }
  $arc_iter finish
  return $arc_sets
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

proc filter_timing_arcs1 { filter objects } {
  variable filter_regexp1
  variable filter_or_regexp
  variable filter_and_regexp
  set filtered_objects {}
  # Ignore sub-exprs in filter_regexp1 for expr2 match var.
  if { [regexp $filter_or_regexp $filter ignore expr1 \
	  ignore ignore ignore expr2] } {
    regexp $filter_regexp1 $expr1 ignore attr_name op arg
    set filtered_objects1 [filter_timing_arcs $attr_name $op $arg $objects]
    regexp $filter_regexp1 $expr2 ignore attr_name op arg
    set filtered_objects2 [filter_timing_arcs $attr_name $op $arg $objects]
    set filtered_objects [concat $filtered_objects1 $filtered_objects2]
  } elseif { [regexp $filter_and_regexp $filter ignore expr1 \
		ignore ignore ignore expr2] } {
    regexp $filter_regexp1 $expr1 ignore attr_name op arg
    set filtered_objects [filter_timing_arcs $attr_name $op $arg $objects]
    regexp $filter_regexp1 $expr2 ignore attr_name op arg
    set filtered_objects [filter_timing_arcs $attr_name $op \
			    $arg $filtered_objects]
  } elseif { [regexp $filter_regexp1 $filter ignore attr_name op arg] } {
    set filtered_objects [filter_timing_arcs $attr_name $op $arg $objects]
  } else {
    sta_error "unsupported -filter expression."
  }
  return $filtered_objects
}

################################################################

proc set_assigned_delay_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {-corner -from -to} \
    flags {-cell -net -rise -fall -max -min}
  check_argc_eq1 $cmd $cmd_args
  set corner [parse_corner keys]
  set min_max [parse_min_max_all_check_flags flags]
  set to_rf [parse_rise_fall_flags flags]

  if [info exists keys(-from)] {
    set from_pins [get_port_pins_error "from_pins" $keys(-from)]
  } else {
    sta_error "$cmd missing -from argument."
  }
  if [info exists keys(-to)] {
    set to_pins [get_port_pins_error "to_pins" $keys(-to)]
  } else {
    sta_error "$cmd missing -to argument."
  }

  set delay [lindex $cmd_args 0]
  if {![string is double $delay]} {
    sta_error "$cmd delay is not a float."
  }
  set delay [time_ui_sta $delay]

  if {[info exists flags(-cell)] && [info exists flags(-net)]} {
    sta_error "set_annotated_delay -cell and -net options are mutually excluive."
  } elseif {[info exists flags(-cell)]} {
    if { $from_pins != {} } {
      set inst [[lindex $from_pins 0] instance]
      foreach pin $from_pins {
	if {[$pin instance] != $inst} {
	  sta_error "$cmd pin [get_full_name $pin] is not attached to instance [get_full_name $inst]."
	}
      }
      foreach pin $to_pins {
	if {[$pin instance] != $inst} {
	  sta_error "$cmd pin [get_full_name $pin] is not attached to instance [get_full_name $inst]"
	}
      }
    }
  } elseif {![info exists flags(-net)]} {
    sta_error "$cmd -cell or -net required."
  }
  foreach from_pin $from_pins {
    set from_vertices [$from_pin vertices]
    set_assigned_delay1 [lindex $from_vertices 0] \
      $to_pins $to_rf $corner $min_max $delay
    if { [llength $from_vertices] == 2 } {
      set_assigned_delay1 [lindex $from_vertices 1] \
	$to_pins $to_rf $corner $min_max $delay
    }
  }
}

proc set_assigned_delay1 { from_vertex to_pins to_rf corner min_max delay } {
  foreach to_pin $to_pins {
    set to_vertices [$to_pin vertices]
    set_assigned_delay2 $from_vertex [lindex $to_vertices 0] \
      $to_rf $corner $min_max $delay
    if { [llength $to_vertices] == 2 } {
      # Bidirect driver.
      set_assigned_delay2 $from_vertex [lindex $to_vertices 1] \
	$to_rf $corner $min_max $delay
    }
  }
}

proc set_assigned_delay2 {from_vertex to_vertex to_rf corner min_max delay} {
  set edge_iter [$from_vertex out_edge_iterator]
  while {[$edge_iter has_next]} {
    set edge [$edge_iter next]
    if { [$edge to] == $to_vertex \
	   && ![timing_role_is_check [$edge role]] } {
      set arc_iter [$edge timing_arc_iterator]
      while {[$arc_iter has_next]} {
	set arc [$arc_iter next]
	if { $to_rf == "rise_fall" \
	       || $to_rf eq [$arc to_trans_name] } {
	  set_arc_delay $edge $arc $corner $min_max $delay
	}
      }
      $arc_iter finish
    }
  }
  $edge_iter finish
}

################################################################

# -worst is ignored.
proc set_assigned_check_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-from -to -corner -clock -cond} \
    flags {-setup -hold -recovery -removal -rise -fall -max -min -worst}
  check_argc_eq1 $cmd $cmd_args

  if { [info exists keys(-from)] } {
    set from_pins [get_port_pins_error "from_pins" $keys(-from)]
  } else {
    sta_error "$cmd missing -from argument."
  }
  set from_rf "rise_fall"
  if { [info exists keys(-clock)] } {
    set clk_arg $keys(-clock)
    if { $clk_arg eq "rise" \
	   || $clk_arg eq "fall" } {
      set from_rf $clk_arg
    } else {
      sta_error "$cmd -clock must be rise or fall."
    }
  }

  if { [info exists keys(-to)] } {
    set to_pins [get_port_pins_error "to_pins" $keys(-to)]
  } else {
    sta_error "$cmd missing -to argument."
  }
  set to_rf [parse_rise_fall_flags flags]
  set corner [parse_corner keys]
  set min_max [parse_min_max_all_check_flags flags]

  if { [info exists flags(-setup)] } {
    set role "setup"
  } elseif { [info exists flags(-hold)] } {
    set role "hold"
  } elseif { [info exists flags(-recovery)] } {
    set role "recovery"
  } elseif { [info exists flags(-removal)] } {
    set role "removal"
  } else {
    sta_error "$cmd missing -setup|-hold|-recovery|-removal check type.."
  }
  set cond ""
  if { [info exists key(-cond)] } {
    set cond $key(-cond)
  }
  set check_value [lindex $cmd_args 0]
  if { ![string is double $check_value] } {
    sta_error "$cmd check_value is not a float."
  }
  set check_value [time_ui_sta $check_value]

  foreach from_pin $from_pins {
    set from_vertices [$from_pin vertices]
    set_assigned_check1 [lindex $from_vertices 0] $from_rf \
      $to_pins $to_rf $role $corner $min_max $cond $check_value
    if { [llength $from_vertices] == 2 } {
      set_assigned_check1 [lindex $from_vertices 1] $from_rf \
	$to_pins $to_rf $role $corner $min_max $cond $check_value
    }
  }
}

proc set_assigned_check1 { from_vertex from_rf to_pins to_rf \
			     role corner min_max cond check_value } {
  foreach to_pin $to_pins {
    set to_vertices [$to_pin vertices]
    set_assigned_check2 $from_vertex $from_rf [lindex $to_vertices 0] \
      $to_rf $role $corner $min_max $cond $check_value
    if { [llength $to_vertices] == 2 } {
      # Bidirect driver.
      set_assigned_check2 $from_vertex $from_rf \
	[lindex $to_vertices 1] $to_rf $role $corner $min_max \
	$cond $check_value
    }
  }
}

proc set_assigned_check2 { from_vertex from_rf to_vertex to_rf \
			     role corner min_max cond check_value } {
  set edge_iter [$from_vertex out_edge_iterator]
  while {[$edge_iter has_next]} {
    set edge [$edge_iter next]
    if { [$edge to] == $to_vertex } {
      set arc_iter [$edge timing_arc_iterator]
      while {[$arc_iter has_next]} {
	set arc [$arc_iter next]
	if { ($from_rf eq "rise_fall" \
		|| $from_rf eq [$arc from_trans_name]) \
	       && ($to_rf eq "rise_fall" \
		     || $to_rf eq [$arc to_trans_name]) \
	       && [$arc role] eq $role \
	       && ($cond eq "" || [$arc sdf_cond] eq $cond) } {
	  set_arc_delay $edge $arc $corner $min_max $check_value
	}
      }
      $arc_iter finish
    }
  }
  $edge_iter finish
}

################################################################

proc set_disable_inferred_clock_gating_cmd { objects } {
  parse_inst_port_pin_arg $objects insts pins
  foreach inst $insts {
    disable_clock_gating_check_inst $inst
  }
  foreach pin $pins {
    disable_clock_gating_check_pin $pin
  }
}

################################################################

proc unset_clk_groups_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-name} \
    flags {-logically_exclusive -physically_exclusive -asynchronous -all}

  set all [info exists flags(-all)]
  set names {}
  if {[info exists keys(-name)]} {
    set names $keys(-name)
  }

  if { $all && $names != {} } {
    sta_error "the -all and -name options are mutually exclusive."
  }
  if { !$all && $names == {} } {
    sta_error "either -all or -name options must be specified."
  }

  set logically_exclusive [info exists flags(-logically_exclusive)]
  set physically_exclusive [info exists flags(-physically_exclusive)]
  set asynchronous [info exists flags(-asynchronous)]

  if { ($logically_exclusive+$physically_exclusive+$asynchronous) == 0 } {
    sta_error "one of -logically_exclusive, -physically_exclusive or -asynchronous is required."
  }
  if { ($logically_exclusive+$physically_exclusive+$asynchronous) > 1 } {
    sta_error "the keywords -logically_exclusive, -physically_exclusive and -asynchronous are mutually exclusive."
  }

  if { $all } {
    if { $logically_exclusive } {
      unset_clock_groups_logically_exclusive "NULL"
    } elseif { $physically_exclusive } {
      unset_clock_groups_physically_exclusive "NULL"
    } elseif { $asynchronous } {
      unset_clock_groups_asynchronous "NULL"
    }
  } else {
    foreach name $names {
      if { $logically_exclusive } {
	unset_clock_groups_logically_exclusive $name
      } elseif { $physically_exclusive } {
	unset_clock_groups_physically_exclusive $name
      } elseif { $asynchronous } {
	unset_clock_groups_asynchronous $name
      }
    }
  }
}

################################################################

proc unset_clk_latency_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {-clock} flags {-source}
  check_argc_eq1 $cmd $cmd_args
  set objects [lindex $cmd_args 0]
  parse_clk_port_pin_arg $objects clks pins
  set pin_clk "NULL"
  if { [info exists keys(-clock)] } {
    set pin_clk [get_clock_warn "clock" $keys(-clock)]
    if { $clks != {} } {
      sta_warn "-clock ignored for clock objects."
    }
  }

  if {[info exists flags(-source)]} {
    # Source latency.
    foreach clk $clks {
      unset_clock_insertion_cmd $clk "NULL"
    }
    foreach pin $pins {
      # Source only allowed on clocks and clock pins.
      if { ![is_clock_pin $pin] } {
	sta_error "-source '[$pin path_name]' is not a clock pin."
      }
      unset_clock_insertion_cmd $pin_clk $pin
    }
  } else {
    # Latency.
    foreach clk $clks {
      unset_clock_latency_cmd $clk "NULL"
    }
    foreach pin $pins {
      unset_clock_latency_cmd $pin_clk $pin
    }
  }
}

################################################################

proc unset_clk_uncertainty_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to} \
    flags {-rise -fall -setup -hold}

  set min_max "min_max"
  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set min_max "max"
  }
  if { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set min_max "min"
  }

  if { [info exists keys(-from)] } {
    set from_key "-from"
    set from_rf "rise_fall"
  } elseif { [info exists keys(-rise_from)] } {
    set from_key "-rise_from"
    set from_rf "rise"
  } elseif { [info exists keys(-fall_from)] } {
    set from_key "-fall_from"
    set from_rf "fall"
  } else {
    set from_key "none"
  }

  if { [info exists keys(-to)] } {
    set to_key "-to"
    set to_rf "rise_fall"
  } elseif { [info exists keys(-rise_to)] } {
    set to_key "-rise_to"
    set to_rf "rise"
  } elseif { [info exists keys(-fall_to)] } {
    set to_key "-fall_to"
    set to_rf "fall"
  } else {
    set to_key "none"
  }

  if { $from_key != "none" && $to_key == "none" \
	 || $from_key == "none" && $to_key != "none" } {
    sta_error "-from/-to must be used together."
  } elseif { $from_key != "none" && $to_key != "none" } {
    # Inter-clock uncertainty.
    check_argc_eq0 "unset_clock_uncertainty" $cmd_args

    # -from/-to can be lists.
    set from_clks [get_clocks_warn "from_clocks" $keys($from_key)]
    set to_clks [get_clocks_warn "to_clocks" $keys($to_key)]

    foreach from_clk $from_clks {
      foreach to_clk $to_clks {
	unset_inter_clock_uncertainty $from_clk $from_rf \
	  $to_clk $to_rf $min_max
      }
    }
  } else {
    # Single clock uncertainty.
    check_argc_eq1 $cmd $cmd_args
    if { [info exists keys(-rise)] \
	   || [info exists keys(-fall)] } {
      sta_error "-rise, -fall options not allowed for single clock uncertainty."
    }
    set objects [lindex $cmd_args 0]
    parse_clk_port_pin_arg $objects clks pins

    foreach clk $clks {
      unset_clock_uncertainty_clk $clk $min_max
    }
    foreach pin $pins {
      unset_clock_uncertainty_pin $pin $min_max
    }
  }
}

################################################################

proc unset_data_checks_cmd { cmd cmd_args } {
  parse_key_args cmd cmd_args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to -clock} \
    flags {-setup -hold}
  check_argc_eq0 $cmd $cmd_args

  set from_rf "rise_fall"
  set to_rf "rise_fall"
  set clk "NULL"
  set setup_hold "max"
  if [info exists keys(-from)] {
    set from [get_port_pin_error "from_pin" $keys(-from)]
  } elseif [info exists keys(-rise_from)] {
    set from [get_port_pin_error "from_pin" $keys(-rise_from)]
    set from_rf "rise"
  } elseif [info exists keys(-fall_from)] {
    set from [get_port_pin_error "from_pin" $keys(-fall_from)]
    set from_rf "fall"
  } else {
    sta_error "missing -from, -rise_from or -fall_from argument."
  }

  if [info exists keys(-to)] {
    set to [get_port_pin_error "to_pin" $keys(-to)]
  } elseif [info exists keys(-rise_to)] {
    set to [get_port_pin_error "to_pin" $keys(-rise_to)]
    set to_rf "rise"
  } elseif [info exists keys(-fall_to)] {
    set to [get_port_pin_error "to_pin" $keys(-fall_to)]
    set to_rf "fall"
  } else {
    sta_error "missing -to, -rise_to or -fall_to argument."
  }

  if [info exists keys(-clock)] {
    set clk [get_clock_warn "clock" $keys(-clock)]
  }

  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set setup_hold "setup"
  } elseif { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set setup_hold "hold"
  } else {
    set setup_hold "setup_hold"
  }

  unset_data_check_cmd $from $from_rf $to $to_rf $clk $setup_hold
}

################################################################

proc unset_disable_inferred_clock_gating_cmd { objects } {
  parse_inst_port_pin_arg $objects insts pins
  foreach inst $insts {
    unset_disable_clock_gating_check_inst $inst
  }
  foreach pin $pins {
    unset_disable_clock_gating_check_pin $pin
  }
}

################################################################

proc remove_gclk_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {} flags {-all}
  if { [info exists flags(-all)] } {
    check_argc_eq0 $cmd $cmd_args
    set clks [all_clocks]
  } else {
    check_argc_eq1 $cmd $cmd_args
    set clks [get_clocks_warn "clocks" [lindex $cmd_args 0]]
  }
  foreach clk $clks {
    if { [$clk is_generated] } {
      remove_clock_cmd $clk
    }
  }
}

################################################################

proc unset_disable_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {-from -to} flags {}
  check_argc_eq1 $cmd $cmd_args

  set from ""
  if { [info exists keys(-from)] } {
    set from $keys(-from)
  }
  set to ""
  if { [info exists keys(-to)] } {
    set to $keys(-to)
  }
  parse_libcell_libport_inst_port_pin_edge_timing_arc_set_arg $cmd_args \
    libcells libports insts ports pins edges timing_arc_sets

  if { ([info exists keys(-from)] || [info exists keys(-to)]) \
	 && ($libports != {} || $pins != {} || $ports != {}) } {
    sta_warn "-from/-to keywords ignored for lib_pin, port and pin arguments."
  }

  foreach libcell $libcells {
    unset_disable_timing_cell $libcell $from $to
  }
  foreach libport $libports {
    unset_disable_lib_port $libport
  }
  foreach inst $insts {
    unset_disable_timing_instance $inst $from $to
  }
  foreach pin $pins {
    unset_disable_pin $pin
  }
  foreach port $ports {
    unset_disable_port $port
  }
  foreach edge $edges {
    unset_disable_edge $edge
  }
  foreach timing_arc_set $timing_arc_sets {
    unset_disable_timing_arc_set $timing_arc_set
  }
}

proc unset_disable_timing_cell { cell from to } {
  set from_ports [parse_disable_cell_ports $cell $from]
  set to_ports [parse_disable_cell_ports $cell $to]
  if { $from_ports == "NULL" && $to_ports == "NULL" } {
    unset_disable_cell $cell "NULL" "NULL"
  } elseif { $from_ports == "NULL" } {
    foreach to_port $to_ports {
      unset_disable_cell $cell "NULL" $to_port
    }
  } elseif { $to_ports == "NULL" } {
    foreach from_port $from_ports {
      unset_disable_cell $cell $from_port "NULL"
    }
  } else {
    foreach from_port $from_ports {
      foreach to_port $to_ports {
	unset_disable_cell $cell $from_port $to_port
      }
    }
  }
}

proc unset_disable_timing_instance { inst from to } {
  set from_ports [parse_disable_inst_ports $inst $from]
  set to_ports [parse_disable_inst_ports $inst $to]
  if { ![$inst is_leaf] } {
    sta_error "-from/-to hierarchical instance not supported."
  }
  if { $from_ports == "NULL" && $to_ports == "NULL" } {
    unset_disable_instance $inst "NULL" "NULL"
  } elseif { $from_ports == "NULL" } {
    foreach to_port $to_ports {
      unset_disable_instance $inst "NULL" $to_port
    }
  } elseif { $to_ports == "NULL" } {
    foreach from_port $from_ports {
      unset_disable_instance $inst $from_port "NULL"
    }
  } else {
    foreach from_port $from_ports {
      foreach to_port $to_ports {
	unset_disable_instance $inst $from_port $to_port
      }
    }
  }
}

################################################################

proc unset_path_exceptions_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to} \
    flags {-setup -hold -rise -fall} 0

  set min_max "min_max"
  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set min_max "max"
  }
  if { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set min_max "min"
  }

  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg cmd_args arg_error]
  set to [parse_to_arg keys flags arg_error]
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
    sta_error "$cmd command failed."
    return 0
  }

  check_for_key_args $cmd cmd_args
  if { $cmd_args != {} } {
    delete_from_thrus_to $from $thrus $to
    sta_error "positional arguments not supported."
  }
  if { ($from == "NULL" && $thrus == "" && $to == "NULL") } {
    delete_from_thrus_to $from $thrus $to
    sta_error "-from, -through or -to required."
  }

  reset_path_cmd $from $thrus $to $min_max
  delete_from_thrus_to $from $thrus $to
}

################################################################

proc unset_port_delay { cmd swig_cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-clock -reference_pin} \
    flags {-rise -fall -max -min -clock_fall }
  check_argc_eq2 $cmd $cmd_args
  
  set delay_arg [lindex $cmd_args 0]
  set delay [time_ui_sta $delay_arg]
  set pins [get_port_pins_error "pins" [lindex $cmd_args 1]]
  
  set clk "NULL"
  if [info exists keys(-clock)] {
    set clk [get_clock_warn "clock" $keys(-clock)]
  }
  
  if [info exists flags(-clock_fall)] {
    set clk_rf "fall"
  } else {
    set clk_rf "rise"
  }
  
  set tr [parse_rise_fall_flags flags]
  set min_max [parse_min_max_all_flags flags]

  foreach pin $pins {
    $swig_cmd $pin $tr $clk $clk_rf $min_max
  }
}

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
      get_object_args $obj clks libcells libports cells insts \
	ports pins nets edges timing_arc_sets
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
	sta_error "unsupported object type $object_type."
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
		      sta_warn "object '$obj' not found."
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

# If -corner keyword is missing:
#  one corner: return default
#  multiple corners: error
proc parse_corner { keys_var } {
  upvar 1 $keys_var keys

  if { [info exists keys(-corner)] } {
    set corner_name $keys(-corner)
    set corner [find_corner $corner_name]
    if { $corner == "NULL" } {
      sta_error "$corner_name is not the name of process corner."
    } else {
      return $corner
    }
  } elseif { [multi_corner] } {
    sta_error "-corner keyword required with multi-corner analysis."
  } else {
    return [cmd_corner]
  }
}

# -corner keyword is required.
# Assumes caller checks for existence of -corner keyword arg.
proc parse_corner_required { keys_var } {
  upvar 1 $keys_var keys

  if { [info exists keys(-corner)] } {
    set corner_name $keys(-corner)
    set corner [find_corner $corner_name]
    if { $corner == "NULL" } {
      sta_error "$corner_name is not the name of process corner."
    } else {
      return $corner
    }
  } else {
    sta_error "missing -corner arg."
  }
}

proc parse_corner_or_default { keys_var } {
  upvar 1 $keys_var keys

  if { [info exists keys(-corner)] } {
    set corner_name $keys(-corner)
    set corner [find_corner $corner_name]
    if { $corner == "NULL" } {
      sta_error "$corner_name is not the name of process corner."
    } else {
      return $corner
    }
  } else {
    return [cmd_corner]
  }
}

proc parse_corner_or_all { keys_var } {
  upvar 1 $keys_var keys

  if { [info exists keys(-corner)] } {
    set corner_name $keys(-corner)
    set corner [find_corner $corner_name]
    if { $corner == "NULL" } {
      sta_error "$corner_name is not the name of process corner."
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

proc parse_min_max_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-min)] && [info exists flags(-max)] } {
    sta_error "both -min and -max specified."
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
    sta_error "both -min and -max specified."
  } elseif { [info exists flags(-min)] && ![info exists flags(-max)] } {
    return "min"
  } elseif { [info exists flags(-max)] && ![info exists flags(-min)] } {
    return "max"
  } else {
    return "all"
  }
}

# parse_min_max_all_flags and require analysis type to be min/max.
proc parse_min_max_all_check_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-min)] && [info exists flags(-max)] } {
    return "all"
  } elseif { [info exists flags(-min)] && ![info exists flags(-max)] } {
    return "min"
  } elseif { [info exists flags(-max)] && ![info exists flags(-min)] } {
    return "max"
  } else {
    return "all"
  }
}

proc parse_early_late_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-early)] && [info exists flags(-late)] } {
    sta_error "only one of -early and -late can be specified."
  } elseif { [info exists flags(-early)] } {
    return "min"
  } elseif { [info exists flags(-late)] } {
    return "max"
  } else {
    sta_error "-early or -late must be specified."
  }
}

proc parse_early_late_all_flags { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-early)] && [info exists flags(-late)] } {
    sta_error "both -early and -late specified."
  } elseif { [info exists flags(-early)] && ![info exists flags(-late)] } {
    return "min"
  } elseif { [info exists flags(-late)] && ![info exists flags(-early)] } {
    return "max"
  } else {
    return "all"
  }
}

################################################################

proc get_liberty_error { arg_name arg } {
  set lib "NULL"
  if {[llength $arg] > 1} {
    sta_error "$arg_name must be a single library."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "LibertyLibrary" } {
      set lib $arg
    } else {
      sta_error "$arg_name type '$object_type' is not a library."
    }
  } else {
    set lib [find_liberty $arg]
    if { $lib == "NULL" } {
      sta_error "library '$arg' not found."
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
    sta_error "$arg_name must be a single lib cell."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "LibertyCell" } {
      set lib_cell $arg
    } else {
      $error_proc "$arg_name type '$object_type' is not a liberty cell."
    }
    # Parse library_name/cell_name.
  } elseif {[regexp [cell_regexp] $arg ignore lib_name cell_name]} {
    set library [find_liberty $lib_name]
    if { $library != "NULL" } {
      set lib_cell [$library find_liberty_cell $cell_name]
      if { $lib_cell == "NULL" } {
	$error_proc "liberty cell '$arg' not found."
      }
    } else {
      $error_proc "library '$lib_name' not found."
    }
  } else {
    set lib_cell [find_liberty_cell $arg]
    if { $lib_cell == "NULL" } {
      $error_proc "liberty cell '$arg' not found."
    }
  }
  return $lib_cell
}

proc get_instance_error { arg_name arg } {
  set inst "NULL"
  if {[llength $arg] > 1} {
    sta_error "$arg_name must be a single instance."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Instance" } {
      set inst $arg
    } else {
      sta_error "$arg_name type '$object_type' is not an instance."
    }
  } else {
    set inst [find_instance $arg]
    if { $inst == "NULL" } {
      sta_error "instance '$arg' not found."
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
	sta_error "$arg_name type '$object_type' is not an instance."
      }
    } elseif { $arg != {} } {
      set arg_insts [get_cells -quiet $arg]
      if { $arg_insts != {} } {
	set insts [concat $insts $arg_insts]
      } else {
	sta_error "instance '$arg' not found."
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
    sta_warn_error $warn_error "$arg_name must be a single port or pin."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Pin" } {
      set pin $arg
    } elseif { $object_type == "Port" } {
      # Explicit port arg - convert to pin.
      set pin [find_pin [get_name $arg]]
    } else {
      sta_warn_error $warn_error "$arg_name type '$object_type' is not a pin or port."
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
      sta_warn_error $warn_error "pin $arg not found."
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
	sta_error "$arg_name type '$object_type' is not a pin or port."
      }
    } elseif { $arg != {} } {
      set arg_pins [get_ports_or_pins $arg]
      if { $arg_pins != {} } {
	set pins [concat $pins $arg_pins]
      } else {
	sta_error "pin '$arg' not found."
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
	sta_error "$arg_name type '$object_type' is not a port."
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
    sta_warn_error $warn_error "$arg_name must be a single pin."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Pin" } {
      set pin $arg
    } else {
      sta_warn_error $warn_error "$arg_name type '$object_type' is not a pin."
    }
  } else {
    set pin [find_pin $arg]
    if { $pin == "NULL" } {
      sta_warn_error $warn_error "$arg_name pin $arg not found."
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
    $error_proc "$arg_name arg must be a single clock, not a list."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Clock" } {
      set clk $arg
    } else {
      $error_proc "$arg_name arg value is a $object_type, not a clock."
    }
  } elseif { $arg != {} } {
    set clk [find_clock $arg]
    if { $clk == "NULL" } {
      $error_proc "$arg_name arg '$arg' clock not found."
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
	sta_warn "unsupported object type $object_type."
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

proc get_net_warn { arg_name arg } {
  set net "NULL"
  if {[llength $arg] > 1} {
    sta_warn "$arg_name must be a single net."
  } elseif { [is_object $arg] } {
    set object_type [object_type $arg]
    if { $object_type == "Net" } {
      set net $arg
    } else {
      sta_warn "$arg_name '$object_type' is not a net."
    }
  } else {
    set net [find_net $arg]
    if { $net == "NULL" } {
      sta_warn "$arg_name '$arg' not found."
    }
  }
  return $net
}

proc get_nets_error { arg_name arglist } {
  return [get_nets_arg $arg_name $arglist "error"]
}

proc get_nets_warn { arg_name arglist } {
  return [get_nets_arg $arg_name $arglist "warn"]
}

proc get_nets_arg { arg_name arglist warn_error } {
  set nets {}
  # Copy backslashes that will be removed by foreach.
  set arglist [string map {\\ \\\\} $arglist]
  foreach arg $arglist {
    if {[llength $arg] > 1} {
      # Embedded list.
      set nets [concat $nets [get_nets_arg $arg_name $arg $warn_error]]
    } elseif { [is_object $arg] } {
      set object_type [object_type $arg]
      if { $object_type == "Net" } {
	lappend nets $arg
      } else {
	sta_warn_error $warn_error "unsupported object type $object_type."
      }
    } elseif { $arg != {} } {
      set arg_nets [get_nets -quiet $arg]
      if { $arg_nets != {} } {
	set nets [concat $nets $arg_nets]
      }
    }
  }
  return $nets
}

################################################################

proc cell_regexp {} {
  global hierarchy_separator
  if { $hierarchy_separator == "." } {
    set lib_regexp {[a-zA-Z0-9_]+}
  } else {
    set lib_regexp {[a-zA-Z0-9_\.]+}
  }
  set cell_regexp {[a-zA-Z0-9_]+}
  return "^(${lib_regexp})${hierarchy_separator}(${cell_regexp})$"
}

proc cell_wild_regexp { divider } {
  if { $divider == "." } {
    set lib_regexp {[a-zA-Z0-9_*+?^$\{\}]+}
  } else {
    set lib_regexp {[a-zA-Z0-9_.*+?^$\{\}]+}
  }
  set cell_wild_regexp {[a-zA-Z0-9_.*+?^$\{\}]+}
  return "^(${lib_regexp})${divider}(${cell_wild_regexp})$"
}

proc port_regexp {} {
  global hierarchy_separator
  if { $hierarchy_separator == "." } {
    set lib_regexp {[a-zA-Z0-9_]+}
  } else {
    set lib_regexp {[a-zA-Z0-9_\.]+}
  }
  set id_regexp {[a-zA-Z0-9_]+(?:\[[0-9]+\])?}
  return "^(${lib_regexp})${hierarchy_separator}(${id_regexp})${hierarchy_separator}(${id_regexp})$"
}

proc port_wild_regexp { divider } {
  if { $divider == "." } {
    set lib_regexp {[a-zA-Z0-9_]+}
  } else {
    set lib_regexp {[a-zA-Z0-9_\.]+}
  }
  set cell_regexp {[a-zA-Z0-9_]+}
  set wild_regexp {[a-zA-Z0-9_.*+?^$\{\}]+}
  return "^(${lib_regexp})${divider}(${wild_regexp})${divider}(${wild_regexp})$"
}

proc path_regexp {} {
  global hierarchy_separator
  set id_regexp {[a-zA-Z0-9_]+(?:\[[0-9]+\])?}
  set prefix_regexp "${id_regexp}(?:${hierarchy_separator}${id_regexp})*"
  return "^(${prefix_regexp})${hierarchy_separator}(${id_regexp})$"
}

proc get_property_cmd { cmd type_key cmd_args } {
  parse_key_args $cmd cmd_args keys $type_key flags {-quiet}
  set quiet [info exists flags(-quiet)]
  check_argc_eq2 $cmd $cmd_args
  set object [lindex $cmd_args 0]
  if { $object == "" } {
    sta_error "$cmd object is null."
  } elseif { ![is_object $object] } {
    if [info exists keys($type_key)] {
      set object_type $keys($type_key)
    } else {
      sta_error "$cmd $type_key must be specified with object name argument."
    }
    set object [get_property_object_type $object_type $object $quiet]
  }
  set prop [lindex $cmd_args 1]
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
    } elseif { $object_type == "PathRef" } {
      return [path_ref_property $object $prop]
    } elseif { $object_type == "TimingArcSet" } {
      return [timing_arc_set_property $object $prop]
    } else {
      sta_error "get_property unsupported object type $object_type."
    }
  } else {
    sta_error "get_property $object is not an object."
  }
}

proc get_property_object_type { object_type object_name quiet } {
  set object "NULL"
  if { $object_type == "cell" } {
    set object [get_cells -quiet $object_name]
  } elseif { $object_type == "pin" } {
    set object [get_pins -quiet $object_name]
  } elseif { $object_type == "net" } {
    set object [get_nets -quiet $object_name]
  } elseif { $object_type == "port" } {
    set object [get_ports -quiet $object_name]
  } elseif { $object_type == "clock" } {
    set object [get_clocks -quiet $object_name]
  } elseif { $object_type == "lib_cell" } {
    set object [get_lib_cells -quiet $object_name]
  } elseif { $object_type == "lib_pin" } {
    set object [get_lib_pins -quiet $object_name]
  } elseif { $object_type == "lib" } {
    set object [get_libs -quiet $object_name]
  } else {
    sta_error "$object_type not supported."
  }
  if { $object == "NULL" && !$quiet } {
    sta_error "$object_type '$object_name' not found."
  }
  return [lindex $object 0]
}

proc get_object_type { obj } {
  set object_type [object_type $obj]
  if { $object_type == "Clock" } {
    return "clock"
  } elseif { $object_type == "LibertyCell" } {
    return "libcell"
  } elseif { $object_type == "LibertyPort" } {
    return "libpin"
  } elseif { $object_type == "Cell" } {
    return "design"
  } elseif { $object_type == "Instance" } {
    return "cell"
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

################################################################

define_cmd_args "write_path_spice" { -path_args path_args\
				       -spice_directory spice_directory\
				       -lib_subckt_file lib_subckts_file\
				       -model_file model_file\
				       -power power\
				       -ground ground}

proc write_path_spice { args } {
  parse_key_args "write_path_spice" args \
    keys {-spice_directory -lib_subckt_file -model_file \
	    -power -ground -path_args} \
    flags {}

  if { [info exists keys(-spice_directory)] } {
    set spice_dir [file_expand_tilde $keys(-spice_directory)]
    if { ![file exists $spice_dir] } {
      sta_error "Directory $spice_dir not found.\n"
    }
    if { ![file isdirectory $spice_dir] } {
      sta_error "$spice_dir is not a directory.\n"
    }
    if { ![file writable $spice_dir] } {
      sta_error "Cannot write in $spice_dir.\n"
    }
  } else {
    sta_error "No -spice_directory specified.\n"
  }

  if { [info exists keys(-lib_subckt_file)] } {
    set lib_subckt_file [file_expand_tilde $keys(-lib_subckt_file)]
    if { ![file readable $lib_subckt_file] } {
      sta_error "-lib_subckt_file $lib_subckt_file is not readable.\n"
    }
  } else {
    sta_error "No -lib_subckt_file specified.\n"
  }

  if { [info exists keys(-model_file)] } {
    set model_file [file_expand_tilde $keys(-model_file)]
    if { ![file readable $model_file] } {
      sta_error "-model_file $model_file is not readable.\n"
    }
  } else {
    sta_error "No -model_file specified.\n"
  }

  if { [info exists keys(-power)] } {
    set power $keys(-power)
  } else {
    sta_error "No -power specified.\n"
  }

  if { [info exists keys(-ground)] } {
    set ground $keys(-ground)
  } else {
    sta_error "No -ground specified.\n"
  }

  if { ![info exists keys(-path_args)] } {
    sta_error "No -path_args specified.\n"
  }
  set path_args $keys(-path_args)
  set path_ends [eval [concat find_timing_paths $path_args]]
  if { $path_ends == {} } {
    sta_error "No paths found for -path_args $path_args.\n"
  } else {
    set path_index 1
    foreach path_end $path_ends {
      set path [$path_end path]
      set path_name "path_$path_index"
      set spice_file [file join $spice_dir "$path_name.sp"]
      set subckt_file [file join $spice_dir "$path_name.subckt"]
      write_path_spice_cmd $path $spice_file $subckt_file \
	$lib_subckt_file $model_file $power $ground
      incr path_index
    }
  }
}

proc file_expand_tilde { filename } {
  global env

  if { [string range $filename 0 1] == "~/" } {
    return [file join $env(HOME) [string range $filename 2 end]]
  } else {
    return $filename
  }
}

# sta namespace end.
}
