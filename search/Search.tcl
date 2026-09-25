# OpenSTA, Static Timing Analyzer
# Copyright (c) 2026, Parallax Software, Inc.
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

################################################################
#
# Search debugging/probing commands
#
################################################################

define_cmd_args "check_setup" \
  { [-verbose] [-no_input_delay] [-no_output_delay]\
      [-multiple_clock] [-no_clock]\
      [-unconstrained_endpoints] [-loops] [-generated_clocks]\
      [> filename] [>> filename] } \
  -help {The `check_setup` command performs sanity checks on the design. Individual checks can be performed with the keywords. If no check keywords are specified all checks are performed. Checks that fail are reported as warnings. If no checks fail nothing is reported. The command returns 1 if there are no warnings for use in scripts.} \
  -arg_help {
    -verbose {Show offending objects rather than just error counts.}
    -unconstrained_endpoints {Check path endpoints for timing constraints (timing check or `set_output_delay`).}
    -multiple_clock {Check register/latch clock pins for multiple clocks.}
    -no_clock {Check register/latch clock pins for a clock.}
    -no_input_delay {Check for inputs that do not have a `set_input_delay` command.}
    -no_output_delay {Check for outputs that do not have a `set_output_delay` command.}
    -no_output_delay {Check for outputs that do not have a `set_output_delay` command.}
    -loops {Check for combinational logic loops.}
    -generated_clocks {Check that generated clock source pins have been defined as clocks.}
  }

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
     [-scenes scenes]\
     [-group_path_count path_count] \
     [-endpoint_path_count path_count]\
     [-unique_paths_to_endpoint]\
     [-unique_edges_to_endpoint]\
     [-slack_max slack_max]\
     [-slack_min slack_min]\
     [-sort_by_slack]\
     [-path_group group_name]} \
  -help {The `find_timing_paths` command returns a list of path objects for scripting. Use the `get_property` function to access properties of the paths.} \
  -arg_help {
    -from {Return paths from a list of clocks, instances, ports, register clock pins, or latch data pins.}
    -rise_from {Return paths from the rising edge of clocks, instances, ports, register clock pins, or latch data pins.}
    -fall_from {Return paths from the falling edge of clocks, instances, ports, register clock pins, or latch data pins.}
    -through {Return paths through a list of instances, pins or nets.}
    -rise_through {Return rising paths through a list of instances, pins or nets.}
    -fall_through {Return falling paths through a list of instances, pins or nets.}
    -to {Return paths to a list of clocks, instances, ports or pins.}
    -rise_to {Return rising paths to a list of clocks, instances, ports or pins.}
    -fall_to {Return falling paths to a list of clocks, instances, ports or pins.}
    -unconstrained {Report unconstrained paths also.}
    -path_delay {`min`: Return min path (hold) checks. `min_rise`: Return min path (hold) checks for rising endpoints. `min_fall`: Return min path (hold) checks for falling endpoints. `max`: Return max path (setup) checks. `max_rise`: Return max path (setup) checks for rising endpoints. `max_fall`: Return max path (setup) checks for falling endpoints. `min_max`: Return min and max path (setup and hold) checks.}
    -group_path_count {`path_count`: The number of paths to return in each path group.}
    -endpoint_path_count {`endpoint_path_count`: The number of paths to return for each endpoint.}
    -unique_paths_to_endpoint {Return multiple paths to an endpoint that traverse different pins without showing multiple paths with different rise/fall transitions.}
    -unique_edges_to_endpoint {When multiple paths to an endpoint are requested, only the worst path through the same pins and rise/fall edges is returned.}
    -scenes {`scenes`: Return paths for these scenes. The default is all scenes.}
    -slack_max {`max_slack`: Return paths with slack less than max_slack.}
    -slack_min {`min_slack`: Return paths with slack greater than min_slack.}
    -sort_by_slack {Sort paths by slack rather than slack within path groups.}
    -path_group {`groups`: Return paths in path groups. Paths in all groups are returned if this option is not specified.}
  }

proc find_timing_paths { args } {
  set path_ends [find_timing_paths_cmd "find_timing_paths" args]
  return $path_ends
}

