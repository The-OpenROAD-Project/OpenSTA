# OpenSTA, Static Timing Analyzer
# Copyright (c) 2018, Parallax Software, Inc.
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
      [-corner corner_name]]\
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
    set insts [get_instance_error "-cell" $keys(-instances)]
    foreach inst $insts {
      report_power_inst $inst $corner $digits
    }
  } else {
    report_power_design $corner $digits
  }
}

proc report_power_design { corner digits } {
  set power_result [design_power $corner]
  puts "Group                 Internal Switching   Leakage     Total"
  puts "                         Power     Power     Power     Power (mW)"
  puts "-------------------------------------------------------------------"
  set design_total [lindex $power_result 3]
  report_power_row "Sequential"    $power_result  4 $design_total $digits
  report_power_row "Combinational" $power_result  8 $design_total $digits
  report_power_row "Macro"         $power_result 12 $design_total $digits
  report_power_row "Pad"           $power_result 16 $design_total $digits
  puts "-------------------------------------------------------------------"
  report_power_row "Total" $power_result 0 $design_total $digits

  puts -nonewline "                    "
  report_power_col_percent $power_result 0
  report_power_col_percent $power_result 1
  report_power_col_percent $power_result 2
  puts ""
}

proc report_power_row { type power_result index design_total digits } {
  set internal [lindex $power_result [expr $index + 0]]
  set switching [lindex $power_result [expr $index + 1]]
  set leakage [lindex $power_result [expr $index + 2]]
  set total [lindex $power_result [expr $index + 3]]
  set percent [expr $total / $design_total * 100]
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

proc report_power_col_percent { power_result index } {
  set total [lindex $power_result 3]
  set col [lindex $power_result $index]
  puts -nonewline [format "%9.1f%%" [expr $col / $total * 100]]
}

proc report_power_inst { inst corner digits } {
  puts "Instance: [$inst path_name]"
  puts "Cell: [[$inst liberty_cell] name]"
  puts "Liberty file: [[[$inst liberty_cell] liberty_library] filename]"
  set power_result [instance_power $inst $corner]
  set switching [lindex $power_result 0]
  set internal [lindex $power_result 1]
  set leakage [lindex $power_result 2]
  set total [lindex $power_result 3]
  report_power_line "Switching power" $switching $digits
  report_power_line "Internal power" $internal $digits
  report_power_line "Leakage power" $leakage $digits
  report_power_line "Total power" $total $digits
}

proc report_power_line { type pwr digits } {
  puts [format "%-16s %.${digits}fmW" $type [expr $pwr * 1e+3]]
}

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
