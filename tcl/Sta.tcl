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

namespace eval sta {

proc define_sta_cmd_args { cmd arglist } {
  variable sta_cmd_args

  set sta_cmd_args($cmd) $arglist
}

# Import Sta commands to global namespace.
proc define_sta_cmds {} {
  variable sta_cmd_args
  variable native

  foreach cmd [array names sta_cmd_args] {
    define_cmd_args $cmd $sta_cmd_args($cmd)
  }
  define_report_path_fields
  set native 1
}

proc define_report_path_fields {} {
  variable report_path_field_width_extra

  set_rise_fall_short_names "^" "v"
  set_report_path_field_order { fanout capacitance slew \
				 incr total edge case description }
  set_report_path_field_properties "description" "Description" 36 1
  set width $report_path_field_width_extra
  set_report_path_field_properties "total" "Time" $width 0
  set_report_path_field_properties "incr" "Delay" $width 0
  set_report_path_field_properties "capacitance" "Cap" $width 0
  set_report_path_field_properties "slew" "Slew" $width 0
  set_report_path_field_properties "fanout" "Fanout" 5 0
  set_report_path_field_properties "edge" " " 1 0
  set_report_path_field_properties "case" " " 11 0
}

################################################################
#
# Search commands
#  
################################################################

define_sta_cmd_args "check_setup" \
  { [-verbose] [-no_input_delay] [-no_output_delay]\
      [-multiple_clock] [-no_clock]\
      [-unconstrained_endpoints] [-loops] [-generated_clocks]\
      [> filename] [>> filename] }

proc_redirect check_setup {
  check_setup_cmd "check_setup" $args
}

################################################################

define_sta_cmd_args "delete_clock" {[-all] clocks}

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

define_sta_cmd_args "delete_generated_clock" {[-all] clocks}

proc delete_generated_clock { args } {
  remove_gclk_cmd "delete_generated_clock" $args
}

################################################################

define_sta_cmd_args "find_timing" {[-full_update]}

proc find_timing { args } {
  parse_key_args "find_timing" args keys {} flags {-full_update}
  find_timing_cmd [info exists flags(-full_update)]
}

################################################################

define_sta_cmd_args "report_clock_skew" {[-setup|-hold]\
					   [-clock clocks]\
					   [-corner corner_name]]\
					   [-digits digits]}

proc_redirect report_clock_skew {
  global sta_report_default_digits

  parse_key_args "report_clock_skew" args \
    keys {-clock -corner -digits} flags {-setup -hold}
  check_argc_eq0 "report_clock_skew" $args

  if { [info exists flags(-setup)] && [info exists flags(-hold)] } {
    sta_error "report_clock_skew -setup and -hold are mutually exclusive options."
  } elseif { [info exists flags(-setup)] } {
    set setup_hold "setup"
  } elseif { [info exists flags(-hold)] } {
    set setup_hold "hold"
  } else {
    set setup_hold "setup"
  }

  if [info exists keys(-clock)] {
    set clks [get_clocks_warn "-clocks" $keys(-clock)]
  } else {
    set clks [all_clocks]
  }
  set corner [parse_corner_or_all keys]
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  report_clk_skew $clks $corner $setup_hold $digits
}

################################################################

define_sta_cmd_args "find_timing_paths" \
  {[-from from_list|-rise_from from_list|-fall_from from_list]\
     [-through through_list|-rise_through through_list|-fall_through through_list]\
     [-to to_list|-rise_to to_list|-fall_to to_list]\
     [-path_delay min|min_rise|min_fall|max|max_rise|max_fall|min_max]\
     [-unconstrained]
     [-corner corner_name]\
     [-group_count path_count] \
     [-endpoint_count path_count]\
     [-unique_paths_to_endpoint]\
     [-slack_max slack_max]\
     [-slack_min slack_min]\
     [-sort_by_slack]\
     [-path_group group_name]}

proc find_timing_paths { args } {
  set path_ends [find_timing_paths_cmd "find_timing_paths" args]
  return $path_ends
}