proc find_timing_paths_cmd { cmd args_var } {
  global sta_report_unconstrained_paths
  upvar 1 $args_var args

  parse_key_args $cmd args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to \
            -path_delay -corner -scenes -group_count -endpoint_count \
            -group_path_count -endpoint_path_count \
            -slack_max -slack_min -path_group} \
    flags {-unconstrained -sort_by_slack \
             -unique_paths_to_endpoint \
             -unique_edges_to_endpoint} 0

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
      sta_error 510 "$cmd -path_delay must be min, min_rise, min_fall, max, max_rise, max_fall or min_max."
    }
  }

  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg args arg_error]
  set to [parse_to_arg1 keys $end_rf arg_error]
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
    sta_error 511 "$cmd command failed."
  }

  if { [info exists flags(-unconstrained)] } {
    set unconstrained 1
  } elseif { [info exists sta_report_unconstrained_paths] } {
    set unconstrained $sta_report_unconstrained_paths
  } else {
    set unconstrained 0
  }

  set scenes [parse_scenes_or_all keys]

  set endpoint_path_count 1
  if { [info exists keys(-endpoint_count)] } {
    # deprecated 2024-11-22
    sta_warn 502 "$cmd -endpoint_count is deprecated. Use -endpoint_path_count instead."
    set endpoint_path_count $keys(-endpoint_count)
  }
  if [info exists keys(-endpoint_path_count)] {
    set endpoint_path_count $keys(-endpoint_path_count)
  }
  if { $endpoint_path_count < 1 } {
    sta_error 512 "-endpoint_path_count must be a positive integer."
  }

  set group_path_count $endpoint_path_count
  if { [info exists keys(-group_count)] } {
    # deprecated 2024-11-22
    sta_warn 503 "$cmd -group_count is deprecated. Use -group_path_count instead."
    set group_path_count $keys(-group_count)
  }
  if [info exists keys(-group_path_count)] {
    set group_path_count $keys(-group_path_count)
  }
  check_positive_integer "-group_path_count" $group_path_count
  if { $group_path_count < 1 } {
    sta_error 513 "-group_path_count must be >= 1."
  }

  set unique_pins [info exists flags(-unique_paths_to_endpoint)]
  set unique_edges [info exists flags(-unique_edges_to_endpoint)]

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
      sta_error 514 "'$arg' is not a known keyword or flag."
    } else {
      sta_error 515 "positional arguments not supported."
    }
  }
  set path_ends [find_path_ends $from $thrus $to $unconstrained \
                   $scenes $min_max \
                   $group_path_count $endpoint_path_count \
                   $unique_pins $unique_edges \
                   $slack_min $slack_max \
                   $sort_by_slack $groups \
                   1 1 1 1 1 1]
  return $path_ends
}

################################################################

define_cmd_args "report_clock_skew" {[-setup|-hold]\
                                       [-clocks clocks]\
                                       [-scenes scenes]\
                                       [-include_internal_latency]
                                       [-digits digits]} \
  -help {Report the maximum difference in clock arrival between every source and target register that has a path between the source and target registers.} \
  -arg_help {
    -clocks {The clocks to report. The default is all clocks.}
    -scenes {Report clocks for these scenes. The default is all scenes.}
    -include_internal_latency {Include internal clock latency from liberty min/max_clock_tree_path timing groups.}
  }

proc_redirect report_clock_skew {
  global sta_report_default_digits

  parse_key_args "report_clock_skew" args \
    keys {-clocks -corner -scenes -digits} \
    flags {-setup -hold -include_internal_latency}
  check_argc_eq0 "report_clock_skew" $args

  if { [info exists flags(-setup)] && [info exists flags(-hold)] } {
    sta_error 516 "report_clock_skew -setup and -hold are mutually exclusive options."
  } elseif { [info exists flags(-setup)] } {
    set setup_hold "setup"
  } elseif { [info exists flags(-hold)] } {
    set setup_hold "hold"
  } else {
    set setup_hold "setup"
  }

  set scenes [parse_scenes_or_all keys]
  if [info exists keys(-clocks)] {
    puts "clks1 = [get_object_names $clks]"
  } else {
    set clks [get_scene_clocks $scenes]
  }

  set include_internal_latency [info exists flags(-include_internal_latency)]
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  if { $clks != {} } {
    report_clk_skew $clks $scenes $setup_hold $include_internal_latency $digits
  }
}

################################################################

define_cmd_args "report_clock_latency" {[-clocks clocks]\
                                          [-scenes scene]\
                                          [-include_internal_latency]
                                          [-digits digits]} \
  -help {Report the clock network latency.} \
  -arg_help {
    -clocks {The clocks to report. The default is all clocks.}
    -scenes {Report latency for these scenes. The default is all scenes.}
    -include_internal_latency {Include internal clock latency from liberty min/max_clock_tree_path timing groups.}
  }

