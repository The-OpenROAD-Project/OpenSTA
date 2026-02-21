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

################################################################
#
# Power commands.
#
################################################################

namespace eval sta {

define_cmd_args "report_power" \
  { [-instances instances]\
      [-highest_power_instances count]\
      [-scene scene]\
      [-digits digits]\
      [-format format]\
      [> filename] [>> filename] }

proc_redirect report_power {
  global sta_report_default_digits

  parse_key_args "report_power" args \
    keys {-instances -highest_power_instances -corner -scene -format -digits}\
    flags {}

  check_argc_eq0 "report_power" $args

  if { ![liberty_libraries_exist] } {
    sta_error 304 "No liberty libraries have been read."
  }
  if { [info exists keys(-digits)] } {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  set scene [parse_scene keys]

  if { [info exists keys(-format)] } {
    set format $keys(-format)
    if { $format != "text" && $format != "json" } {
      sta_error 311 "unknown power report -format $format"
    }
  } else {
    set format "text"
  }

  if { [info exists keys(-instances)] } {
    set insts [get_instances_error "-instances" $keys(-instances)]
    if { $format == "json" } {
      report_power_insts_json $insts $scene $digits
    } else {
      report_power_insts $insts $scene $digits
    }
  } elseif { [info exists keys(-highest_power_instances)] } {
    set count $keys(-highest_power_instances)
    check_positive_integer "-highest_power_instances" $count
    set insts [highest_power_instances $count $scene]
    if { $format == "json" } {
      report_power_insts_json $insts $scene $digits
    } else {
      report_power_insts $insts $scene $digits
    }
  } else {
    if { $format == "json" } {
      report_power_design_json $scene $digits
    } else {
      report_power_design $scene $digits
    }
  }
}

proc liberty_libraries_exist {} {
  set lib_iter [liberty_library_iterator]
  set have_liberty 0
  if { [$lib_iter has_next] } {
    set have_liberty 1
  }
  $lib_iter finish
  return $have_liberty
}

################################################################

define_cmd_args "set_power_activity" { [-global]\
                                         [-input]\
                                         [-input_ports ports]\
                                         [-pins pins]\
                                         [-activity activity | -density density]\
                                         [-duty duty]\
                                         [-clock clock]}

proc set_power_activity { args } {
  parse_key_args "set_power_activity" args \
    keys {-input_ports -pins -activity -density -duty -clock} \
    flags {-global -input}

  check_argc_eq0 "set_power_activity" $args
  if { [info exists keys(-activity)] && [info exists keys(-density)] \
         || ![info exists keys(-activity)] && ![info exists keys(-density)] } {
    sta_error 306 "Specify -activity or -density."
  }

  set density 0.0
  if { [info exists keys(-activity)] } {
    set activity $keys(-activity)
    check_positive_float "activity" $activity
    if { [info exists keys(-clock)] } {
      set clk [get_clock_warn "-clock" $keys(-clock)]
    } else {
      set clks [get_clocks]
      if { $clks == {} } {
        sta_error 307 "-activity requires a clock to be defined"
      }
    }
    set density [expr $activity / [clock_min_period [sta::cmd_mode]]]
  }

  if { [info exists keys(-density)] } {
    set density $keys(-density)
    check_positive_float "density" $density
    set density [expr $density / [time_ui_sta 1.0]]
    if { [info exists keys(-clock)] } {
      sta_warn 308 "-clock ignored for -density"
    }
  }
  set duty 0.5
  if { [info exists keys(-duty)] } {
    set duty $keys(-duty)
    check_float "duty" $duty
    if { $duty < 0.0 || $duty > 1.0 } {
      sta_error 309 "duty should be 0.0 to 1.0"
    }
  }

  if { [info exists flags(-global)] } {
    set_power_global_activity $density $duty
  }
  if { [info exists flags(-input)] } {
    set_power_input_activity $density $duty
  }
  if { [info exists keys(-input_ports)] } {
    set ports [get_ports_error "input_ports" $keys(-input_ports)]
    foreach port $ports {
      if { [get_property $port "direction"] == "input" } {
        if { [is_clock_src [sta::get_port_pin $port]] } {
          sta_warn 310 "activity cannot be set on clock ports."
        } else {
          set_power_input_port_activity $port $density $duty
        }
      }
    }
  }
  if { [info exists keys(-pins)] } {
    set pins [get_pins $keys(-pins)]
    foreach pin $pins {
      set_power_pin_activity $pin $density $duty
    }
  }
}

################################################################

define_cmd_args "unset_power_activity" { [-global]\
                                           [-input]\
                                           [-input_ports ports]\
                                           [-pins pins]\
                                           [-clock clock]}

proc unset_power_activity { args } {
  parse_key_args "unset_power_activity" args \
    keys {-input_ports -pins -clock} \
    flags {-global -input}

  check_argc_eq0 "unset_power_activity" $args

  if { [info exists flags(-global)] } {
    unset_power_global_activity
  }
  if { [info exists flags(-input)] } {
    unset_power_input_activity
  }
  if { [info exists keys(-input_ports)] } {
    set ports [get_ports_error "input_ports" $keys(-input_ports)]
    foreach port $ports {
      if { [get_property $port "direction"] == "input" } {
        if { [is_clock_src [sta::get_port_pin $port]] } {
          sta_warn 303 "activity cannot be set on clock ports."
        } else {
          unset_power_input_port_activity $port
        }
      }
    }
  }
  if { [info exists keys(-pins)] } {
    set pins [get_pins $keys(-pins)]
    foreach pin $pins {
      unset_power_pin_activity $pin
    }
  }
}

################################################################

# Deprecated 9/2024
define_cmd_args "read_power_activities" { [-scope scope] -vcd filename }

proc read_power_activities { args } {
  parse_key_args "read_power_activities" args \
    keys {-scope} flags {-vcd}

  check_argc_eq1 "set_power_activity" $args
  set filename [file nativename [lindex $args 0]]
  set scope ""
  if { [info exists keys(-scope)] } {
    set scope $keys(-scope)
  }
  sta_warn 305 "read_power_activities is deprecated. Use read_vcd."
  read_vcd_file $filename $scope
}

################################################################

define_cmd_args "read_vcd" { [-scope scope] [-mode mode_name] filename }

proc read_vcd { args } {
  parse_key_args "read_vcd" args \
    keys {-scope -mode_name} flags {}

  check_argc_eq1 "read_vcd" $args
  set filename [file nativename [lindex $args 0]]
  set scope ""
  if { [info exists keys(-scope)] } {
    set scope $keys(-scope)
  }
  set mode_name [sta::cmd_mode]
  if { [info exists keys(-mode)] } {
    set mode_name $keys(-mode)
  }
  read_vcd_file $filename $scope $mode_name
}

################################################################

define_cmd_args "read_saif" { [-scope scope] filename }

proc read_saif { args } {
  parse_key_args "read_saif" args keys {-scope} flags {}

  check_argc_eq1 "read_saif" $args
  set filename [file nativename [lindex $args 0]]
  set scope ""
  if { [info exists keys(-scope)] } {
    set scope $keys(-scope)
  }
  read_saif_file $filename $scope
}

################################################################

define_cmd_args "report_activity_annotation" { [-report_unannotated] \
                                                 [-report_annotated] }

proc_redirect report_activity_annotation {
  parse_key_args "report_activity_annotation" args \
    keys {} flags {-report_unannotated -report_annotated}
  check_argc_eq0 "report_activity_annotation" $args
  set report_unannotated [info exists flags(-report_unannotated)]
  set report_annotated [info exists flags(-report_annotated)];
  report_activity_annotation_cmd $report_unannotated $report_annotated
}

################################################################

proc power_find_nan { } {
  set scene [cmd_scene]
  foreach inst [network_leaf_instances] {
    set power_result [instance_power $inst $scene]
    lassign $power_result internal switching leakage total
    if { [is_nan $internal] || [is_nan $switching] || [is_nan $leakage] } {
      report_line "[get_full_name $inst] $internal $switching $leakage"
      break
    }
  }
}

proc is_nan { str } {
  return  [string match "*NaN" $str]
}

# sta namespace end.
}