proc find_timing_paths_cmd { cmd args_var } {
  global sta_report_unconstrained_paths
  upvar 1 $args_var args

  parse_key_args $cmd args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to \
	    -path_delay -corner -group_count -endpoint_count \
	    -slack_max -slack_min -path_group} \
    flags {-unconstrained -sort_by_slack -unique_paths_to_endpoint} 0

  set min_max "max"
  set end_rf "rise_fall"
  if [info exists keys(-path_delay)] {
    set mm_key $keys(-path_delay)
    if { $mm_key == "max_rise" } {
      set min_max "max"
      set end_rf "rise"
    } elseif { $mm_key == "max_fall" } {
      set min_max "max"
      set end_rf "fall"
    } elseif { $mm_key == "min_rise" } {
      set min_max "min"
      set end_rf "rise"
    } elseif { $mm_key == "min_fall" } {
      set min_max "min"
      set end_rf "fall"
    } elseif { $mm_key == "min" || $mm_key == "max" || $mm_key == "min_max" } {
      set min_max $mm_key
    } else {
      sta_error "$cmd -path_delay must be min, min_rise, min_fall, max, max_rise, max_fall or min_max."
    }
  }

  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg args arg_error]
  set to [parse_to_arg1 keys $end_rf arg_error]
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
    sta_error "$cmd command failed."
  }

  check_for_key_args $cmd args

  if { [info exists flags(-unconstrained)] } {
    set unconstrained 1
  } elseif { [info exists sta_report_unconstrained_paths] } {
    set unconstrained $sta_report_unconstrained_paths
  } else {
    set unconstrained 0
  }

  set corner [parse_corner_or_all keys]

  set endpoint_count 1
  if [info exists keys(-endpoint_count)] {
    set endpoint_count $keys(-endpoint_count)
    if { $endpoint_count < 1 } {
      sta_error "-endpoint_count must be a positive integer."
    }
  }

  set group_count $endpoint_count
  if [info exists keys(-group_count)] {
    set group_count $keys(-group_count)
    if { $group_count < 1 } {
      sta_error "-group_count must be a positive integer."
    }
  }

  set unique_pins [info exists flags(-unique_paths_to_endpoint)]

  set slack_min "-1e+30"
  if [info exist keys(-slack_min)] {
    set slack_min $keys(-slack_min)
    check_float "-slack_min" $slack_min
    set slack_min [time_ui_sta $slack_min]
  }

  set slack_max "1e+30"
  if [info exist keys(-slack_max)] {
    set slack_max $keys(-slack_max)
    check_float "-slack_max" $slack_max
    set slack_max [time_ui_sta $slack_max]
  }

  set sort_by_slack [info exists flags(-sort_by_slack)]

  set groups {}
  if [info exists keys(-path_group)] {
    set groups [parse_path_group_arg $keys(-path_group)]
  }

  if { [llength $args] != 0 } {
    delete_from_thrus_to $from $thrus $to
    set arg [lindex $args 0]
    if { [is_keyword_arg $arg] } {
      sta_error "'$arg' is not a known keyword or flag."
    } else {
      sta_error "positional arguments not supported."
    }
  }

  set path_ends [find_path_ends $from $thrus $to $unconstrained \
		   $corner $min_max \
		   $group_count $endpoint_count $unique_pins \
		   $slack_min $slack_max \
		   $sort_by_slack $groups \
		   1 1 1 1 1 1]
  return $path_ends
}

################################################################

define_sta_cmd_args "report_checks" \
  {[-from from_list|-rise_from from_list|-fall_from from_list]\
     [-through through_list|-rise_through through_list|-fall_through through_list]\
     [-to to_list|-rise_to to_list|-fall_to to_list]\
     [-unconstrained]\
     [-path_delay min|min_rise|min_fall|max|max_rise|max_fall|min_max]\
     [-corner corner_name]\
     [-group_count path_count] \
     [-endpoint_count path_count]\
     [-unique_paths_to_endpoint]\
     [-slack_max slack_max]\
     [-slack_min slack_min]\
     [-sort_by_slack]\
     [-path_group group_name]\
     [-format full|full_clock|full_clock_expanded|short|end|summary]\
     [-fields [capacitance|slew|input_pin|net]]\
     [-digits digits]\
     [-no_line_splits]\
     [> filename] [>> filename]}

