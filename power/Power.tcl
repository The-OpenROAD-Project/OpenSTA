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
      [-corner corner]\
      [-digits digits]\
      [-format format]\
      [> filename] [>> filename] }

proc_redirect report_power {
  global sta_report_default_digits

  parse_key_args "report_power" args \
    keys {-instances -highest_power_instances -corner -digits -format} flags {}

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
  set corner [parse_corner keys]

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
      report_power_insts_json $insts $corner $digits
    } else {
      report_power_insts $insts $corner $digits
    }
  } elseif { [info exists keys(-highest_power_instances)] } {
    set count $keys(-highest_power_instances)
    check_positive_integer "-highest_power_instances" $count
    set insts [highest_power_instances $count $corner]
    if { $format == "json" } {
      report_power_insts_json $insts $corner $digits
    } else {
      report_power_insts $insts $corner $digits
    }
  } else {
    if { $format == "json" } {
      report_power_design_json $corner $digits
    } else {
      report_power_design $corner $digits
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

proc report_power_design { corner digits } {
  set power_result [design_power $corner]
  set totals        [lrange $power_result  0  3]
  set sequential    [lrange $power_result  4  7]
  set combinational [lrange $power_result  8 11]
  set clock         [lrange $power_result 12 15]
  set macro         [lrange $power_result 16 19]
  set pad           [lrange $power_result 20 end]
  lassign $totals design_internal design_switching design_leakage design_total

  set field_width [max [expr $digits + 6] 10]
  report_power_title5       "Group" "Internal" "Switching" "Leakage" "Total" $field_width
  report_power_title5_units "     " "Power"    "Power"     "Power"   "Power" "(Watts)" $field_width
  report_title_dashes5 $field_width
  report_power_row "Sequential"    $sequential    $design_total $field_width $digits
  report_power_row "Combinational" $combinational $design_total $field_width $digits
  report_power_row "Clock"         $clock         $design_total $field_width $digits
  report_power_row "Macro"         $macro         $design_total $field_width $digits
  report_power_row "Pad"           $pad           $design_total $field_width $digits
  report_title_dashes5 $field_width
  report_power_row "Total" $power_result $design_total $field_width $digits

  report_line "[format %-20s {}][power_col_percent $design_internal  $design_total $field_width][power_col_percent $design_switching $design_total $field_width][power_col_percent $design_leakage $design_total $field_width]"
}

proc report_power_design_json { corner digits } {
  set power_result [design_power $corner]
  set totals        [lrange $power_result  0  3]
  set sequential    [lrange $power_result  4  7]
  set combinational [lrange $power_result  8 11]
  set clock         [lrange $power_result 12 15]
  set macro         [lrange $power_result 16 19]
  set pad           [lrange $power_result 20 end]

  report_line "\{"
  report_power_row_json "Sequential" $sequential $digits ","
  report_power_row_json "Combinational" $combinational $digits ","
  report_power_row_json "Clock" $clock $digits ","
  report_power_row_json "Macro" $macro $digits ","
  report_power_row_json "Pad" $pad $digits ","
  report_power_row_json "Total" $totals $digits ""
  report_line "\}"
}

proc report_power_row_json { name row_result digits separator } {
  lassign $row_result internal switching leakage total
  report_line "  \"$name\": \{"
  report_line "    \"internal\": [format %.${digits}e $internal],"
  report_line "    \"switching\": [format %.${digits}e $switching],"
  report_line "    \"leakage\": [format %.${digits}e $leakage],"
  report_line "    \"total\": [format %.${digits}e $total]"
  report_line "  \}$separator"
}

proc max { x y } {
  if { $x >= $y } {
    return $x
  } else {
    return $y
  }
}

proc report_power_title5 { title1 title2 title3 title4 title5 field_width } {
  report_line "[format %-20s $title1] [format %${field_width}s $title2] [format %${field_width}s $title3] [format %${field_width}s $title4] [format %${field_width}s $title5]"
}

proc report_power_title5_units { title1 title2 title3 title4 title5 units field_width } {
  report_line "[format %-20s $title1] [format %${field_width}s $title2] [format %${field_width}s $title3] [format %${field_width}s $title4] [format %${field_width}s $title5] $units"
}

proc report_power_title4 { title1 title2 title3 title4 field_width } {
  report_line " [format %${field_width}s $title1] [format %${field_width}s $title2] [format  %${field_width}s $title3] [format %${field_width}s $title4]"
}

proc report_power_title4_units { title1 title2 title3 title4 units field_width } {
  report_line " [format %${field_width}s $title1] [format %${field_width}s $title2] [format  %${field_width}s $title3] [format %${field_width}s $title4] $units"
}

proc report_title_dashes5 { field_width } {
  set count [expr 20 + ($field_width + 1) * 4]
  report_title_dashes $count
}

proc report_title_dashes4 { field_width } {
  set count [expr ($field_width + 1) * 4]
  report_title_dashes $count
}

proc report_title_dashes { count } {
  set line ""
  for {set i 0} {$i < $count} {incr i} {
    set line "-$line"
  }
  report_line $line
}

proc report_power_row { type row_result design_total field_width digits } {
  lassign $row_result internal switching leakage total
  if { $design_total == 0.0 || [is_nan $design_total] } {
    set percent 0.0
  } else {
    set percent [expr $total / $design_total * 100]
  }
  report_line "[format %-20s $type][power_col $internal $field_width $digits][power_col $switching $field_width $digits][power_col $leakage $field_width $digits][power_col $total $field_width $digits] [format %5.1f $percent]%"
}

proc is_nan { str } {
  return  [string match "*NaN" $str]
}

proc power_col { pwr field_width digits } {
  if { [is_nan $pwr] } {
    format " %${field_width}s" $pwr
  } else {
    format " %$field_width.${digits}e" $pwr
  }
}

proc power_col_percent { col_total total field_width } {
  if { $total == 0.0 || [is_nan $total]} {
    set percent 0.0
  } else {
    set percent [expr $col_total / $total * 100]
  }
  format "%$field_width.1f%%" $percent
}

proc report_power_line { type pwr digits } {
  if { [is_nan $pwr] } {
    report_line [format "%-16s %s" $type $pwr]
  } else {
    report_line [format "%-16s %.${digits}e" $type $pwr]
  }
}

proc report_power_insts { insts corner digits } {
  set inst_pwrs {}
  foreach inst $insts {
    set power_result [instance_power $inst $corner]
    lappend inst_pwrs [list $inst $power_result]
  }
  set inst_pwrs [lsort -command inst_pwr_cmp $inst_pwrs]

  set field_width [max [expr $digits + 6] 10]

  report_power_title4       "Internal" "Switching" "Leakage" "Total" $field_width
  report_power_title4_units "Power"    "Power"     "Power"   "Power" "(Watts)" $field_width
  report_title_dashes4 $field_width

  foreach inst_pwr $inst_pwrs {
    set inst [lindex $inst_pwr 0]
    set power [lindex $inst_pwr 1]
    report_power_inst $inst $power $field_width $digits
  }
}

proc report_power_insts_json { insts corner digits } {
  set inst_pwrs {}
  foreach inst $insts {
    set power_result [instance_power $inst $corner]
    lappend inst_pwrs [list $inst $power_result]
  }
  set inst_pwrs [lsort -command inst_pwr_cmp $inst_pwrs]

  report_line "\["
  set first 1
  foreach inst_pwr $inst_pwrs {
    set inst [lindex $inst_pwr 0]
    set power [lindex $inst_pwr 1]
    if { !$first } {
      report_line ","
    }
    set first 0
    report_power_inst_json $inst $power $digits
  }
  report_line "\]"
}

proc report_power_inst_json { inst power digits } {
  lassign $power internal switching leakage total
  set inst_name [get_full_name $inst]
  report_line "\{"
  report_line "  \"name\": \"$inst_name\","
  report_line "  \"internal\": [format %.${digits}e $internal],"
  report_line "  \"switching\": [format %.${digits}e $switching],"
  report_line "  \"leakage\": [format %.${digits}e $leakage],"
  report_line "  \"total\": [format %.${digits}e $total]"
  report_line "\}"
}

proc inst_pwr_cmp { inst_pwr1 inst_pwr2 } {
  set pwr1 [lindex $inst_pwr1 1]
  set pwr2 [lindex $inst_pwr2 1]
  lassign $pwr1 internal1 switching1 leakage1 total1
  lassign $pwr2 internal2 switching2 leakage2 total2
  if { $total1 < $total2 } {
    return 1
  } elseif { $total1 == $total2 } {
    return 0
  } else {
    return -1
  }
}

proc report_power_inst { inst power_result field_width digits } {
  lassign $power_result internal switching leakage total
  report_line "[power_col $internal $field_width $digits][power_col $switching $field_width $digits][power_col $leakage $field_width $digits][power_col $total $field_width $digits] [get_full_name $inst]"
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
    set density [expr $activity / [clock_min_period]]
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
      if { [get_property $port "direction"] == "input" || [get_property $port "direction"] == "in" } {
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

define_cmd_args "read_vcd" { [-scope scope] filename }

proc read_vcd { args } {
  parse_key_args "read_vcd" args \
    keys {-scope} flags {}

  check_argc_eq1 "read_vcd" $args
  set filename [file nativename [lindex $args 0]]
  set scope ""
  if { [info exists keys(-scope)] } {
    set scope $keys(-scope)
  }
  read_vcd_file $filename $scope
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
  set corner [cmd_corner]
  foreach inst [network_leaf_instances] {
    set power_result [instance_power $inst $corner]
    lassign $power_result internal switching leakage total
    if { [is_nan $internal] || [is_nan $switching] || [is_nan $leakage] } {
      report_line "[get_full_name $inst] $internal $switching $leakage"
      break
    }
  }
}

# sta namespace end.
}
