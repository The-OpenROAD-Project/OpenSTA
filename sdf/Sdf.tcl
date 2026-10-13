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

define_cmd_args "read_sdf" \
  {[-path path] [-scene scene]\
     [-cond_use min|max|min_max]\
     [-unescaped_dividers] filename} \
  -help {Read SDF delays from a file. The min and max values in the SDF tuples are used to annotate delays. Typical values in the SDF tuples are ignored. If multiple scenes are defined `-scene` must be specified. SDC annotation for MCMM analysis must follow the scene definitions.

Files compressed with gzip are automatically uncompressed.

INCREMENT is supported as an alias for INCREMENTAL.

The following SDF statements are not supported.

```
PORT
INSTANCE wildcards
```} \
  -arg_help {
    -scene {Scene delays to annotate.}
    -path {Hierarchical instance path prefix for SDF annotation.}
    -cond_use {`min`: Use SDF COND delays for min analysis. `max`: Use COND delays for max analysis. `min_max`: Use COND delays for min and max analysis.}
    -unescaped_dividers {With this option path names in the SDF do not have to escape hierarchy dividers when the path name is escaped. For example, the escaped Verilog name "\inst1/inst2 " can be referenced as "inst1/inst2". The correct SDF name is "inst1\/inst2", since the divider does not represent a change in hierarchy in this case.}
    filename {The name of the SDF file to read.}
  }

proc_redirect read_sdf {
  parse_key_args "read_sdf" args \
    keys {-path -corner -scene -cond_use -analysis_type} \
    flags {-unescaped_dividers -incremental_only}

  check_argc_eq1 "read_sdf" $args
  set filename [file nativename [lindex $args 0]]
  set path ""
  if [info exists keys(-path)] {
    set path $keys(-path)
  }
  set scene [parse_scene keys]

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
  read_sdf_file $filename $path $scene $unescaped_dividers \
    $incremental_only $cond_use
}

################################################################

define_cmd_args "report_annotated_delay" \
  {[-cell] [-net] [-from_in_ports] [-to_out_ports]\
     [-scene scene] [-max_lines lines]\
     [-report_annotated] [-report_unannotated] [-constant_arcs]} \
  -help {The `report_annotated_delay` command reports a summary of SDF delay annotation. Without the `-from_in_ports` and `-to_out_ports` options arcs to and from top level ports are not reported. The `-report_annotated` and `-report_unannotated` options can be used to list arcs that are annotated or not annotated.} \
  -arg_help {
    -cell {Report annotated cell delays.}
    -net {Report annotated internal net delays.}
    -from_in_ports {Report annotated delays from input ports.}
    -to_out_ports {Report annotated delays to output ports.}
    -max_lines {`lines`: Maximum number of lines listed by the `-report_annotated` and `-report_unannotated` options.}
    -report_annotated {Report annotated timing arcs.}
    -report_unannotated {Report unannotated timing arcs.}
    -constant_arcs {Report separate annotation counts for arcs disabled by logic constants (`set_logic_one`, `set_logic_zero`).}
  }

proc_redirect report_annotated_delay {
  parse_key_args "report_annotated_delay" args keys {-scene -corner -max_lines} \
    flags {-cell -net -from_in_ports -to_out_ports  \
             -report_annotated -report_unannotated -constant_arcs \
             -list_not_annotated -list_annotated}
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

  set scene [parse_scene keys]
  set max_lines 0
  if { [info exists keys(-max_lines)] } {
    set max_lines $keys(-max_lines)
    check_positive_integer "-max_lines" $max_lines
  }

  set report_annotated [info exists flags(-report_annotated)]
  if { [info exists flags(-list_annotated)] } {
    # Deprecated 05/26/2025
    sta_warn 624 "-list_annotated is deprecated. Use -report_annotated."
    set report_annotated 1
  }
  set report_unannotated [info exists flags(-report_unannotated)]
  if { [info exists flags(-list_not_annotated)] } {
    # Deprecated 05/26/2025
    sta_warn 625  "-list_not_annotated is deprecated. Use -report_unannotated."
    set report_unannotated 1
  }

  report_annotated_delay_cmd $scene $report_cells $report_nets \
    $report_in_nets $report_out_nets \
    $max_lines $report_annotated $report_unannotated \
    [info exists flags(-constant_arcs)]
}

