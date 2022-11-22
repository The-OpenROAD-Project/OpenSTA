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

namespace eval sta {

################################################################
#
# Non-SDC commands
#
################################################################

define_cmd_args "delete_clock" {[-all] clocks}

proc delete_clock { args } {
  parse_key_args "delete_clock" args keys {} flags {-all}
  if { [info exists flags(-all)] } {
    check_argc_eq0 "delete_clock" $args
    set clks [all_clocks]
  } else {
    check_argc_eq1 "delete_clock" $args
    set clks [get_clocks_warn "clocks" [lindex $args 0]]
  }
  foreach clk $clks {
    remove_clock_cmd $clk
  }
}

################################################################

define_cmd_args "delete_generated_clock" {[-all] clocks}

proc delete_generated_clock { args } {
  remove_gclk_cmd "delete_generated_clock" $args
}

################################################################

define_cmd_args "set_disable_inferred_clock_gating" { objects }

proc set_disable_inferred_clock_gating { objects } {
  set_disable_inferred_clock_gating_cmd $objects
}

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

define_cmd_args "unset_case_analysis" {pins}

proc unset_case_analysis { pins } {
  set pins1 [get_port_pins_error "pins" $pins]
  foreach pin $pins1 {
    unset_case_analysis_cmd $pin
  }
}

################################################################

define_cmd_args "unset_clock_groups" \
  {[-logically_exclusive] [-physically_exclusive]\
     [-asynchronous] [-name names] [-all]}
				
proc unset_clock_groups { args } {
  unset_clk_groups_cmd "unset_clock_groups" $args
}

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
    sta_error 454 "the -all and -name options are mutually exclusive."
  }
  if { !$all && $names == {} } {
    sta_error 455 "either -all or -name options must be specified."
  }

  set logically_exclusive [info exists flags(-logically_exclusive)]
  set physically_exclusive [info exists flags(-physically_exclusive)]
  set asynchronous [info exists flags(-asynchronous)]

  if { ($logically_exclusive+$physically_exclusive+$asynchronous) == 0 } {
    sta_error 456 "one of -logically_exclusive, -physically_exclusive or -asynchronous is required."
  }
  if { ($logically_exclusive+$physically_exclusive+$asynchronous) > 1 } {
    sta_error 457 "the keywords -logically_exclusive, -physically_exclusive and -asynchronous are mutually exclusive."
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

define_cmd_args "unset_clock_latency" {[-source] [-clock clock] objects}

proc unset_clock_latency { args } {
  unset_clk_latency_cmd "unset_clock_latency" $args
}

proc unset_clk_latency_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {-clock} flags {-source}
  check_argc_eq1 $cmd $cmd_args
  set objects [lindex $cmd_args 0]
  parse_clk_port_pin_arg $objects clks pins
  set pin_clk "NULL"
  if { [info exists keys(-clock)] } {
    set pin_clk [get_clock_warn "clock" $keys(-clock)]
    if { $clks != {} } {
      sta_warn 303 "-clock ignored for clock objects."
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
	sta_error 458 "-source '[$pin path_name]' is not a clock pin."
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

define_cmd_args "unset_clock_transition" {clocks}

proc unset_clock_transition { args } {
  check_argc_eq1 "unset_clock_transition" $args
  set clks [get_clocks_warn "clocks" [lindex $args 0]]
  foreach clk $clks {
    unset_clock_slew_cmd $clk
  }
}

################################################################

define_cmd_args "unset_clock_uncertainty" \
  {[-from|-rise_from|-fall_from from_clock]\
     [-to|-rise_to|-fall_to to_clock] [-rise] [-fall]\
     [-setup] [-hold] [objects]}

proc unset_clock_uncertainty { args } {
  unset_clk_uncertainty_cmd "unset_clock_uncertainty" $args
}

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
    sta_error 459 "-from/-to must be used together."
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
      sta_error 460 "-rise, -fall options not allowed for single clock uncertainty."
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

define_cmd_args "unset_data_check" \
  {[-from from_pin] [-rise_from from_pin] [-fall_from from_pin]\
     [-to to_pin] [-rise_to to_pin] [-fall_to to_pin]\
     [-setup | -hold] [-clock clock]}

proc unset_data_check { args } {
  unset_data_checks_cmd "unset_data_check" $args
}

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
    sta_error 461 "missing -from, -rise_from or -fall_from argument."
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
    sta_error 462 "missing -to, -rise_to or -fall_to argument."
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

define_cmd_args "unset_disable_inferred_clock_gating" { objects }

proc unset_disable_inferred_clock_gating { objects } {
  unset_disable_inferred_clock_gating_cmd $objects
}

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

define_cmd_args "unset_disable_timing" \
  {[-from from_port] [-to to_port] objects}

proc unset_disable_timing { args } {
  unset_disable_cmd "unset_disable_timing" $args
}

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
    sta_warn 304 "-from/-to keywords ignored for lib_pin, port and pin arguments."
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
    sta_error 463 "-from/-to hierarchical instance not supported."
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

define_cmd_args "unset_generated_clock" {[-all] clocks}

proc unset_generated_clock { args } {
  unset_gclk_cmd "unset_generated_clock" $args
}

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

define_cmd_args "unset_input_delay" \
  {[-rise] [-fall] [-max] [-min]\
     [-clock clock] [-clock_fall]\
     port_pin_list}

proc unset_input_delay { args } {
  unset_port_delay "unset_input_delay" "unset_input_delay_cmd" $args
}

################################################################

define_cmd_args "unset_output_delay" \
  {[-rise] [-fall] [-max] [-min]\
     [-clock clock] [-clock_fall]\
     port_pin_list}

proc unset_output_delay { args } {
  unset_port_delay "unset_output_delay" "unset_output_delay_cmd" $args
}

proc unset_port_delay { cmd swig_cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-clock -reference_pin} \
    flags {-rise -fall -max -min -clock_fall }
  check_argc_eq1 $cmd $cmd_args
  
  set pins [get_port_pins_error "pins" [lindex $cmd_args 0]]
  
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

################################################################

define_cmd_args "unset_path_exceptions" \
  {[-setup] [-hold] [-rise] [-fall] [-from from_list]\
     [-rise_from from_list] [-fall_from from_list]\
     [-through through_list] [-rise_through through_list]\
     [-fall_through through_list] [-to to_list] [-rise_to to_list]\
     [-fall_to to_list]}

proc unset_path_exceptions { args } {
  unset_path_exceptions_cmd "unset_path_exceptions" $args
}

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
    sta_error 464 "$cmd command failed."
    return 0
  }

  check_for_key_args $cmd cmd_args
  if { $cmd_args != {} } {
    delete_from_thrus_to $from $thrus $to
    sta_error 465 "positional arguments not supported."
  }
  if { ($from == "NULL" && $thrus == "" && $to == "NULL") } {
    delete_from_thrus_to $from $thrus $to
    sta_error 466 "-from, -through or -to required."
  }

  reset_path_cmd $from $thrus $to $min_max
  delete_from_thrus_to $from $thrus $to
}

################################################################

define_cmd_args "unset_propagated_clock" {objects}

proc unset_propagated_clock { objects } {
  parse_clk_port_pin_arg $objects clks pins
  foreach clk $clks {
    unset_propagated_clock_cmd $clk
  }
  foreach pin $pins {
    unset_propagated_clock_pin_cmd $pin
  }
}

################################################################

define_cmd_args "unset_timing_derate" {}

proc unset_timing_derate { args } {
  check_argc_eq0 "unset_timing_derate" $args
  unset_timing_derate_cmd
}

################################################################
#
# Network editing commands
#  
################################################################

define_cmd_args "connect_pin" {net pin}
# deprecated 2.0.16 05/02/2019
define_cmd_args "connect_pins" {net pins}

define_cmd_args "delete_instance" {inst}

define_cmd_args "delete_net" {net}

define_cmd_args "disconnect_pin" {net -all|pin}
# deprecated 2.0.16 05/02/2019
define_cmd_args "disconnect_pins" {net -all|pins}

define_cmd_args "make_instance" {inst_path lib_cell}

define_cmd_args "make_net" {}

define_cmd_args "replace_cell" {instance lib_cell}

define_cmd_args "insert_buffer" {buffer_name buffer_cell net load_pins\
				       buffer_out_net_name}

################################################################
#
# Delay calculation commands
#  
################################################################

define_cmd_args "set_assigned_delay" \
  {-cell|-net [-rise] [-fall] [-corner corner] [-min] [-max]\
     [-from from_pins] [-to to_pins] delay}

# Change the delay for timing arcs between from_pins and to_pins matching
# on cell (instance) or net.
proc set_assigned_delay { args } {
  set_assigned_delay_cmd "set_assigned_delay" $args
}

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
    sta_error 442 "$cmd missing -from argument."
  }
  if [info exists keys(-to)] {
    set to_pins [get_port_pins_error "to_pins" $keys(-to)]
  } else {
    sta_error 443 "$cmd missing -to argument."
  }

  set delay [lindex $cmd_args 0]
  if {![string is double $delay]} {
    sta_error 444 "$cmd delay is not a float."
  }
  set delay [time_ui_sta $delay]

  if {[info exists flags(-cell)] && [info exists flags(-net)]} {
    sta_error 445 "set_annotated_delay -cell and -net options are mutually excluive."
  } elseif {[info exists flags(-cell)]} {
    if { $from_pins != {} } {
      set inst [[lindex $from_pins 0] instance]
      foreach pin $from_pins {
	if {[$pin instance] != $inst} {
	  sta_error 446 "$cmd pin [get_full_name $pin] is not attached to instance [get_full_name $inst]."
	}
      }
      foreach pin $to_pins {
	if {[$pin instance] != $inst} {
	  sta_error 447 "$cmd pin [get_full_name $pin] is not attached to instance [get_full_name $inst]"
	}
      }
    }
  } elseif {![info exists flags(-net)]} {
    sta_error 448 "$cmd -cell or -net required."
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
      foreach arc [$edge timing_arcs] {
	if { $to_rf == "rise_fall" \
	       || $to_rf eq [$arc to_edge_name] } {
	  set_arc_delay $edge $arc $corner $min_max $delay
	}
      }
    }
  }
  $edge_iter finish
}