proc_redirect report_checks {
  global sta_report_unconstrained_paths

  parse_report_path_options "report_checks" args "full" 0
  set path_ends [find_timing_paths_cmd "report_checks" args]
  if { $path_ends == {} } {
    puts "No paths found."
  } else {
    report_path_ends $path_ends
  }
}

################################################################

define_sta_cmd_args "report_check_types" \
  {[-all_violators] [-verbose]\
     [-corner corner_name]\
     [-format slack_only|end]\
     [-max_delay] [-min_delay]\
     [-recovery] [-removal]\
     [-clock_gating_setup] [-clock_gating_hold]\
     [-max_transition] [-min_transition]\
     [-min_pulse_width] [-min_period] [-max_skew]\
     [-digits digits] [-no_line_splits]\
     [> filename] [>> filename]}

proc_redirect report_check_types {
  variable path_options

  parse_key_args "report_check_types" args keys {-corner}\
    flags {-all_violators -verbose -no_line_splits} 0

  set all_violators [info exists flags(-all_violators)]
  set verbose [info exists flags(-verbose)]
  set nosplit [info exists flags(-no_line_splits)]

  if { $verbose } {
    set default_format "full"
  } else {
    set default_format "slack_only"
  }
  parse_report_path_options "report_check_types" args $default_format 0

  set min_max "min_max"
  if { [operating_condition_analysis_type] == "single" } {
    set min_max "max"
  }

  set corner [parse_corner_or_all keys]

  if { $args == {} } {
    if { $min_max == "max" || $min_max == "min_max" } {
      set setup 1
      set recovery 1
      set clk_gating_setup 1
      set max_transition 1
    } else {
      set setup 0
      set recovery 0
      set clk_gating_setup 0
      set max_transition 0
    }
    if { $min_max == "min" || $min_max == "min_max" } {
      set hold 1
      set removal 1
      set clk_gating_hold 1
      set min_transition 1
    } else {
      set hold 0
      set min_delay 0
      set removal 0
      set clk_gating_hold 0
      set min_transition 0
    }
    set min_pulse_width 1
    set min_period 1
    set max_skew 1
  } else {
    parse_key_args "report_check_types" args keys {}\
      flags {-max_delay -min_delay -recovery -removal\
	       -clock_gating_setup -clock_gating_hold\
	       -max_transition -min_transition -min_pulse_width\
	       -min_period -max_skew} 1

    set setup [info exists flags(-max_delay)]
    set hold [info exists flags(-min_delay)]
    set recovery [info exists flags(-recovery)]
    set removal [info exists flags(-removal)]
    set clk_gating_setup [info exists flags(-clock_gating_setup)]
    set clk_gating_hold [info exists flags(-clock_gating_hold)]
    set max_transition [info exists flags(-max_transition)]
    set min_transition [info exists flags(-min_transition)]
    set min_pulse_width [info exists flags(-min_pulse_width)]
    set min_period [info exists flags(-min_period)]
    set max_skew [info exists flags(-max_skew)]
    if { [operating_condition_analysis_type] == "single" \
	   && (($setup && $hold) \
		 || ($recovery && $removal) \
		 || ($clk_gating_setup && $clk_gating_hold)) } {
      sta_error "analysis type single is not consistent with doing both setup/max and hold/min checks."
    }
  }

  if { $args != {} } {
    sta_error "positional arguments not supported."
  }

  set corner [parse_corner_or_all keys]

  if { $setup || $hold || $recovery || $removal \
	 || $clk_gating_setup || $clk_gating_hold } {
    if { ($setup && $hold) \
	   || ($recovery && $removal) \
	   || ($clk_gating_setup && $clk_gating_hold) } {
      set path_min_max "min_max"
    } elseif { $setup || $recovery || $clk_gating_setup } {
      set path_min_max "max"
    } elseif { $hold || $removal || $clk_gating_hold } {
      set path_min_max "min"
    }
    if { $all_violators } {
      set group_count $sta::group_count_max
      set slack_min [expr -$sta::float_inf]
      set slack_max 0.0
    } else {
      set group_count 1
      set slack_min [expr -$sta::float_inf]
      set slack_max $sta::float_inf
    }
    set path_ends [find_path_ends "NULL" {} "NULL" 0 \
		     $corner $path_min_max $group_count 1 0 \
		     $slack_min $slack_max \
		     0 {} \
		     $setup $hold \
		     $recovery $removal \
		     $clk_gating_setup $clk_gating_hold]
    report_path_ends $path_ends
  }

  if { $max_transition } {
    report_slew_limits $corner "max" $all_violators $verbose $nosplit
  }
  if { $min_transition } {
    report_slew_limits $corner "min" $all_violators $verbose $nosplit
  }
  if { $min_pulse_width } {
    if { $all_violators } {
      set checks [min_pulse_width_violations $corner]
      report_mpw_checks $checks $verbose
    } else {
      set check [min_pulse_width_check_slack $corner]
      if { $check != "NULL" } {
	report_mpw_check $check $verbose
      }
    }
  }
  if { $min_period } {
    if { $all_violators } {
      set checks [min_period_violations]
      report_min_period_checks $checks $verbose
    } else {
      set check [min_period_check_slack]
      if { $check != "NULL" } {
	report_min_period_check $check $verbose
      }
    }
  }
  if { $max_skew } {
    if { $all_violators } {
      set checks [max_skew_violations]
      report_max_skew_checks $checks $verbose
    } else {
      set check [max_skew_check_slack]
      if { $check != "NULL" } {
	report_max_skew_check $check $verbose
      }
    }
  }
}

