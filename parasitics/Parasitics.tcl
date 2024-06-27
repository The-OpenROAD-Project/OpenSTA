# OpenSTA, Static Timing Analyzer
# Copyright (c) 2024, Parallax Software, Inc.
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

define_cmd_args "read_spef" \
  {[-corner corner]\
     [-min]\
     [-max]\
     [-path path]\
     [-pin_cap_included]\
     [-keep_capacitive_coupling]\
     [-coupling_reduction_factor factor]\
     [-reduce]\
     [-delete_after_reduce]\
     filename}

proc_redirect read_spef {
  parse_key_args "read_spef" args \
    keys {-path -coupling_reduction_factor -reduce_to -corner} \
    flags {-min -max -increment -pin_cap_included -keep_capacitive_coupling \
	     -reduce -delete_after_reduce -quiet -save}
  check_argc_eq1 "read_spef" $args

  set reduce [info exists flags(-reduce)]
  if { [info exists flags(-quiet)] } {
    # deprecated 2024-02-08
    sta_warn 272 "read_spef -quiet is deprecated."
  }
  if { [info exists keys(-reduce_to)] } {
    # deprecated 2024-02-08
    sta_warn 273 "read_spef -reduce_to is deprecated. Use -reduce instead."
    set reduce 1
  }
  if { [info exists flags(-delete_after_reduce)] } {
    # deprecated 2024-02-08
    sta_warn 274 "read_spef -delete_after_reduce is deprecated."
  }
  if { [info exists flags(-save)] } {
    # deprecated 2024-02-08
    sta_warn 275 "read_spef -save is deprecated."
  }

  set instance [top_instance]
  if [info exists keys(-path)] {
    set path $keys(-path)
    set instance [find_instance $path]
    if { $instance == "NULL" } {
      sta_error 276 "path instance '$path' not found."
    }
  }
  set corner [parse_corner_or_all keys]
  set min_max [parse_min_max_all_flags flags]
  set coupling_reduction_factor 1.0
  if [info exists keys(-coupling_reduction_factor)] {
    set coupling_reduction_factor $keys(-coupling_reduction_factor)
    check_positive_float "-coupling_reduction_factor" $coupling_reduction_factor
  }
  set keep_coupling_caps [info exists flags(-keep_capacitive_coupling)]
  set pin_cap_included [info exists flags(-pin_cap_included)]

  set filename [file nativename [lindex $args 0]]
  return [read_spef_cmd $filename $instance $corner $min_max \
	    $pin_cap_included $keep_coupling_caps \
            $coupling_reduction_factor $reduce]
}

define_cmd_args "report_parasitic_annotation" {-report_unannotated}

proc_redirect report_parasitic_annotation {
  parse_key_args "report_parasitic_annotation" args \
    keys {} flags {-report_unannotated}
  check_argc_eq0 "report_parasitic_annotation" $args

  set report_unannotated [info exists flags(-report_unannotated)]
  report_parasitic_annotation_cmd $report_unannotated [sta::cmd_corner]
}

# set_pi_model [-min] [-max] drvr_pin c2 rpi c1
proc set_pi_model { args } {
  parse_key_args "set_pi_model" args keys {} flags {-max -min}
  check_argc_eq4 "set_pi_model" $args

  set drvr_pin_name [lindex $args 0]

  set c2 [lindex $args 1]
  check_positive_float "c2" $c2
  set c2 [capacitance_ui_sta $c2]

  set rpi [lindex $args 2]
  check_positive_float "Rpi" $rpi
  set rpi [resistance_ui_sta $rpi]

  set c1 [lindex $args 3]
  check_positive_float "c1" $c1
  set c1 [capacitance_ui_sta $c1]

  set min_max [parse_min_max_all_check_flags flags]

  set drvr_pin [get_port_pin_error "drvr_pin" $drvr_pin_name]
  set_pi_model_cmd $drvr_pin "rise" $min_max $c2 $rpi $c1
  set_pi_model_cmd $drvr_pin "fall" $min_max $c2 $rpi $c1
}

# set_elmore [-min] [-max] drvr_pin_name load_pin_name elmore
proc set_elmore { args } {
  parse_key_args "set_elmore" args keys {} flags {-min -max}
  check_argc_eq3  "set_elmore" $args

  set drvr_pin_arg [lindex $args 0]
  set drvr_pin [get_port_pin_error "drvr_pin" $drvr_pin_arg]
  set load_pin_arg [lindex $args 1]
  set load_pin [get_port_pin_error "load_pin" $load_pin_arg]
  set elmore [lindex $args 2]
  check_positive_float "elmore delay" $elmore
  set elmore [time_ui_sta $elmore]
  set min_max [parse_min_max_all_check_flags flags]

  set_elmore_cmd $drvr_pin $load_pin "rise" $min_max $elmore
  set_elmore_cmd $drvr_pin $load_pin "fall" $min_max $elmore
}

# sta namespace end
}