################################################################

define_cmd_args "set_assigned_check" \
  {-setup|-hold|-recovery|-removal [-rise] [-fall]\
     [-corner corner] [-min] [-max]\
     [-from from_pins] [-to to_pins] [-clock rise|fall]\
     [-cond sdf_cond] check_value}

proc set_assigned_check { args } {
  set_assigned_check_cmd "set_assigned_check" $args
}

proc set_assigned_check_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-from -to -corner -clock -cond} \
    flags {-setup -hold -recovery -removal -rise -fall -max -min}
  check_argc_eq1 $cmd $cmd_args

  if { [info exists keys(-from)] } {
    set from_pins [get_port_pins_error "from_pins" $keys(-from)]
  } else {
    sta_error 449 "$cmd missing -from argument."
  }
  set from_rf "rise_fall"
  if { [info exists keys(-clock)] } {
    set clk_arg $keys(-clock)
    if { $clk_arg eq "rise" \
	   || $clk_arg eq "fall" } {
      set from_rf $clk_arg
    } else {
      sta_error 450 "$cmd -clock must be rise or fall."
    }
  }

  if { [info exists keys(-to)] } {
    set to_pins [get_port_pins_error "to_pins" $keys(-to)]
  } else {
    sta_error 451 "$cmd missing -to argument."
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
    sta_error 452 "$cmd missing -setup|-hold|-recovery|-removal check type.."
  }
  set cond ""
  if { [info exists key(-cond)] } {
    set cond $key(-cond)
  }
  set check_value [lindex $cmd_args 0]
  if { ![string is double $check_value] } {
    sta_error 453 "$cmd check_value is not a float."
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
      foreach arc [$edge timing_arcs] {
	if { ($from_rf eq "rise_fall" \
		|| $from_rf eq [$arc from_edge_name]) \
	       && ($to_rf eq "rise_fall" \
		     || $to_rf eq [$arc to_edge_name]) \
	       && [$arc role] eq $role \
	       && ($cond eq "" || [$arc sdf_cond] eq $cond) } {
	  set_arc_delay $edge $arc $corner $min_max $check_value
	}
      }
    }
  }
  $edge_iter finish
}