proc_redirect report_clock_latency {
  global sta_report_default_digits

  parse_key_args "report_clock_" args \
    keys {-clocks -scenes -digits} \
    flags {-include_internal_latency}
  check_argc_eq0 "report_clock_latency" $args

  set scenes [parse_scenes_or_all keys]
  if [info exists keys(-clocks)] {
    set clks [get_clocks_warn "-clocks" $keys(-clocks)]
  } else {
    set clks [get_scene_clocks $scenes]
  }
  set include_internal_latency [info exists flags(-include_internal_latency)]
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  if { $clks != {} } {
    report_clk_latency $clks $scenes $include_internal_latency $digits
  }
}

################################################################

define_cmd_args "report_checks" \
  {[-from from_list|-rise_from from_list|-fall_from from_list]\
     [-through through_list|-rise_through through_list|-fall_through through_list]\
     [-to to_list|-rise_to to_list|-fall_to to_list]\
     [-unconstrained]\
     [-path_delay min|min_rise|min_fall|max|max_rise|max_fall|min_max]\
     [-scenes scenes]\
     [-group_path_count path_count] \
     [-endpoint_path_count path_count]\
     [-unique_paths_to_endpoint]\
     [-unique_edges_to_endpoint]\
     [-slack_max slack_max]\
     [-slack_min slack_min]\
     [-sort_by_slack]\
     [-path_group group_name]\
     [-format full|full_clock|full_clock_expanded|short|end|slack_only|summary|json]\
     [-fields capacitance|slew|fanout|input_pin|net|src_attr|variation]\
     [-digits digits]\
     [-no_line_splits]\
     [> filename] [>> filename]} \
  -help {The `report_checks` command reports paths in the design. Paths are reported in groups by capture clock, unclocked path delays, gated clocks and unconstrained.

See `set_false_path` for a description of allowed from_list, through_list and to_list objects.} \
  -arg_help {
    -from {Report paths from a list of clocks, instances, ports, register clock pins, or latch data pins.}
    -rise_from {Report  paths from the rising edge of clocks, instances, ports, register clock pins, or latch data pins.}
    -fall_from {Report paths from the falling edge of clocks, instances, ports, register clock pins, or latch data pins.}
    -through {Report paths through a list of instances, pins or nets.}
    -rise_through {Report rising paths through a list of instances, pins or nets.}
    -fall_through {Report falling paths through a list of instances, pins or nets.}
    -to {Report paths to a list of clocks, instances, ports or pins.}
    -rise_to {Report rising paths to a list of clocks, instances, ports or pins.}
    -fall_to {Report falling paths to a list of clocks, instances, ports or pins.}
    -unconstrained {Report unconstrained paths also. The unconstrained path group is not reported without this option.}
    -path_delay {`min`: Report min path (hold) checks. `min_rise`: Report min path (hold) checks for rising endpoints. `min_fall`: Report min path (hold) checks for falling endpoints. `max`: Report max path (setup) checks. `max_rise`: Report max path (setup) checks for rising endpoints. `max_fall`: Report max path (setup) checks for falling endpoints. `min_max`: Report min and max path (setup and hold) checks.}
    -group_path_count {`path_count`: The number of paths to report in each path group. The default is 1.}
    -endpoint_path_count {`endpoint_path_count`: The number of paths to report for each endpoint. The default is 1.}
    -unique_paths_to_endpoint {When multiple paths to an endpoint are specified with `-endpoint_path_count`, many of the paths may differ only in the rise/fall edges of the pins in the paths. With this option only the worst path through the set of pins is reported.}
    -unique_edges_to_endpoint {When multiple paths to an endpoint are specified with `-endpoint_path_count`, conditional timing arcs result in paths that go through the same pins and rise/fall edges. With this option only the worst path through the set of pins and rise/fall edges is reported.}
    -scenes {Report paths for these scenes. The default is all scenes.}
    -slack_max {Only report paths with less slack than max_slack.}
    -slack_min {Only report paths with more slack than min_slack.}
    -sort_by_slack {Sort paths by slack rather than slack grouped by path group.}
    -path_group {List of path groups to report. The default is to report all path groups.}
    -format {`end`: Report path ends in one line with delay, required time and slack. `full`: Report path start and end points and the path. This is the default path type. `full_clock`: Report path start and end points, the path, and the source and target clock paths. `full_clock_expanded`: Report path start and end points, the path, and the source and target clock paths. If the clock is generated and propagated, the path from the clock source pin is also reported. `short`: Report only path start and end points. `summary`: Report only path ends with delay. `json`: Report in json format. `-fields` is ignored.}
    -fields {List of capacitance|slew|input_pins|hierarchical_pins|net|fanout|src_attr|variation}
    -no_line_splits {Do not split long lines into multiple lines.}
  }