################################################################

define_sta_cmd_args "report_tns" { [-digits digits]}

proc_redirect report_tns {
  global sta_report_default_digits

  parse_key_args "report_tns" args keys {-digits} flags {}
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }

  puts "tns [format_time [total_negative_slack_cmd "max"] $digits]"
}

################################################################

define_sta_cmd_args "report_wns" { [-digits digits]}

proc_redirect report_wns {
  global sta_report_default_digits

  parse_key_args "report_wns" args keys {-digits} flags {}
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }

  set slack [worst_slack_cmd "max"]
  if { $slack > 0.0 } {
    set slack 0.0
  }
  puts "wns [format_time $slack $digits]"
}

################################################################

define_sta_cmd_args "report_worst_slack" { [-digits digits]}

proc_redirect report_worst_slack {
  global sta_report_default_digits

  parse_key_args "report_worst_slack" args keys {-digits} flags {}
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }

  puts "worst slack [format_time [worst_slack_cmd "max"] $digits]"
}

################################################################

define_sta_cmd_args "report_dcalc" \
  {[-from from_pin] [-to to_pin] [-corner corner_name] [-min] [-max] [-digits digits]}

proc_redirect report_dcalc {
  report_dcalc_cmd "report_dcalc" $args "-digits"
}

################################################################

define_sta_cmd_args "report_disabled_edges" {}

################################################################

define_sta_cmd_args "report_pulse_width_checks" \
  {[-verbose] [-corner corner_name] [-digits digits] [-no_line_splits] [pins]\
     [> filename] [>> filename]}

proc_redirect report_pulse_width_checks {
  variable path_options

  parse_key_args "report_pulse_width_checks" args keys {-corner} \
    flags {-verbose} 0
  # Only -digits and -no_line_splits are respected.
  parse_report_path_options "report_pulse_width_checks" args "full" 0
  check_argc_eq0or1 "report_pulse_width_checks" $args
  set corner [parse_corner_or_all keys]
  set verbose [info exists flags(-verbose)]
  if { [llength $args] == 1 } {
    set pins [get_port_pins_error "pins" [lindex $args 0]]
    set checks [min_pulse_width_check_pins $pins $corner]
  } else {
    set checks [min_pulse_width_checks $corner]
  }
  if { $checks != {} } {
    report_mpw_checks $checks $verbose
  }
}

################################################################

define_sta_cmd_args "set_disable_inferred_clock_gating" { objects }

proc set_disable_inferred_clock_gating { objects } {
  set_disable_inferred_clock_gating_cmd $objects
}

################################################################

define_sta_cmd_args "unset_case_analysis" {pins}

proc unset_case_analysis { pins } {
  set pins1 [get_port_pins_error "pins" $pins]
  foreach pin $pins1 {
    unset_case_analysis_cmd $pin
  }
}

################################################################

define_sta_cmd_args "unset_clock_groups" \
  {[-logically_exclusive] [-physically_exclusive]\
     [-asynchronous] [-name names] [-all]}
				