################################################################a

define_cmd_args "set_assigned_transition" \
  {[-rise] [-fall] [-corner corner] [-min] [-max] slew pins}

# Change the slew on a list of ports.
proc set_assigned_transition { args } {
  parse_key_args "set_assigned_transition" args keys {-corner} \
    flags {-rise -fall -max -min}

  set corner [parse_corner keys]
  set min_max [parse_min_max_all_check_flags flags]
  set tr [parse_rise_fall_flags flags]
  check_argc_eq2 "set_assigned_transition" $args

  set slew [lindex $args 0]
  if {![string is double $slew]} {
    sta_error 428 "set_assigned_transition transition is not a float."
  }
  set slew [time_ui_sta $slew]
  set pins [get_port_pins_error "pins" [lindex $args 1]]
  foreach pin $pins {
    set vertices [$pin vertices]
    set vertex [lindex $vertices 0]
    set_annotated_slew $vertex $corner $min_max $tr $slew
    if { [llength $vertices] == 2 } {
      # Bidirect driver.
      set vertex [lindex $vertices 1]
      set_annotated_slew $vertex $min_max $tr $slew
    }
  }
}

################################################################a

# compatibility
define_cmd_args "read_parasitics" \
  {[-min]\
     [-max]\
     [-elmore]\
     [-path path]\
     [-increment]\
     [-pin_cap_included]\
     [-keep_capacitive_coupling]\
     [-coupling_reduction_factor factor]\
     [-reduce_to pi_elmore|pi_pole_residue2]\
     [-delete_after_reduce]\
     [-quiet]\
     [-save]\
     filename}