proc_redirect report_checks {
  global sta_report_unconstrained_paths
  parse_report_path_options "report_checks" args "full" 0
  set path_ends [find_timing_paths_cmd "report_checks" args]
  report_path_ends $path_ends
}

################################################################

define_cmd_args "report_check_types" \
  {[-scenes scenes] [-violators] [-verbose]\
     [-format slack_only|end]\
     [-max_delay] [-min_delay]\
     [-recovery] [-removal]\
     [-clock_gating_setup] [-clock_gating_hold]\
     [-max_slew] [-min_slew]\
     [-max_fanout] [-min_fanout]\
     [-max_capacitance] [-min_capacitance]\
     [-min_pulse_width] [-min_period] [-max_skew]\
     [-net net]\
     [-max_count max_count]\
     [-digits digits] [-no_line_splits]\
     [> filename] [>> filename]} \
  -help {The `report_check_types` command reports the slack for each type of timing and design rule constraint. The keyword options allow a subset of the constraint types to be reported.} \
  -arg_help {
    -scenes {Report checks for some scenes. The default value is all scenes.}
    -violators {Report all violated timing and design rule constraints.}
    -verbose {Use a verbose output format.}
    -format {`slack_only`: Report the minimum slack for each timing check. `end`: Report the endpoint for each check.}
    -fields {List of capacitance|slew|input_pins|hierarchical_pins|net|fanout|src_attr|variation}
    -max_delay {Report setup and max delay path delay constraints.}
    -min_delay {Report hold and min delay path delay constraints.}
    -recovery {Report asynchronous recovery checks.}
    -removal {Report asynchronous removal checks.}
    -clock_gating_setup {Report gated clock enable setup checks.}
    -clock_gating_hold {Report gated clock hold setup checks.}
    -max_slew {Report max transition design rule checks.}
    -max_skew {Report max skew design rule checks.}
    -min_pulse_width {Report min pulse width design rule checks.}
    -min_period {Report min period design rule checks.}
    -min_slew {Report min slew design rule checks.}
    -max_fanout {Report max fanout design rule checks.}
    -min_fanout {Report min fanout design rule checks.}
    -max_capacitance {Report max capacitance design rule checks.}
    -min_capacitance {Report min capacitance design rule checks.}
    -net {Report checks on this net.}
    -max_count {Maximum number of checks to report.}
    -no_line_splits {Do not split long lines into multiple lines.}
  }

proc_redirect report_check_types {
  variable float_inf
  variable group_path_count_max
  variable path_options

  parse_key_args "report_check_types" args \
    keys {-scenes -corner -net -max_count}\
    flags {-violators -verbose -no_line_splits} 0

  set violators [info exists flags(-violators)]
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

  set net "NULL"
  if { [info exists keys(-net)] } {
    set net [get_net_arg "-net" $keys(-net)]
  }

  set max_count 1
  if { [info exists keys(-max_count)] } {
    set max_count $keys(-max_count)
    check_positive_integer "-max_count" $max_count
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
               -min_period -max_skew} 1

    set setup [info exists flags(-max_delay)]
    set hold [info exists flags(-min_delay)]
    set recovery [info exists flags(-recovery)]
    set removal [info exists flags(-removal)]
    set clk_gating_setup [info exists flags(-clock_gating_setup)]
    set clk_gating_hold [info exists flags(-clock_gating_hold)]
    set max_slew [info exists flags(-max_slew)]
    set min_slew [info exists flags(-min_slew)]
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
      sta_error 520 "analysis type single is not consistent with doing both setup/max and hold/min checks."
    }
  }
  set scenes [parse_scenes_or_all keys]

  if { $args != {} } {
    sta_error 521 "positional arguments not supported."
  }

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
      set group_path_count $group_path_count_max
      set slack_min [expr -$float_inf]
      set slack_max 0.0
    } else {
      set group_path_count 1
      set slack_min [expr -$float_inf]
      set slack_max $float_inf
    }

    set path_ends [find_path_ends "NULL" {} "NULL" 0 \
                     $scenes $path_min_max $group_path_count 1 1 0 \
                     $slack_min $slack_max \
                     0 {} \
                     $setup $hold \
                     $recovery $removal \
                     $clk_gating_setup $clk_gating_hold]
    report_path_ends $path_ends
  }

  if { $max_slew } {
    report_slew_checks $net $max_count $violators $verbose $scenes "max"
  }
  if { $min_slew } {
    report_slew_checks $net $max_count $violators $verbose $scenes "min"
  }
  if { $max_fanout } {
    report_fanout_checks $net $max_count $violators $verbose $scenes "max"
  }
  if { $min_fanout } {
    report_fanout_checks $net $max_count $violators $verbose $scenes "min"
  }
  if { $max_capacitance } {
    report_capacitance_checks $net $max_count $violators $verbose $scenes "max"
  }
  if { $min_capacitance } {
    report_capacitance_checks $net $max_count $violators $verbose $scenes "min"
  }
  if { $min_pulse_width } {
    report_min_pulse_width_checks $net $max_count $violators $verbose $scenes
  }
  if { $min_period } {
    report_min_period_checks $net $max_count $violators $verbose $scenes
  }
  if { $max_skew } {
    report_max_skew_checks $net $max_count $violators $verbose $scenes
  }
}

