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

# Liberty commands.

namespace eval sta {

define_cmd_args "read_liberty" \
  {[-corner corner_name] [-min] [-max] [-no_latch_infer] filename}

proc read_liberty { args } {
  parse_key_args "read_liberty" args keys {-corner} \
    flags {-min -max -no_latch_infer}
  check_argc_eq1 "read_liberty" $args

  set filename [file nativename $args]
  set corner [parse_corner keys]
  set min_max [parse_min_max_all_flags flags]
  set infer_latches [expr ![info exists flags(-no_latch_infer)]]
  read_liberty_cmd $filename $corner $min_max $infer_latches
}

################################################################

define_hidden_cmd_args "report_lib_cell_power" {lib_cell}

proc report_lib_cell_power { args } {
  global sta_report_default_digits

  check_argc_eq3 "report_internal_power" $args
  set cells [get_lib_cells [lindex $args 0]]
  set slew [time_ui_sta [lindex $args 1]]
  set cap [capacitance_ui_sta [lindex $args 2]]

  foreach cell $cells {
    puts "[$cell name] Leakage Power"
    set leakage_iter [$cell leakage_power_iterator]
    while {[$leakage_iter has_next]} {
      set leakage [$leakage_iter next]
      puts "[format_power [$leakage power] 5] [$leakage when]"
    }
    $leakage_iter finish

    puts "[$cell name] Internal Power"
    set internal_iter [$cell internal_power_iterator]
    while {[$internal_iter has_next]} {
      set internal [$internal_iter next]
      set port_name [[$internal port] name]
      set related_port [$internal related_port]
      if { $related_port != "NULL" } {
	set related_port_name [$related_port name]
      } else {
	set related_port_name ""
      }
      set digits $sta_report_default_digits
      set rise [format_power [$internal power rise $slew $cap] $digits]
      set fall [format_power [$internal power fall $slew $cap] $digits]
      puts "$port_name $related_port_name [rise_short_name] $rise [fall_short_name] $fall [$internal when]"
    }
    $internal_iter finish
  }
}

# sta namespace end
}