proc unset_clock_groups { args } {
  unset_clk_groups_cmd "unset_clock_groups" $args
}

################################################################

define_sta_cmd_args "unset_clock_latency" {[-source] [-clock clock] objects}

proc unset_clock_latency { args } {
  unset_clk_latency_cmd "unset_clock_latency" $args
}

################################################################

define_sta_cmd_args "unset_clock_transition" {clocks}

proc unset_clock_transition { args } {
  check_argc_eq1 "unset_clock_transition" $args
  set clks [get_clocks_warn "clocks" [lindex $args 0]]
  foreach clk $clks {
    unset_clock_slew_cmd $clk
  }
}

################################################################

define_sta_cmd_args "unset_clock_uncertainty" \
  {[-from|-rise_from|-fall_from from_clock]\
     [-to|-rise_to|-fall_to to_clock] [-rise] [-fall]\
     [-setup] [-hold] [objects]}

proc unset_clock_uncertainty { args } {
  unset_clk_uncertainty_cmd "unset_clock_uncertainty" $args
}

################################################################

define_sta_cmd_args "unset_data_check" \
  {[-from from_pin] [-rise_from from_pin] [-fall_from from_pin]\
     [-to to_pin] [-rise_to to_pin] [-fall_to to_pin]\
     [-setup | -hold] [-clock clock]}

proc unset_data_check { args } {
  unset_data_checks_cmd "unset_data_check" $args
}

################################################################

define_sta_cmd_args "unset_disable_inferred_clock_gating" { objects }

proc unset_disable_inferred_clock_gating { objects } {
  unset_disable_inferred_clock_gating_cmd $objects
}

################################################################

define_sta_cmd_args "unset_disable_timing" \
  {[-from from_port] [-to to_port] objects}

proc unset_disable_timing { args } {
  unset_disable_cmd "unset_disable_timing" $args
}

################################################################

define_sta_cmd_args "unset_generated_clock" {[-all] clocks}

proc unset_generated_clock { args } {
  unset_gclk_cmd "unset_generated_clock" $args
}

################################################################

define_sta_cmd_args "unset_input_delay" \
  {[-rise] [-fall] [-max] [-min]\
     [-clock clock] [-clock_fall]\
     port_pin_list}

proc unset_input_delay { args } {
  unset_port_delay "unset_input_delay" "unset_input_delay_cmd" $args
}

################################################################

define_sta_cmd_args "unset_output_delay" \
  {[-rise] [-fall] [-max] [-min]\
     [-clock clock] [-clock_fall]\
     port_pin_list}

proc unset_output_delay { args } {
  unset_port_delay "unset_output_delay" "unset_output_delay_cmd" $args
}

################################################################

define_sta_cmd_args "unset_path_exceptions" \
  {[-setup] [-hold] [-rise] [-fall] [-from from_list]\
     [-rise_from from_list] [-fall_from from_list]\
     [-through through_list] [-rise_through through_list]\
     [-fall_through through_list] [-to to_list] [-rise_to to_list]\
     [-fall_to to_list]}

proc unset_path_exceptions { args } {
  unset_path_exceptions_cmd "unset_path_exceptions" $args
}

################################################################

define_sta_cmd_args "unset_propagated_clock" {objects}

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

define_sta_cmd_args "unset_timing_derate" {}

proc unset_timing_derate { args } {
  check_argc_eq0 "unset_timing_derate" $args
  reset_timing_derate_cmd
}

################################################################
#
# Network editing commands
#  
################################################################

define_sta_cmd_args "connect_pin" {net pin}
# deprecated 2.0.16 05/02/2019
define_sta_cmd_args "connect_pins" {net pins}

define_sta_cmd_args "delete_instance" {inst}

define_sta_cmd_args "delete_net" {net}

define_sta_cmd_args "disconnect_pin" {net -all|pin}
# deprecated 2.0.16 05/02/2019
define_sta_cmd_args "disconnect_pins" {net -all|pins}

define_sta_cmd_args "make_instance" {inst_path lib_cell}

define_sta_cmd_args "make_net" {}

define_sta_cmd_args "replace_cell" {instance lib_cell}

define_sta_cmd_args "insert_buffer" {buffer_name buffer_cell net load_pins\
				       buffer_out_net_name}