################################################################

define_cmd_args "report_disabled_edges" {} \
  -help {The `report_disabled_edges` command reports disabled timing arcs along with the reason they are disabled. Each disabled timing arc is reported as the instance name along with the from and to ports of the arc. The disable reason is shown next. Arcs that are disabled with `set_disable_timing` are reported with constraint as the reason. Arcs that are disabled by constants are reported with constant as the reason along with the constant instance pin and value. Arcs that are disabled to break combinational feedback loops are reported with loop as the reason.

```
> report_disabled_edges
u1 A B constant B=0
```}

################################################################

define_cmd_args "report_tns" {[-min] [-max] [-digits digits]} \
  -help {Report the total negative slack.}

proc_redirect report_tns {
  global sta_report_default_digits

  parse_key_args "report_tns" args keys {-digits} flags {-min -max}
  set min_max "max"
  if { [info exists flags(-min)] } {
    set min_max "min"
  }
  if { [info exists flags(-max)] } {
    set min_max "max"
  }
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }

  report_line "tns $min_max [format_time [total_negative_slack_cmd $min_max] $digits]"
}

################################################################

define_cmd_args "report_wns" {[-min] [-max] [-digits digits]} \
  -help {Report the worst negative slack. If the worst slack is positive, zero is reported.}

proc_redirect report_wns {
  global sta_report_default_digits

  parse_key_args "report_wns" args keys {-digits} flags {-min -max}
  set min_max "max"
  if { [info exists flags(-min)] } {
    set min_max "min"
  }
  if { [info exists flags(-max)] } {
    set min_max "max"
  }
  if { [info exists keys(-digits)] } {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }

  set slack [worst_slack_cmd $min_max]
  if { $slack > 0.0 } {
    set slack 0.0
  }
  report_line "wns $min_max [format_time $slack $digits]"
}

################################################# ###############

define_cmd_args "report_worst_slack" {[-min] [-max] [-digits digits]} \
  -help {Report the worst slack in the design.}

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

  report_line "worst slack $min_max [format_time [worst_slack_cmd $min_max] $digits]"
}

################################################################

# Note that -all and -tags are intentionally "hidden".
define_cmd_args "report_path" \
  {[-min|-max]\
     [-format full|full_clock|full_clock_expanded|short|end|summary]\
     [-fields capacitance|slew|input_pin|net|src_attr]\
     [-digits digits] [-no_line_splits]\
     [> filename] [>> filename]\
     pin ^|r|rise|v|f|fall}

proc_redirect report_path {
  parse_key_args "report_path" args keys {} \
    flags {-max -min -all -tags} 0

  if { [info exists flags(-min)] && [info exists flags(-max)] } {
    sta_error 522 "-min and -max cannot both be specified."
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
  set rf [parse_rise_fall_arg [lindex $args 1]]

  set pin [get_port_pin_error "pin" $pin_arg]
  if { [$pin is_hierarchical] } {
    sta_error 523 "pin '$pin_arg' is hierarchical."
  } else {
    foreach vertex [$pin vertices] {
      if { $vertex != "NULL" } {
        if { $report_all } {
          set first 1
          set path_iter [$vertex path_iterator $rf $min_max]
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
            set first 0
          }
          $path_iter finish
        } else {
          set worst_path [vertex_worst_arrival_path_rf $vertex $rf $min_max]
          if { $worst_path != "NULL" } {
            if { $report_tags } {
              report_line "Tag: [$worst_path tag]"
            }
            report_path_cmd $worst_path
          }
        }
      }
    }
  }
}

