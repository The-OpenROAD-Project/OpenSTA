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

################################################################
#
# Search debugging/probing commands
#
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
      set clk_iter [clock_iterator]
      while {[$clk_iter has_next]} {
	set clk [$clk_iter next]
	report_delays_wrt_clk $vertex $what $clk "rise"
	report_delays_wrt_clk $vertex $what $clk "fall"
      }
      $clk_iter finish
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
      set clk_str " ([get_name $clk] [rise_fall_short_name $clk_rf])"
    } else {
      set clk_str ""
    }
    puts "$clk_str r $rise_fmt f $fall_fmt"
  }
}

proc report_wrt_clks { pin_arg what } {
  set pin [get_port_pin_error "pin" $pin_arg]
  foreach vertex [$pin vertices] {
    if { $vertex != "NULL" } {
      report_wrt_clk $vertex $what "NULL" "rise"
      report_wrt_clk $vertex $what [default_arrival_clock] "rise"
      set clk_iter [clock_iterator]
      while {[$clk_iter has_next]} {
	set clk [$clk_iter next]
	report_wrt_clk $vertex $what $clk "rise"
	report_wrt_clk $vertex $what $clk "fall"
      }
      $clk_iter finish
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
      set clk_str " ([get_name $clk] [rise_fall_short_name $clk_rf])"
    } else {
      set clk_str ""
    }
    puts "$clk_str r $rise_fmt f $fall_fmt"
  }
}

proc rise_fall_short_name { tr } {
  if { $tr eq "rise" } {
    return [rise_short_name]
  } elseif { $tr eq "fall" } {
    return [fall_short_name]
  } else {
    error "unknown transition name $tr"
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
    sta_error "-min and -max cannot both be specified."
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
    sta_error "pin '$pin_arg' is hierarchical."
  } else {
    foreach vertex [$pin vertices] {
      if { $vertex != "NULL" } {
	if { $report_all } {
	  set first 1
	  set path_iter [$vertex path_iterator $tr $min_max]
	  while {[$path_iter has_next]} {
	    set path [$path_iter next]
	    if { $first }  {
	      puts "Tag group: [$vertex tag_group_index]"
	    } else {
	      puts ""
	    }
	    if { $report_tags } {
	      puts "Tag: [$path tag]"
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
	      puts "Tag: [$worst_path tag]"
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
		   end slack_only summary}
    if { [lsearch $formats $format] == -1 } {
      sta_error "-format $format not recognized."
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
  # Numberic field width expands with digits.
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
      sta_warn "unknown path group '$name'."
    }
  }
  return $names
}

proc report_slew_limits { corner min_max all_violators verbose nosplit } {
  if { $all_violators } {
    set violators [pin_slew_limit_violations $corner $min_max]
    if { $violators != {} } {
      puts "${min_max}_transition"
      puts ""
      if { $verbose } {
	foreach pin $violators {
	  report_slew_limit_verbose $pin $corner $min_max
	  puts ""
	}
      } else {
	report_slew_limit_short_header
	foreach pin $violators {
	  report_slew_limit_short $pin $corner $min_max
	}
	puts ""
      }
    }
  } else {
    set pin [pin_min_slew_limit_slack $corner $min_max]
    if { $pin != "NULL" } {
      puts "${min_max}_transition"
      puts ""
      if { $verbose } {
	report_slew_limit_verbose $pin $corner $min_max
	puts ""
      } else {
	report_slew_limit_short_header
	report_slew_limit_short $pin $corner $min_max
	puts ""
      }
    }
  }
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

# sta namespace end.
}