################################################################
#
# Delay calculation commands
#  
################################################################

define_sta_cmd_args "set_assigned_delay" \
  {-cell|-net [-rise] [-fall] [-corner corner_name] [-min] [-max]\
     [-from from_pins] [-to to_pins] delay}

# Change the delay for timing arcs between from_pins and to_pins matching
# on cell (instance) or net.
proc set_assigned_delay { args } {
  set_assigned_delay_cmd "set_assigned_delay" $args
}

# compatibility
define_sta_cmd_args "read_parasitics" \
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

################################################################a

define_sta_cmd_args "set_assigned_check" \
  {-setup|-hold|-recovery|-removal [-rise] [-fall]\
     [-corner corner_name] [-min] [-max]\
     [-from from_pins] [-to to_pins] [-clock rise|fall]\
     [-cond sdf_cond] [-worst] check_value}

# -worst is ignored.
proc set_assigned_check { args } {
  set_assigned_check_cmd "set_assigned_check" $args
}

################################################################a

define_sta_cmd_args "set_assigned_transition" \
  {[-rise] [-fall] [-corner corner_name] [-min] [-max] slew pins}

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
    sta_error "set_assigned_transition transition is not a float."
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

################################################################
#  
# Utility commands
#
################################################################

define_sta_cmd_args "delete_from_list" {list objs}

proc delete_from_list { list objects } {
  delete_objects_from_list_cmd $list $objects
}

################################################################

define_sta_cmd_args "get_fanin" \
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
      cmd_usage_error "get_fanout"
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

define_sta_cmd_args "get_fanout" \
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

define_sta_cmd_args "get_name" {objects}
define_sta_cmd_args "get_full_name" {objects}

################################################################

define_sta_cmd_args "get_property" \
  {[-object_type cell|pin|net|port|clock|timing_arc] object property}

proc get_property { args } {
  return [get_property_cmd "get_property" "-object_type" $args]
}

################################################################

define_sta_cmd_args "get_timing_edges" \
  {[-from from_pin] [-to to_pin] [-of_objects objects] [-filter expr]}

proc get_timing_edges { args } {
  return [get_timing_edges_cmd "get_timing_edges" $args]
}

################################################################

define_sta_cmd_args "report_clock_properties" {[clocks]}

proc_redirect report_clock_properties {
  check_argc_eq0or1 "report_clock_properties" $args
  update_generated_clks
  puts "Clock                   Period          Waveform"
  puts "----------------------------------------------------"
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

################################################################

define_sta_cmd_args "report_object_full_names" {[-verbose] objects}

proc report_object_full_names { args } {
  parse_key_args "report_object_full_names" args keys {} flags {-verbose}

  set objects [lindex $args 0]
  if { [info exists flags(-verbose)] } {
    puts -nonewline "{"
    set first 1
    foreach obj [sort_by_full_name $objects] {
      if { !$first } {
	puts -nonewline ", "
      }
      puts -nonewline \"[get_object_type $obj]:[get_full_name $obj]\"
      set first 0
    }
    puts "}"
  } else {
    foreach obj [sort_by_full_name $objects] {
      puts [get_full_name $obj]
    }
  }
}

define_sta_cmd_args "report_object_names" {[-verbose] objects}

proc report_object_names { args } {
  parse_key_args "report_object_names" args keys {} flags {-verbose}

  set objects [lindex $args 0]
  if { [info exists flags(-verbose)] } {
    puts -nonewline "{"
    set first 1
    foreach obj [sort_by_name $objects] {
      if { !$first } {
	puts -nonewline ", "
      }
      puts -nonewline \"[get_object_type $obj]:[get_name $obj]\"
      set first 0
    }
    puts "}"
  } else {
    foreach obj [sort_by_name $objects] {
      puts [get_name $obj]
    }
  }
}

################################################################

define_sta_cmd_args "with_output_to_variable" { var { cmds }}

# with_output_to_variable variable { command args... }
proc with_output_to_variable { var_name args } {
  upvar 1 $var_name var

  set body [lindex $args 0]
  sta::redirect_string_begin;
  catch $body ret
  set var [sta::redirect_string_end]
  return $ret
}

define_sta_cmd_args "set_pocv_sigma_factor" { factor }

# sta namespace end.
}