proc parse_report_path_options { cmd args_var default_format
                                 unknown_key_is_error } {
  variable path_options
  global sta_report_default_digits

  upvar 1 $args_var args
  if [info exists path_options] {
    unset path_options
  }
  parse_key_args $cmd args path_options {-format -digits -fields} \
    path_options {-no_line_splits} $unknown_key_is_error

  set format $default_format
  if [info exists path_options(-format)] {
    set format $path_options(-format)
    set formats {full full_clock full_clock_expanded short \
                   end slack_only summary json}
    if { [lsearch $formats $format] == -1 } {
      sta_error 524 "-format $format not recognized."
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

  set path_options(num_fmt) "%.${digits}f"
  set_report_path_digits $digits

  set fields {}
  if { [info exists path_options(-fields)] } {
    foreach field $path_options(-fields) {
      if { [string match "input*" $field] } {
        lappend fields "input_pin"
      } elseif { [string match "hier*" $field] } {
        lappend fields "hierarchical_pin"
      } elseif { [string match "net" $field] } {
        lappend fields "net"
      } else {
        set field_name [find_report_path_field_abrev $field]
        if { $field_name != "" } {
          lappend fields $field_name
        } else {
          sta_warn 168 "unknown field $field."
        }
      }
    }
  }
  set_report_path_fields $fields
  set_report_path_no_split [info exists path_options(-no_line_splits)]
}

################################################################

define_cmd_args "report_arrival" {[-scene scene] [-report_variance] [-digits digits] pin} \
  -help {The `report_arrival` command reports min/max rise/fall arrival times at a pin with respect to each clock that has a path to the pin.} \
  -arg_help {
    pin {A pin or port.}
  }

proc report_arrival { args } {
  global sta_report_default_digits

  parse_key_args "report_arrival" args keys {-scene -digits} flags {-report_variance}
  check_argc_eq1 "report_arrival" $args

  set pin [get_port_pin_error "pin" [lindex $args 0]]
  set scene [parse_scene keys]
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  set report_variance [info exists flags(-report_variance)]
  report_arrival_wrt_clks $pin $scene $report_variance $digits
}

################################################################

define_cmd_args "report_required" {[-scene scene] [-report_variance] [-digits digits] pin} \
  -help {The `report_required` command reports min/max rise/fall required times at a pin with respect to each clock.} \
  -arg_help {
    pin {A pin or port.}
  }

proc report_required { args } {
  global sta_report_default_digits

  parse_key_args "report_required" args keys {-scene -digits} flags {-report_variance}
  check_argc_eq1 "report_required" $args

  set pin [get_port_pin_error "pin" [lindex $args 0]]
  set scene [parse_scene keys]
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  set report_variance [info exists flags(-report_variance)]
  report_required_wrt_clks $pin $scene $report_variance $digits
}

################################################################

define_cmd_args "report_slack" {[-scene scene] [-report_variance] [-digits digits] pin} \
  -help {The `report_slack` command reports min/max rise/fall slack at a pin with respect to each clock.} \
  -arg_help {
    pin {A pin or port.}
  }

proc report_slack { args } {
  global sta_report_default_digits

  parse_key_args "report_slack" args keys {-scene -digits} flags {-report_variance}
  check_argc_eq1 "report_slack" $args

  set pin [get_port_pin_error "pin" [lindex $args 0]]
  set scene [parse_scene keys]
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  set report_variance [info exists flags(-report_variance)]
  report_slack_wrt_clks $pin $scene $report_variance $digits
}

################################################################

# Internal debugging command.
proc report_tag_arrivals { args } {
  global sta_report_default_digits

  parse_key_args "report_tag_arrivals" args keys {-digits} flags {}
  set pin [get_port_pin_error "pin" [lindex $args 0]]
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  foreach vertex [$pin vertices] {
    report_tag_arrivals_cmd $vertex 1 $digits
  }
}

################################################################

define_hidden_cmd_args "total_negative_slack" \
  {[-scene scene] [-min]|[-max]}

proc total_negative_slack { args } {
  parse_key_args "total_negative_slack" args \
    keys {-scene -corner} flags {-min -max}
  check_argc_eq0 "total_negative_slack" $args
  set min_max [parse_min_max_flags flags]
  # compabibility 05/29/2025
  if { [info exists keys(-scene)] || [info exists keys(-corner)]} {
    set scene [parse_scene_required keys]
    set tns [total_negative_slack_scene_cmd $scene $min_max]
  } else {
    set tns [total_negative_slack_cmd $min_max]
  }
  return [time_sta_ui $tns]
}

################################################################

define_hidden_cmd_args "worst_negative_slack" \
  {[-scene scene] [-min]|[-max]}

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
  {[-scene scene] [-min]|[-max]}

proc worst_slack { args } {
  return [worst_slack1 "worst_slack" $args]
}

# arg parsing common to worst_slack/worst_negative_slack
proc worst_slack1 { cmd args1 } {
  parse_key_args $cmd args1 \
    keys {-corner -scene} flags {-min -max}
  check_argc_eq0 $cmd $args1
  set min_max [parse_min_max_flags flags]
  # compabibility 05/29/2025
  if { [info exists keys(-scene)] || [info exists keys(-corner)]} {
    set scene [parse_scene_required keys]
    set worst_slack [worst_slack_scene $scene $min_max]
  } else {
    set worst_slack [worst_slack_cmd $min_max]
  }
  return [time_sta_ui $worst_slack]
}

################################################################

define_hidden_cmd_args "worst_clock_skew" \
  {[-setup]|[-hold][-include_internal_latency]}

proc worst_clock_skew { args } {
  parse_key_args "worst_clock_skew" args keys {} \
    flags {-setup -hold -include_internal_latency}
  check_argc_eq0 "worst_clock_skew" $args
  if { ([info exists flags(-setup)] && [info exists flags(-hold)]) \
         || (![info exists flags(-setup)] && ![info exists flags(-hold)]) } {
    sta_error 526 "specify one of -setup and -hold."
  } elseif { [info exists flags(-setup)] } {
    set setup_hold "setup"
  } elseif { [info exists flags(-hold)] } {
    set setup_hold "hold"
  }
  set include_internal_latency [info exists flags(-include_internal_latency)]
 return [time_sta_ui [worst_clk_skew_cmd $setup_hold $include_internal_latency]]
}

################################################################

define_cmd_args "write_timing_model" {[-scene scene] \
                                        [-library_name lib_name]\
                                        [-cell_name cell_name]\
                                        filename} \
  -help {The `write_timing_model` command constructs a liberty timing model for the current design and writes it to filename. cell_name defaults to the cell name of the top level block in the design.

The SDC used to extract the block should include the clock definitions. If the block contains a clock network `set_propagated_clock` should be used so the clock delays are included in the timing model. The following SDC commands are ignored when building the timing model.

```
set_input_delay
set_output_delay
set_load
set_timing_derate
```

Using `set_input_transition` with the slew from the block context will be used will improve the match between the timing model and the block netlist.  Paths defined on clocks that are defined on internal pins are ignored because the model has no way to include the clock definition.

The resulting timing model can be used in a hierarchical timing flow as a replacement for the block to speed up timing analysis. This hierarchical timing methodology does not handle timing exceptions that originate or terminate inside the block. The timing model includes:

```
combinational paths between inputs and outputs
setup and hold timing constraints on inputs
clock to output timing paths
```

Resistance of long wires on inputs and outputs of the block cannot be modeled in Liberty. To reduce inaccuracies from wire resistance in technologies with resistive wires place buffers on inputs and ouputs.

The extracted timing model setup/hold checks are scalar (no input slew dependence). Delay timing arcs are load dependent but do not include input slew dependency.} \
  -arg_help {
    -library_name {The name to use for the liberty library. Defaults to cell_name.}
    -cell_name {The name to use for the liberty cell. Defaults to the top level module name.}
    -scene {The scene to use for extracting the model.}
    filename {Filename for the liberty timing model.}
  }

proc write_timing_model { args } {
  parse_key_args "write_timing_model" args \
    keys {-library_name -cell_name -scene} flags {}
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
  set scene [parse_scene keys]
  write_timing_model_cmd $lib_name $cell_name $filename $scene
    
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
      sta_warn 527 "unknown path group '$name'."
    }
  }
  return $names
}