define_cmd_args "report_annotated_check" \
  {[-setup] [-hold] [-recovery] [-removal] [-nochange]\
     [-width] [-period] [-max_skew]\
     [-scene scene] [-max_lines lines]\
     [-report_annotated] [-report_unannotated] [-constant_arcs]} \
  -help {The `report_annotated_check` command reports a summary of SDF timing check annotation. The `-report_annotated` and `-report_annotated` options can be used to list arcs that are annotated or not annotated.} \
  -arg_help {
    -recovery {Report annotated recovery checks.}
    -removal {Report annotated removal checks.}
    -nochange {Report annotated nochange checks.}
    -width {Report annotated width checks.}
    -period {Report annotated period checks.}
    -max_skew {Report annotated max skew checks.}
    -max_lines {`lines`: Maximum number of lines listed by the `-report_annotated` and `-report_unannotated` options.}
    -report_annotated {Report annotated timing arcs.}
    -report_unannotated {Report unannotated timing arcs.}
    -constant_arcs {Report separate annotation counts for arcs disabled by logic constants (`set_logic_one`, `set_logic_zero`).}
  }

proc_redirect report_annotated_check {
  parse_key_args "report_annotated_check" args keys {-scene -max_lines} \
    flags {-setup -hold -recovery -removal -nochange -width -period \
             -max_skew -report_annotated -report_unannotated -constant_arcs \
             -list_annotated -list_not_annotated}
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

  set scene [parse_scene keys]
  set max_lines 0
  if { [info exists keys(-max_lines)] } {
    set max_lines $keys(-max_lines)
    check_positive_integer "-max_lines" $max_lines
  }

  set report_annotated [info exists flags(-report_annotated)]
  if { [info exists flags(-list_annotated)] } {
    # Deprecated 05/26/2025
    sta_warn 626 "-list_annotated is deprecated. Use -report_annotated."
    set report_annotated 1
  }
  set report_unannotated [info exists flags(-report_unannotated)]
  if { [info exists flags(-list_not_annotated)] } {
    # Deprecated 05/26/2025
    sta_warn 627 "-list_not_annotated is deprecated. Use -report_unannotated."
    set report_unannotated 1
  }

  report_annotated_check_cmd $scene $report_setup $report_hold \
    $report_recovery $report_removal $report_nochange \
    $report_width $report_period $report_max_skew \
    $max_lines $report_annotated $report_unannotated \
    [info exists flags(-constant_arcs)]
}

define_cmd_args "write_sdf" \
  {[-scene scene] [-divider /|.] [-include_typ]\
     [-digits digits] [-gzip] [-no_timestamp] [-no_version] filename} \
  -help {Write the delay calculation delays for the design in SDF format to `filename`. If `-scene` is not specified the min/max delays are across all scenes. With `-scene` the min/max delays for that scene are written. The SDF TIMESCALE is the same as the time_unit in the first Liberty file read.} \
  -arg_help {
    -scene {Write delays for scene.}
    -divider {Divider to use between hierarchy levels in pin and instance names.}
    -include_typ {Include a 'typ' value in the SDF triple that is the average of min and max delays to satisfy some Verilog simulators that require three values in the delay triples.}
    -gzip {Compress the SDF using gzip.}
    -no_timestamp {Do not write a DATE statement.}
    -no_version {Do not write a VERSION statement.}
    filename {The SDF filename to write.}
  }

proc_redirect write_sdf {
  parse_key_args "write_sdf" args \
    keys {-corner -scene -divider -digits} \
    flags {-include_typ -gzip -no_timestamp -no_version}
  check_argc_eq1 "write_sdf" $args
  set scene [parse_scene keys]
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
  write_sdf_cmd $filename $scene $divider $include_typ $digits $gzip \
    $no_timestamp $no_version
}

# sta namespace end
}
