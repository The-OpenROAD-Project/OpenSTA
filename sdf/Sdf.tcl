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

define_cmd_args "read_sdf" \
  {[-path path] [-corner corner]\
     [-cond_use min|max|min_max]\
     [-unescaped_dividers] filename}

proc_redirect read_sdf {
  parse_key_args "read_sdf" args \
    keys {-path -corner -cond_use -analysis_type} \
    flags {-unescaped_dividers -incremental_only}

  check_argc_eq1 "read_sdf" $args
  set filename [file nativename [lindex $args 0]]
  set path ""
  if [info exists keys(-path)] {
    set path $keys(-path)
  }
  set corner [parse_corner keys]

  set cond_use "NULL"
  if [info exists keys(-cond_use)] {
    set cond_use $keys(-cond_use)
    if { $cond_use != "min" && $cond_use != "max" && $cond_use != "min_max" } {
      sta_warn 620 "-cond_use must be min, max or min_max."
      set cond_use "NULL"
    }
    if { $cond_use == "min_max" \
	   && { [operating_condition_analysis_type] == "single" }} {
      sta_error 621 "-cond_use min_max cannot be used with analysis type single."
    }
  }

  set unescaped_dividers [info exists flags(-unescaped_dividers)]
  set incremental_only [info exists flags(-incremental_only)]
  read_sdf_file $filename $path $corner $unescaped_dividers \
    $incremental_only $cond_use
}

################################################################

define_cmd_args "report_annotated_delay" \
  {[-cell] [-net] [-from_in_ports] [-to_out_ports] [-max_lines liness]\
     [-list_annotated] [-list_not_annotated] [-constant_arcs]}

proc_redirect report_annotated_delay {
  parse_key_args "report_annotated_delay" args keys {-max_lines} \
    flags {-cell -net -from_in_ports -to_out_ports -list_annotated \
	     -list_not_annotated -constant_arcs}
  if { [info exists flags(-cell)] || [info exists flags(-net)] \
	 || [info exists flags(-from_in_ports)] \
	 || [info exists flags(-to_out_ports)] } {
    set report_cells [info exists flags(-cell)]
    set report_nets [info exists flags(-net)]
    set report_in_nets [info exists flags(-from_in_ports)]
    set report_out_nets [info exists flags(-to_out_ports)]
  } else {
    set report_cells 1
    set report_nets 1
    set report_in_nets 1
    set report_out_nets 1
  }

  set max_lines 0
  if { [info exists keys(-max_lines)] } {
    set max_lines $keys(-max_lines)
    check_positive_integer "-max_lines" $max_lines
  }

  report_annotated_delay_cmd $report_cells $report_nets \
    $report_in_nets $report_out_nets \
    $max_lines [info exists flags(-list_annotated)] \
    [info exists flags(-list_not_annotated)] \
    [info exists flags(-constant_arcs)]
}

define_cmd_args "report_annotated_check" \
  {[-setup] [-hold] [-recovery] [-removal] [-nochange] [-width] [-period]\
     [-max_skew] [-max_lines liness] [-list_annotated] [-list_not_annotated]\
     [-constant_arcs]}

proc_redirect report_annotated_check {
  parse_key_args "report_annotated_check" args keys {-max_lines} \
    flags {-setup -hold -recovery -removal -nochange -width -period \
	     -max_skew -list_annotated -list_not_annotated -constant_arcs}
  if { [info exists flags(-setup)] || [info exists flags(-hold)] \
	 || [info exists flags(-recovery)] || [info exists flags(-removal)] \
	 || [info exists flags(-nochange)] || [info exists flags(-width)] \
	 || [info exists flags(-period)] || [info exists flags(-max_skew)] } {
    set report_setup [info exists flags(-setup)]
    set report_hold [info exists flags(-hold)]
    set report_recovery [info exists flags(-recovery)]
    set report_removal [info exists flags(-removal)]
    set report_nochange [info exists flags(-nochange)]
    set report_width [info exists flags(-width)]
    set report_period [info exists flags(-period)]
    set report_max_skew [info exists flags(-max_skew)]
  } else {
    set report_setup 1
    set report_hold 1
    set report_recovery 1
    set report_removal 1
    set report_nochange 1
    set report_width 1
    set report_period 1
    set report_max_skew 1
  }

  set max_lines 0
  if { [info exists keys(-max_lines)] } {
    set max_lines $keys(-max_lines)
    check_positive_integer "-max_lines" $max_lines
  }

  report_annotated_check_cmd $report_setup $report_hold \
    $report_recovery $report_removal $report_nochange \
    $report_width $report_period $report_max_skew \
    $max_lines [info exists flags(-list_annotated)] \
    [info exists flags(-list_not_annotated)] \
    [info exists flags(-constant_arcs)]
}

define_cmd_args "write_sdf" \
  {[-corner corner] [-divider /|.] [-include_typ]\
     [-digits digits] [-gzip] [-no_timestamp] [-no_version] filename}

proc_redirect write_sdf {
  parse_key_args "write_sdf" args \
    keys {-corner -divider -digits -significant_digits} \
    flags {-include_typ -gzip -no_timestamp -no_version}
  check_argc_eq1 "write_sdf" $args
  set corner [parse_corner keys]
  set filename [file nativename [lindex $args 0]]
  set divider "/"
  if [info exists keys(-divider)] {
    set divider $keys(-divider)
    if { !($divider == "/" || $divider == ".") } {
      sta_error 623 "SDF -divider must be / or ."
    }
  }
  set digits 3
  if [info exists keys(-digits)] {
    set digits $keys(-digits)
    check_positive_integer "-digits" $digits
  }

  set include_typ [info exists flags(-include_typ)]
  set no_timestamp [info exists flags(-no_timestamp)]
  set no_version [info exists flags(-no_version)]
  set gzip [info exists flags(-gzip)]
  write_sdf_cmd $filename $corner $divider $include_typ $digits $gzip \
    $no_timestamp $no_version
}

# sta namespace end
}