################################################################

define_cmd_args "report_clock_min_period" \
  { [-clocks clocks] [-include_port_paths] } \
  -help {Report the minimum period and maximum frequency for clocks. If the `-clocks` argument is not specified all clocks are reported. The minimum period is determined by examining the smallest slack paths between registers on the rising edges of the clock or between falling edges of the clock. Paths between different clocks, different clock edges of the same clock, level-sensitive latches, or paths constrained by `set_multicycle_path` or `set_max_delay` are not considered.} \
  -arg_help {
    -clocks {The clocks to report.}
    -include_port_paths {Include paths from input port and to output ports.}
  }

proc_redirect report_clock_min_period {
  parse_key_args "report_min_clock_period" args \
    keys {-clocks} flags {-include_port_paths} 0
  
  if { [info exists keys(-clocks)] } {
    set clks [get_clocks $keys(-clocks)]
  } else {
    set clks [sort_by_name [all_clocks]]
  }
  set include_port_paths [info exists flags(-include_port_paths)]

  foreach clk $clks {
    set min_period [find_clk_min_period $clk $include_port_paths]
    if { $min_period == 0.0 } {
      set min_period 0
      set fmax "INF"
    } else {
      # max frequency in MHz
      set fmax [expr 1.0e-6 / $min_period]
    }
    report_line "[get_name $clk] period_min = [format_time $min_period 2] fmax = [format %.2f $fmax]"
  }
}

