# OpenSTA, Static Timing Analyzer
# Copyright (c) 2019, Parallax Software, Inc.
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
# Power commands.
#
################################################################

namespace eval sta {

define_cmd_args "report_power" \
  { [-instances instances]\
      [-corner corner_name]\
      [-digits digits]\
      [> filename] [>> filename] }

proc_redirect report_power {
  global sta_report_default_digits

  parse_key_args "report_power" args keys {-instances -corner -digits} flags {} 1

  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  } else {
    set digits $sta_report_default_digits
  }
  set corner [parse_corner keys]

  if { [info exists keys(-instances)] } {
    set insts [get_instances_error "-instances" $keys(-instances)]
    report_power_insts $insts $corner $digits
  } else {
    report_power_design $corner $digits
  }
}

proc report_power_design { corner digits } {
  set power_result [design_power $corner]
  set totals        [lrange $power_result  0  3]
  set sequential    [lrange $power_result  4  7]
  set combinational [lrange $power_result  8 11]
  set macro         [lrange $power_result 12 15]
  set pad           [lrange $power_result 16 end]
  lassign $totals design_internal design_switching design_leakage design_total

  puts "Group                 Internal Switching   Leakage     Total"
  puts "                         Power     Power     Power     Power (mW)"
  puts "-------------------------------------------------------------------"
  report_power_row "Sequential"    $sequential    $design_total $digits
  report_power_row "Combinational" $combinational $design_total $digits
  report_power_row "Macro"         $macro         $design_total $digits
  report_power_row "Pad"           $pad           $design_total $digits
  puts "-------------------------------------------------------------------"
  report_power_row "Total" $power_result $design_total $digits

  puts -nonewline "                    "
  report_power_col_percent $design_internal  $design_total
  report_power_col_percent $design_switching $design_total
  report_power_col_percent $design_leakage   $design_total
  puts ""
}

proc report_power_row { type row_result design_total digits } {
  lassign $row_result internal switching leakage total
  if { $design_total == 0.0 } {
    set percent 0.0
  } else {
    set percent [expr $total / $design_total * 100]
  }
  puts -nonewline [format "%-20s" $type]
  report_power_col $internal $digits
  report_power_col $switching $digits
  report_power_col $leakage $digits
  report_power_col $total $digits
  puts [format " %5.1f%%" $percent]
}

proc report_power_col { pwr digits } {
  puts -nonewline [format "%10.${digits}f" [expr $pwr * 1e+3]]
}

proc report_power_col_percent { col_total total } {
  if { $total == 0.0 } {
    set percent 0.0
  } else {
    set percent [expr $col_total / $total * 100]
  }
  puts -nonewline [format "%9.1f%%" $percent]
}

proc report_power_line { type pwr digits } {
  puts [format "%-16s %.${digits}fmW" $type [expr $pwr * 1e+3]]
}

proc report_power_insts { insts corner digits } {
  set inst_pwrs {}
  foreach inst $insts {
    set power_result [instance_power $inst $corner]
    lappend inst_pwrs [list $inst $power_result]
  }
  set inst_pwrs [lsort -command inst_pwr_cmp $inst_pwrs]

  puts "  Internal Switching   Leakage     Total Instance"
  puts "     Power     Power     Power     Power (mW)"
  puts "-------------------------------------------------------------------"

  foreach inst_pwr $inst_pwrs {
    set inst [lindex $inst_pwr 0]
    set power [lindex $inst_pwr 1]
    report_power_inst $inst $power $digits
  }
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

proc report_power_inst { inst power_result digits } {
  lassign $power_result internal switching leakage total
  report_power_col $internal $digits
  report_power_col $switching $digits
  report_power_col $leakage $digits
  report_power_col $total $digits
  puts " [get_full_name $inst]"
}

################################################################

set ::power_default_signal_toggle_rate 0.1

trace variable ::power_default_signal_toggle_rate "rw" \
  sta::trace_power_default_signal_toggle_rate

proc trace_power_default_signal_toggle_rate { name1 name2 op } {
  global power_default_signal_toggle_rate

  if { $op == "r" } {
    set power_default_signal_toggle_rate [power_default_signal_toggle_rate]
  } elseif { $op == "w" } {
    if { [string is double $power_default_signal_toggle_rate] \
	   && $power_default_signal_toggle_rate >= 0.0 } {
      set_power_default_signal_toggle_rate $power_default_signal_toggle_rate 
    } else {
      sta_error "power_default_signal_toggle_rate must be a positive float."
    }
  }
}

# sta namespace end.
}
