# OpenSTA, Static Timing Analyzer
# Copyright (c) 2023, Parallax Software, Inc.
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
# Search debugging/probing commands
#
################################################################

define_cmd_args "check_setup" \
  { [-verbose] [-no_input_delay] [-no_output_delay]\
      [-multiple_clock] [-no_clock]\
      [-unconstrained_endpoints] [-loops] [-generated_clocks]\
      [> filename] [>> filename] }

proc_redirect check_setup {
  check_setup_cmd "check_setup" $args
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
    report_line [lindex $error 0]
    if { $verbose } {
      foreach obj [lrange $error 1 end] {
	report_line "  $obj"
      }
    }
  }
  # return value
  expr [llength $errors] == 0
}

################################################################

# Not a command because users have no reason to ever call this.
# It is only useful for debugging incremental timing updates.
proc find_timing { args } {
  parse_key_args "find_timing" args keys {} flags {-full_update}
  find_timing_cmd [info exists flags(-full_update)]
}

################################################################

define_cmd_args "find_timing_paths" \
  {[-from from_list|-rise_from from_list|-fall_from from_list]\
     [-through through_list|-rise_through through_list|-fall_through through_list]\
     [-to to_list|-rise_to to_list|-fall_to to_list]\
     [-path_delay min|min_rise|min_fall|max|max_rise|max_fall|min_max]\
     [-unconstrained]
     [-corner corner]\
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
      sta_error 420 "$cmd -path_delay must be min, min_rise, min_fall, max, max_rise, max_fall or min_max."
    }
  }

  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg args arg_error]
  set to [parse_to_arg1 keys $end_rf arg_error]
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
    sta_error 421 "$cmd command failed."
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
      sta_error 422 "-endpoint_count must be a positive integer."
    }
  }

  set group_count $endpoint_count
  if [info exists keys(-group_count)] {
    set group_count $keys(-group_count)
    check_positive_integer "-group_count" $group_count
    if { $group_count < 1 } {
      sta_error 423 "-group_count must be >= 1."
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
      sta_error 424 "'$arg' is not a known keyword or flag."
    } else {
      sta_error 425 "positional arguments not supported."
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

define_cmd_args "report_arrival" {pin}

proc report_arrival { pin } {
  report_delays_wrt_clks $pin "arrivals_clk_delays"
}

proc report_delays_wrt_clks { pin_arg what } {
  set pin [get_port_pin_error "pin" $pin_arg]
  foreach vertex [$pin vertices] {
    if { $vertex != "NULL" } {
      report_delays_wrt_clk $vertex $what "NULL" "rise"
      report_delays_wrt_clk $vertex $what [default_arrival_clock] "rise"
      foreach clk [all_clocks] {
	report_delays_wrt_clk $vertex $what $clk "rise"
	report_delays_wrt_clk $vertex $what $clk "fall"
      }
    }
  }
}

proc report_delays_wrt_clk { vertex what clk clk_rf } {
  global sta_report_default_digits

  set rise [$vertex $what rise $clk $clk_rf $sta_report_default_digits]
  set fall [$vertex $what fall $clk $clk_rf $sta_report_default_digits]
  # Filter INF/-INF arrivals.
  if { !([delays_are_inf $rise] && [delays_are_inf $fall]) } {
    set rise_fmt [format_delays $rise]
    set fall_fmt [format_delays $fall]
    if {$clk != "NULL"} {
      set clk_str " ([get_name $clk] [rf_short_name $clk_rf])"
    } else {
      set clk_str ""
    }
    report_line "$clk_str r $rise_fmt f $fall_fmt"
  }
}

proc report_wrt_clks { pin_arg what } {
  set pin [get_port_pin_error "pin" $pin_arg]
  foreach vertex [$pin vertices] {
    if { $vertex != "NULL" } {
      report_wrt_clk $vertex $what "NULL" "rise"
      report_wrt_clk $vertex $what [default_arrival_clock] "rise"
      foreach clk [all_clocks] {
	report_wrt_clk $vertex $what $clk "rise"
	report_wrt_clk $vertex $what $clk "fall"
      }
    }
  }
}

proc report_wrt_clk { vertex what clk clk_rf } {
  global sta_report_default_digits

  set rise [$vertex $what rise $clk $clk_rf]
  set fall [$vertex $what fall $clk $clk_rf]
  # Filter INF/-INF arrivals.
  if { !([times_are_inf $rise] && [times_are_inf $fall]) } {
    set rise_fmt [format_times $rise $sta_report_default_digits]
    set fall_fmt [format_times $fall $sta_report_default_digits]
    if {$clk != "NULL"} {
      set clk_str " ([get_name $clk] [rf_short_name $clk_rf])"
    } else {
      set clk_str ""
    }
    report_line "$clk_str r $rise_fmt f $fall_fmt"
  }
}

proc times_are_inf { times } {
  foreach time $times {
    if { $time < 1e+10 && $time > -1e+10 } {
      return 0
    }
  }
  return 1
}

proc delays_are_inf { delays } {
  foreach delay $delays {
    if { !([string match "INF*" $delay] \
	     || [string match "-INF*" $delay]) } {
      return 0
    }
  }
  return 1
}

################################################################

define_cmd_args "report_clock_skew" {[-setup|-hold]\
					   [-clock clocks]\
					   [-corner corner]]\
					   [-digits digits]}

proc_redirect report_clock_skew {
  global sta_report_default_digits

  parse_key_args "report_clock_skew" args \
    keys {-clock -corner -digits} flags {-setup -hold}
  check_argc_eq0 "report_clock_skew" $args

  if { [info exists flags(-setup)] && [info exists flags(-hold)] } {
    sta_error 419 "report_clock_skew -setup and -hold are mutually exclusive options."
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
  if { $clks != {} } {
    report_clk_skew $clks $corner $setup_hold $digits
  }
}

################################################################

define_cmd_args "report_checks" \
  {[-from from_list|-rise_from from_list|-fall_from from_list]\
     [-through through_list|-rise_through through_list|-fall_through through_list]\
     [-to to_list|-rise_to to_list|-fall_to to_list]\
     [-unconstrained]\
     [-path_delay min|min_rise|min_fall|max|max_rise|max_fall|min_max]\
     [-corner corner]\
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
    report_line "No paths found."
  } else {
    report_path_ends $path_ends
  }
}

################################################################

define_cmd_args "report_check_types" \
  {[-violators] [-verbose]\
     [-corner corner]\
     [-format slack_only|end]\
     [-max_delay] [-min_delay]\
     [-recovery] [-removal]\
     [-clock_gating_setup] [-clock_gating_hold]\
     [-max_slew] [-min_slew]\
     [-max_fanout] [-min_fanout]\
     [-max_capacitance] [-min_capacitance]\
     [-min_pulse_width] [-min_period] [-max_skew]\
     [-net net]\
     [-digits digits] [-no_line_splits]\
     [> filename] [>> filename]}

proc_redirect report_check_types {
  variable path_options

  parse_key_args "report_check_types" args keys {-net -corner}\
    flags {-violators -all_violators -verbose -no_line_splits} 0

  set violators [info exists flags(-violators)]
  if { [info exists flags(-all_violators)] } {
    sta_warn 609 "-all_violators is deprecated. Use -violators"
    set violators 1
  }

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

  set net "NULL"
  if { [info exists keys(-net)] } {
    set net [get_net_warn "-net" $keys(-net)]
  }

  if { $args == {} } {
    if { $min_max == "max" || $min_max == "min_max" } {
      set setup 1
      set recovery 1
      set clk_gating_setup 1
      set max_slew 1
      set max_fanout 1
      set max_capacitance 1
    } else {
      set setup 0
      set recovery 0
      set clk_gating_setup 0
      set max_slew 0
      set max_fanout 0
      set max_capacitance 0
    }
    if { $min_max == "min" || $min_max == "min_max" } {
      set hold 1
      set removal 1
      set clk_gating_hold 1
      set min_slew 1
      set min_fanout 1
      set min_capacitance 1
    } else {
      set hold 0
      set min_delay 0
      set removal 0
      set clk_gating_hold 0
      set min_slew 0
      set min_fanout 0
      set min_capacitance 0
    }
    set min_pulse_width 1
    set min_period 1
    set max_skew 1
  } else {
    parse_key_args "report_check_types" args keys {} \
      flags {-max_delay -min_delay -recovery -removal \
	       -clock_gating_setup -clock_gating_hold \
	       -max_slew -min_slew \
	       -max_fanout -min_fanout \
	       -max_capacitance -min_capacitance \
	       -min_pulse_width \
	       -min_period -max_skew \
	       -max_transition -min_transition } 1

    set setup [info exists flags(-max_delay)]
    set hold [info exists flags(-min_delay)]
    set recovery [info exists flags(-recovery)]
    set removal [info exists flags(-removal)]
    set clk_gating_setup [info exists flags(-clock_gating_setup)]
    set clk_gating_hold [info exists flags(-clock_gating_hold)]
    set max_slew [info exists flags(-max_slew)]
    if { [info exists flags(-max_transition)] } {
      sta_warn 610 "-max_transition deprecated. Use -max_slew."
      set max_slew 1
    }
    set min_slew [info exists flags(-min_slew)]
    if { [info exists flags(-min_transition)] } {
      sta_warn 611 "-min_transition deprecated. Use -min_slew."
      set min_slew 1
    }
    set max_fanout [info exists flags(-max_fanout)]
    set min_fanout [info exists flags(-min_fanout)]
    set max_capacitance [info exists flags(-max_capacitance)]
    set min_capacitance [info exists flags(-min_capacitance)]
    set min_pulse_width [info exists flags(-min_pulse_width)]
    set min_period [info exists flags(-min_period)]
    set max_skew [info exists flags(-max_skew)]
    if { [operating_condition_analysis_type] == "single" \
	   && (($setup && $hold) \
		 || ($recovery && $removal) \
		 || ($clk_gating_setup && $clk_gating_hold)) } {
      sta_error 426 "analysis type single is not consistent with doing both setup/max and hold/min checks."
    }
  }

  if { $args != {} } {
    sta_error 427 "positional arguments not supported."
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
    if { $violators } {
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

  if { $max_slew } {
    report_slew_limits $net $corner "max" $violators $verbose $nosplit
  }
  if { $min_slew } {
    report_slew_limits $net $corner "min" $violators $verbose $nosplit
  }
  if { $max_fanout } {
    report_fanout_limits $net "max" $violators $verbose $nosplit
  }
  if { $min_fanout } {
    report_fanout_limits $net "min" $violators $verbose $nosplit
  }
  if { $max_capacitance } {
    report_capacitance_limits $net $corner "max" $violators $verbose $nosplit
  }
  if { $min_capacitance } {
    report_capacitance_limits $net $corner "min" $violators $verbose $nosplit
  }
  if { $min_pulse_width } {
    if { $violators } {
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
    if { $violators } {
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
    if { $violators } {
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

proc report_slew_limits { net corner min_max violators verbose nosplit } {
  set pins [check_slew_limits $net $violators $corner $min_max]
  if { $pins != {} } {
    report_line "${min_max} slew"
    report_line ""
    if { $verbose } {
      foreach pin $pins {
        report_slew_limit_verbose $pin $corner $min_max
        report_line ""
      }
    } else {
      report_slew_limit_short_header
      foreach pin $pins {
        report_slew_limit_short $pin $corner $min_max
      }
      report_line ""
    }
  }
}

proc report_fanout_limits { net min_max violators verbose nosplit } {
  set pins [check_fanout_limits $net $violators $min_max]
  if { $pins != {} } {
    report_line "${min_max} fanout"
    report_line ""
    if { $verbose } {
      foreach pin $pins {
        report_fanout_limit_verbose $pin $min_max
        report_line ""
      }
    } else {
      report_fanout_limit_short_header
      foreach pin $pins {
        report_fanout_limit_short $pin $min_max
      }
      report_line ""
    }
  }
}

proc report_capacitance_limits { net corner min_max violators verbose nosplit } {
  set pins [check_capacitance_limits $net $violators $corner $min_max]
  if { $pins != {} } {
    report_line "${min_max} capacitance"
    report_line ""
    if { $verbose } {
      foreach pin $pins {
        report_capacitance_limit_verbose $pin $corner $min_max
        report_line ""
      }
    } else {
      report_capacitance_limit_short_header
      foreach pin $pins {
        report_capacitance_limit_short $pin $corner $min_max
      }
      report_line ""
    }
  }
}

################################################################

define_cmd_args "report_dcalc" \
  {[-from from_pin] [-to to_pin] [-corner corner] [-min] [-max] [-digits digits]}

proc_redirect report_dcalc {
  report_dcalc_cmd "report_dcalc" $args "-digits"
}

################################################################

define_cmd_args "report_disabled_edges" {}

################################################################

define_cmd_args "report_tns" { [-digits digits]}

proc_redirect report_tns {
  global sta_report_default_digits

  parse_key_args "report_tns" args keys {-digits} flags {}
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }

  report_line "tns [format_time [total_negative_slack_cmd "max"] $digits]"
}

################################################################

define_cmd_args "report_wns" { [-digits digits]}

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
  report_line "wns [format_time $slack $digits]"
}

################################################################

define_cmd_args "report_worst_slack" {[-min] [-max] [-digits digits]}

proc_redirect report_worst_slack {
  global sta_report_default_digits

  parse_key_args "report_worst_slack" args keys {-digits} flags {-min -max}
  set min_max [parse_min_max_flags flags]
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }

  report_line "worst slack [format_time [worst_slack_cmd $min_max] $digits]"
}

################################################################

define_cmd_args "report_pulse_width_checks" \
  {[-verbose] [-corner corner] [-digits digits] [-no_line_splits] [pins]\
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

# Note that -all and -tags are intentionally "hidden".
define_cmd_args "report_path" \
  {[-min|-max]\
     [-format full|full_clock|full_clock_expanded|short|end|summary]\
     [-fields [capacitance|slew|input_pin|net]\
     [-digits digits] [-no_line_splits]\
     [> filename] [>> filename]\
     pin ^|r|rise|v|f|fall}

proc_redirect report_path {
  parse_key_args "report_path" args keys {} \
    flags {-max -min -all -tags} 0

  if { [info exists flags(-min)] && [info exists flags(-max)] } {
    sta_error 508 "-min and -max cannot both be specified."
  } elseif [info exists flags(-min)] {
    set min_max "min"
  } elseif [info exists flags(-max)] {
    set min_max "max"
  } else {
    # Default to max path.
    set min_max "max"
  }
  set report_tags [info exists flags(-tags)]
  set report_all [info exists flags(-all)]

  parse_report_path_options "report_path" args "full" 1
  check_argc_eq2 "report_path" $args

  set pin_arg [lindex $args 0]
  set tr [parse_rise_fall_arg [lindex $args 1]]

  set pin [get_port_pin_error "pin" $pin_arg]
  if { [$pin is_hierarchical] } {
    sta_error 509 "pin '$pin_arg' is hierarchical."
  } else {
    foreach vertex [$pin vertices] {
      if { $vertex != "NULL" } {
	if { $report_all } {
	  set first 1
	  set path_iter [$vertex path_iterator $tr $min_max]
	  while {[$path_iter has_next]} {
	    set path [$path_iter next]
	    if { $first }  {
	      report_line "Tag group: [$vertex tag_group_index]"
	    } else {
	      report_line ""
	    }
	    if { $report_tags } {
	      report_line "Tag: [$path tag]"
	    }
	    report_path_cmd $path
	    delete_path_ref $path
	    set first 0
	  }
	  $path_iter finish
	} else {
	  set worst_path [vertex_worst_arrival_path_rf $vertex $tr $min_max]
	  if { $worst_path != "NULL" } {
	    if { $report_tags } {
	      report_line "Tag: [$worst_path tag]"
	    }
	    report_path_cmd $worst_path
	    delete_path_ref $worst_path
	  }
	}
      }
    }
  }
}

proc parse_rise_fall_arg { arg } {
  if { $arg eq "r" || $arg eq "^" || $arg eq "rise" } {
    return "rise"
  } elseif { $arg eq "f" || $arg eq "v" || $arg eq "fall" } {
    retur "fall"
  } else {
    error "unknown rise/fall transition name."
  }
}

proc parse_report_path_options { cmd args_var default_format
				 unknown_key_is_error } {
  variable path_options
  variable report_path_field_width_extra
  global sta_report_default_digits

  upvar 1 $args_var args
  if [info exists path_options] {
    unset path_options
  }
  parse_key_args $cmd args path_options {-format -digits -fields} \
    path_options {-no_line_splits -report_sigmas} $unknown_key_is_error

  set format $default_format
  if [info exists path_options(-format)] {
    set format $path_options(-format)
    set formats {full full_clock full_clock_expanded short \
		   end slack_only summary json}
    if { [lsearch $formats $format] == -1 } {
      sta_error 510 "-format $format not recognized."
    }
  } else {
    set path_options(-format) $default_format
  }
  set_report_path_format $format

  set digits $sta_report_default_digits
  if [info exists path_options(-digits)] {
    set digits $path_options(-digits)
    check_positive_integer "-digits" $digits
  }

  set report_sigmas [info exists path_options(-report_sigmas)]
  set_report_path_sigmas $report_sigmas

  set path_options(num_fmt) "%.${digits}f"
  set_report_path_digits $digits
  # Numeric field width expands with digits.
  set field_width [expr $digits + $report_path_field_width_extra]
  if { $report_sigmas } {
    set delay_field_width [expr $field_width * 3 + $report_path_field_width_extra]
  } else {
    set delay_field_width $field_width
  }
  foreach field {total incr} {
    set_report_path_field_width $field $delay_field_width
  }
  foreach field {capacitance slew} {
    set_report_path_field_width $field $field_width
  }

  if { [info exists path_options(-fields)] } {
    set fields $path_options(-fields)
    set report_input_pin [expr [lsearch $fields "input*"] != -1]
    set report_cap [expr [lsearch $fields "cap*"] != -1]
    set report_net [expr [lsearch $fields "net*"] != -1]
    # transition_time - compatibility 06/24/2019
    set report_slew [expr [lsearch $fields "slew*"] != -1 \
		       || [lsearch $fields "trans*"] != -1]
  } else {
    set report_input_pin 0
    set report_cap 0
    set report_net 0
    set report_slew 0
  }
  set_report_path_fields $report_input_pin $report_net \
    $report_cap $report_slew

  set_report_path_no_split [info exists path_options(-no_line_splits)]
}

################################################################

define_cmd_args "report_required" {pin}

proc report_required { pin } {
  report_delays_wrt_clks $pin "requireds_clk_delays"
}

################################################################

define_cmd_args "report_slack" {pin}

proc report_slack { pin } {
  report_delays_wrt_clks $pin "slacks_clk_delays"
}

################################################################

# Internal debugging command.
proc report_tag_arrivals { pin } {
  set pin [get_port_pin_error "pin" $pin]
  foreach vertex [$pin vertices] {
    report_tag_arrivals_cmd $vertex
  }
}

################################################################

define_hidden_cmd_args "total_negative_slack" \
  {[-corner corner] [-min]|[-max]}

proc total_negative_slack { args } {
  parse_key_args "total_negative_slack" args \
    keys {-corner} flags {-min -max}
  check_argc_eq0 "total_negative_slack" $args
  set min_max [parse_min_max_flags flags]
  if { [info exists keys(-corner)] } {
    set corner [parse_corner_required keys]
    set tns [total_negative_slack_corner_cmd $corner $min_max]
  } else {
    set tns [total_negative_slack_cmd $min_max]
  }
  return [time_sta_ui $tns]
}

################################################################

define_hidden_cmd_args "worst_negative_slack" \
  {[-corner corner] [-min]|[-max]}

proc worst_negative_slack { args } {
  set worst_slack [worst_slack1 "worst_negative_slack" $args]
  if { $worst_slack < 0.0 } {
    return $worst_slack
  } else {
    return 0.0
  }
}

################################################################

define_hidden_cmd_args "worst_slack" \
  {[-corner corner] [-min]|[-max]}

proc worst_slack { args } {
  return [worst_slack1 "worst_slack" $args]
}

# arg parsing common to worst_slack/worst_negative_slack
proc worst_slack1 { cmd args1 } {
  parse_key_args $cmd args1 \
    keys {-corner} flags {-min -max}
  check_argc_eq0 $cmd $args1
  set min_max [parse_min_max_flags flags]
  if { [info exists keys(-corner)] } {
    set corner [parse_corner_required keys]
    set worst_slack [worst_slack_corner $corner $min_max]
  } else {
    set worst_slack [worst_slack_cmd $min_max]
  }
  return [time_sta_ui $worst_slack]
}

################################################################

define_hidden_cmd_args "worst_clock_skew" {[-setup]|[-hold]}

proc worst_clock_skew { args } {
  parse_key_args "worst_clock_skew" args keys {} flags {-setup -hold}
  check_argc_eq0 "worst_clock_skew" $args
  if { ([info exists flags(-setup)] && [info exists flags(-hold)]) \
         || (![info exists flags(-setup)] && ![info exists flags(-hold)]) } {
    sta_error 616 "specify one of -setup and -hold."
  } elseif { [info exists flags(-setup)] } {
    set setup_hold "setup"
  } elseif { [info exists flags(-hold)] } {
    set setup_hold "hold"
  }
  return [time_sta_ui [worst_clk_skew_cmd $setup_hold]]
}

################################################################

define_cmd_args "write_timing_model" {[-corner corner] \
                                        [-library_name lib_name]\
                                        [-cell_name cell_name]\
                                        filename}

proc write_timing_model { args } {
  parse_key_args "write_timing_model" args \
    keys {-library_name -cell_name -corner} flags {}
  check_argc_eq1 "write_timing_model" $args

  set filename [file nativename [lindex $args 0]]
  if { [info exists keys(-cell_name)] } {
    set cell_name $keys(-cell_name)
  } else {
    set cell_name [get_name [[top_instance] cell]]
  }
  if { [info exists keys(-library_name)] } {
    set lib_name $keys(-library_name)
  } else {
    set lib_name $cell_name
  }
  set corner [parse_corner keys]
  write_timing_model_cmd $lib_name $cell_name $filename $corner
    
}

################################################################
#
# Helper functions
#
################################################################

proc parse_path_group_arg { group_names } {
  set names {}
  foreach name $group_names {
    if { [is_path_group_name $name] } {
      lappend names $name
    } else {
      sta_warn 318 "unknown path group '$name'."
    }
  }
  return $names
}

proc report_path_ends { path_ends } {
  report_path_end_header
  set prev_end "NULL"
  foreach path_end $path_ends {
    report_path_end2 $path_end $prev_end
    set prev_end $path_end
  }
  report_path_end_footer
}

################################################################

define_cmd_args "report_clock_min_period" \
  { [-clocks clocks] [-include_port_paths] }

proc report_clock_min_period { args } {
  parse_key_args "report_min_clock_period" args \
    keys {-clocks} flags {-include_port_paths} 0
  
  if { [info exists keys(-clocks)] } {
    set clks [get_clocks $keys(-clocks)]
  } else {
    set clks [sort_by_name [all_clocks]]
  }
  set include_port_paths [info exists flags(-include_port_paths)]

  foreach clk $clks {
    set min_period [sta::find_clk_min_period $clk $include_port_paths]
    if { $min_period == 0.0 } {
      set min_period 0
      set fmax "INF"
    } else {
      # max frequency in MHz
      set fmax [expr 1.0e-6 / $min_period]
    }
    puts "[get_name $clk] period_min = [sta::format_time $min_period 2] fmax = [format %.2f $fmax]"
  }
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

# max slew slack / limit
proc max_slew_check_slack_limit {} {
  return [expr "[sta::max_slew_check_slack] / [sta::max_slew_check_limit]"]
}

# max cap slack / limit
proc max_capacitance_check_slack_limit {} {
  return [expr [sta::max_capacitance_check_slack] / [sta::max_capacitance_check_limit]]
}

# max fanout slack / limit
proc max_fanout_check_slack_limit {} {
  return [expr [sta::max_fanout_check_slack] / [sta::max_fanout_check_limit]]
}

################################################################

proc rf_short_name { rf } {
  if { [rf_is_rise $rf] } {
    return [rise_short_name]
  } elseif { [rf_is_fall $rf] } {
    return [fall_short_name]
  } else {
    error "unknown transition name $rf"
  }
}

proc opposite_rf { rf } {
  if { [rf_is_rise $rf] } {
    return "fall"
  } elseif { [rf_is_fall $rf] } {
    return "rise"
  } else {
    error "opposite_rf unknown transition $rf"
  }
}

proc rf_is_rise { rf } {
  if { $rf == "rise" || $rf == "^" || $rf == "r"} {
    return 1
  } else {
    return 0
  }
}

proc rf_is_fall { rf } {
  if { $rf == "fall" || $rf == "v" || $rf == "f"} {
    return 1
  } else {
    return 0
  }
}

# sta namespace end.
}