################################################################
#  
# Utility commands
#
################################################################

define_cmd_args "delete_from_list" {list objs}

proc delete_from_list { list objects } {
  delete_objects_from_list_cmd $list $objects
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
	sta_error 439 "unsupported object type $list_type."
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

define_cmd_args "get_name" {objects}
define_cmd_args "get_full_name" {objects}

################################################################

define_cmd_args "get_property" \
  {[-object_type cell|pin|net|port|clock|timing_arc] object property}

proc get_property { args } {
  return [get_property_cmd "get_property" "-object_type" $args]
}

################################################################

proc get_property_cmd { cmd type_key cmd_args } {
  parse_key_args $cmd cmd_args keys $type_key flags {-quiet}
  set quiet [info exists flags(-quiet)]
  check_argc_eq2 $cmd $cmd_args
  set object [lindex $cmd_args 0]
  if { $object == "" } {
    sta_error 491 "$cmd object is null."
  } elseif { ![is_object $object] } {
    if [info exists keys($type_key)] {
      set object_type $keys($type_key)
    } else {
      sta_error 492 "$cmd $type_key must be specified with object name argument."
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
      sta_error 606 "get_property unsupported object type $object_type."
    }
  } else {
    sta_error 493 "get_property $object is not an object."
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
    sta_error 494 "$object_type not supported."
  }
  if { $object == "NULL" && !$quiet } {
    sta_error 495 "$object_type '$object_name' not found."
  }
  return [lindex $object 0]
}

proc get_object_type { obj } {
  set object_type [object_type $obj]
  if { $object_type == "Clock" } {
    return "clock"
  } elseif { $object_type == "LibertyCell" } {
    return "lib_cell"
  } elseif { $object_type == "LibertyPort" } {
    return "lib_pin"
  } elseif { $object_type == "Cell" } {
    return "cell"
  } elseif { $object_type == "Instance" } {
    return "instance"
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
      sta_error 440 "-from/-to arguments not supported with -of_objects."
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
    sta_error 441 "unsupported -filter expression."
  }
  return $filtered_objects
}

################################################################

define_cmd_args "report_clock_properties" {[clocks]}

proc_redirect report_clock_properties {
  check_argc_eq0or1 "report_clock_properties" $args
  update_generated_clks
  report_line "Clock                   Period          Waveform"
  report_line "----------------------------------------------------"
  if { [llength $args] == 0 } {
    set clk_iter [clock_iterator]
    while {[$clk_iter has_next]} {
      set clk [$clk_iter next]
      report_clock1 $clk
    }
    $clk_iter finish
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

################################################################

define_cmd_args "report_object_full_names" {objects}

proc report_object_full_names { objects } {
  foreach obj [sort_by_full_name $objects] {
    report_line [get_full_name $obj]
  }
}

define_cmd_args "report_object_names" {objects}

proc report_object_names { objects } {
  foreach obj [sort_by_name $objects] {
    report_line [get_name $obj]
  }
}

################################################################

define_cmd_args "report_units" {}

proc report_units { args } {
  check_argc_eq0 "report_units" $args
  foreach unit {"time" "capacitance" "resistance" "voltage" "current" "power" "distance"} {
    report_line " $unit 1[unit_scale_abreviation $unit][unit_suffix $unit]"
  }
}

################################################################

define_cmd_args "with_output_to_variable" { var { cmds }}

# with_output_to_variable variable { command args... }
proc with_output_to_variable { var_name args } {
  upvar 1 $var_name var

  set body [lindex $args 0]
  sta::redirect_string_begin;
  catch $body ret
  set var [sta::redirect_string_end]
  return $ret
}

define_cmd_args "set_pocv_sigma_factor" { factor }

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
    set spice_dir [file nativename $keys(-spice_directory)]
    if { ![file exists $spice_dir] } {
      sta_error 496 "Directory $spice_dir not found."
    }
    if { ![file isdirectory $spice_dir] } {
      sta_error 497 "$spice_dir is not a directory."
    }
    if { ![file writable $spice_dir] } {
      sta_error 498 "Cannot write in $spice_dir."
    }
  } else {
    sta_error 499 "No -spice_directory specified."
  }

  if { [info exists keys(-lib_subckt_file)] } {
    set lib_subckt_file [file nativename $keys(-lib_subckt_file)]
    if { ![file readable $lib_subckt_file] } {
      sta_error 500 "-lib_subckt_file $lib_subckt_file is not readable."
    }
  } else {
    sta_error 501 "No -lib_subckt_file specified."
  }

  if { [info exists keys(-model_file)] } {
    set model_file [file nativename $keys(-model_file)]
    if { ![file readable $model_file] } {
      sta_error 502 "-model_file $model_file is not readable."
    }
  } else {
    sta_error 503 "No -model_file specified."
  }

  if { [info exists keys(-power)] } {
    set power $keys(-power)
  } else {
    sta_error 504 "No -power specified."
  }

  if { [info exists keys(-ground)] } {
    set ground $keys(-ground)
  } else {
    sta_error 505 "No -ground specified."
  }

  if { ![info exists keys(-path_args)] } {
    sta_error 506 "No -path_args specified."
  }
  set path_args $keys(-path_args)
  set path_ends [eval [concat find_timing_paths $path_args]]
  if { $path_ends == {} } {
    sta_error 507 "No paths found for -path_args $path_args."
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

# sta namespace end.
}