################################################################

define_cmd_args "set_disable_clock_gating_check" { objects } \
  -help {The `set_disable_clock_gating_check` command disables clock gating checks on Liberty cells, instances, ports, or pins. When a Liberty cell is specified, the check is disabled for every instance of that cell.} \
  -arg_help {
    objects {A list of Liberty cells, instances, ports, or pins.}
  }

proc set_disable_clock_gating_check { objects } {
  set_disable_clock_gating_check_cmd $objects
}

proc set_disable_clock_gating_check_cmd { objects } {
  set libcells {}
  set insts {}
  set ports {}
  set pins {}
  get_object_args $objects {} libcells {} {} insts ports pins {} {} {}
  foreach lc $libcells {
    disable_clock_gating_check_lib_cell $lc
  }
  foreach inst $insts {
    disable_clock_gating_check_inst $inst
  }
  foreach port $ports {
    disable_clock_gating_check_pin [get_port_pin $port]
  }
  foreach pin $pins {
    disable_clock_gating_check_pin $pin
  }
}

################################################################

define_cmd_args "unset_disable_clock_gating_check" { objects } \
  -help {The `unset_disable_clock_gating_check` command removes a previous `set_disable_clock_gating_check`.} \
  -arg_help {
    objects {A list of Liberty cells, instances, ports, or pins.}
  }

proc unset_disable_clock_gating_check { objects } {
  unset_disable_clock_gating_check_cmd $objects
}

proc unset_disable_clock_gating_check_cmd { objects } {
  set libcells {}
  set insts {}
  set ports {}
  set pins {}
  get_object_args $objects {} libcells {} {} insts ports pins {} {} {}
  foreach lc $libcells {
    unset_disable_clock_gating_check_lib_cell $lc
  }
  foreach inst $insts {
    unset_disable_clock_gating_check_inst $inst
  }
  foreach port $ports {
    unset_disable_clock_gating_check_pin [get_port_pin $port]
  }
  foreach pin $pins {
    unset_disable_clock_gating_check_pin $pin
  }
}

################################################################

# deprecated 2026-08-18
define_cmd_args "set_disable_inferred_clock_gating" { objects } \
  -help {The `set_disable_inferred_clock_gating` command is deprecated. Use `set_disable_clock_gating_check` instead.} \
  -arg_help {
    objects {A list of Liberty cells, instances, ports, or pins.}
  }

proc set_disable_inferred_clock_gating { objects } {
  sta_warn 528 "set_disable_inferred_clock_gating is deprecated. Use set_disable_clock_gating_check."
  set_disable_clock_gating_check_cmd $objects
}

################################################################

# deprecated 2026-08-18
define_cmd_args "unset_disable_inferred_clock_gating" { objects } \
  -help {The `unset_disable_inferred_clock_gating` command is deprecated. Use `unset_disable_clock_gating_check` instead.} \
  -arg_help {
    objects {A list of Liberty cells, instances, ports, or pins.}
  }

proc unset_disable_inferred_clock_gating { objects } {
  sta_warn 529 "unset_disable_inferred_clock_gating is deprecated. Use unset_disable_clock_gating_check."
  unset_disable_clock_gating_check_cmd $objects
}

################################################################

# max slew slack / limit
proc max_slew_check_slack_limit {} {
  return [expr "[max_slew_check_slack] / [max_slew_check_limit]"]
}

# max cap slack / limit
proc max_capacitance_check_slack_limit {} {
  return [expr [max_capacitance_check_slack] / [max_capacitance_check_limit]]
}

# max fanout slack / limit
proc max_fanout_check_slack_limit {} {
  return [expr [max_fanout_check_slack] / [max_fanout_check_limit]]
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
