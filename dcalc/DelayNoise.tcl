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

namespace eval sta {

variable min_coupling_sum_ratio_default .7
variable min_coupling_ratio_default .1

define_cmd_args "annotate_noise_delays" {[-scene scene]\
                                           [-min] [-max]\
                                           [-min_coupling_sum_ratio coupling_sum_ratio]\
                                           [-min_coupling_ratio coupling_ratio]} \
  -help {Annotate wire delays on victim nets that have coupling capacitances to aggressor nets that switch simultaneosly. Adjacent aggressor nets that switch in the opposite direction push the waveform down and increase the maximum delay on the wire. Nets that switch in the same direction decrease the minimum delay on the wire. Aggressor nets with min/max timing windows that do not overlap the victim timing window are not annotated.

Noise delays can be reported in timing reports with the -noise field.

Use the read_spef -keep_capacitive_coupling option to retain coupling capacitors for noise analysis.} \
  -arg_help {
    -scene {Annotate delay calculation for a scene. Required for multi-scene analysis.}
    -min {Only annotate minimum delays.}
    -max { Only annotate maximum delays.}
    -min_coupling_sum_ratio {The minimum ratio of total coupling capaictance for all aggressor nets to victim net capacitance. Nets with less than this capacitance ratio are not annotated. The default value is .1.}
    -min_coupling_ratio {The minimum ratio of aggressor coupling capaictance to victim net capacitance. Nets with less than this capacitance ratio are not annotated. The default value is .7.}
  }

proc annotate_noise_delays { args } {
  variable min_coupling_sum_ratio_default
  variable min_coupling_ratio_default

  parse_key_args "annotate_noise_delays" args \
    keys {-min_coupling_sum_ratio -min_coupling_ratio -scene} \
    flags {-min -max}

  set min_coupling_sum_ratio $min_coupling_sum_ratio_default
  if { [info exists keys(-min_coupling_sum_ratio)] } {
    set min_coupling_sum_ratio $keys(-min_coupling_sum_ratio)
    check_positive_float "-min_coupling_sum_ratio" $min_coupling_sum_ratio
  }

  set min_coupling_ratio $min_coupling_ratio_default
  if { [info exists keys(-min_coupling_ratio)] } {
    set min_coupling_ratio $keys(-min_coupling_ratio)
    check_positive_float "-min_coupling_ratio" $min_coupling_ratio
  }

  set scene [parse_scene keys]
  set min_max [parse_min_max_all_flags flags]
  check_argc_eq0 "annotate_noise_delays" $args
  
  annotate_noise_delays_cmd $min_coupling_sum_ratio \
    $min_coupling_ratio $scene $min_max
}

################################################################

define_cmd_args "report_noise_delays" {[-net net]\
                                         [-max_count max_count]\
                                         [-rise|-fall]\
                                         [-scene sc`ene] [-min] [-max]\
                                         [-min_coupling_sum_ratio coupling_sum_ratio]\
                                         [-min_coupling_ratio coupling_ratio]} \
  -help {Report noise delays on nets. Use -net to report noise delays on a net. Use -max_coiunt to see the top max_count noise delays in the design.} \
  -arg_help {
    -net {Report noise delays for net.}
    -max_count {Report noise delays for the top max_count nets.}
    -rise {Only report rise delays.}
    -fall {Only report fall delays.}
    -min {nly report minimum delays.}
    -max {Only report maximum delays.}
    -scene {Annotate delay calculation for a scene. Required for multi-scene analysis.}
    -min_coupling_sum_ratio {The minimum ratio of total coupling capaictance for all aggressor nets to victim net capacitance. Nets with less than this capacitance ratio are not annotated. The default value is .1.}
    -min_coupling_ratio {The minimum ratio of aggressor coupling capaictance to victim net capacitance. Nets with less than this capacitance ratio are not annotated. The default value is .7.}
  }

proc report_noise_delays { args } {
  variable min_coupling_sum_ratio_default
  variable min_coupling_ratio_default

  parse_key_args "report_noise_delays" args \
    keys {-net -max_count -min_coupling_sum_ratio -min_coupling_ratio -scene} \
    flags {-rise -fall -min -max}

  check_argc_eq0 "report_noise_delays" $args

  set min_coupling_sum_ratio $min_coupling_sum_ratio_default
  if { [info exists keys(-min_coupling_sum_ratio)] } {
    set min_coupling_sum_ratio $keys(-min_coupling_sum_ratio)
    check_positive_float "-min_coupling_sum_ratio" $min_coupling_sum_ratio
  }
  set min_coupling_ratio $min_coupling_ratio_default
  if { [info exists keys(-min_coupling_ratio)] } {
    set min_coupling_ratio $keys(-min_coupling_ratio)
    check_positive_float "-min_coupling_ratio" $min_coupling_ratio
  }
  set scene [parse_scene keys]
  set min_maxs [parse_min_max_all_flags flags]

  if { [info exists keys(-net)] } {
    set net [get_net_arg "net" $keys(-net)]
    set drvr_rfs [parse_rise_fall_flags flags]
    report_net_noise_delays_cmd $net $drvr_rfs $min_coupling_ratio $scene $min_maxs
  } else {
    set max_count 1
    if { [info exists keys(-max_count)] } {
      set max_count $keys(-max_count)
      check_positive_integer "-max_count" $max_count
    }
    report_noise_delays_cmd $max_count $min_coupling_sum_ratio $min_coupling_ratio\
      $scene $min_maxs
  }
}

################################################################

define_cmd_args "report_victim_nets" {[-scene scene]\
                                        [-min] [-max]\
                                        [-min_coupling_sum_ratio coupling_sum_ratio]} \
  -help {Report victim nets that have coupling capacitances to aggressor nets that switch simultaneosly.} \
  -arg_help {
    -scene {The delay calculation scene to annotate. Required for multi-scene analysis.}
    -min {Only annotate minimum delays.}
    -max {Only annotate maximum delays.}
    -min_coupling_sum_ratio {The minimum ratio of total coupling capaictance for all aggressor nets to victim net capacitance. Nets with less than this capacitance ratio are not annotated. The default value is .1.}
  }

proc report_victim_nets { args } {
  variable min_coupling_sum_ratio_default

  parse_key_args "report_victim_nets" args \
    keys {-min_coupling_sum_ratio -scene} \
    flags {-min -max}

  set min_coupling_sum_ratio $min_coupling_sum_ratio_default
  if { [info exists keys(-min_coupling_sum_ratio)] } {
    set min_coupling_sum_ratio $keys(-min_coupling_sum_ratio)
    check_positive_float "-min_coupling_sum_ratio" $min_coupling_sum_ratio
  }
  set scene [parse_scene keys]
  set min_max [parse_min_max_flags flags]
  check_argc_eq0 "report_victim_nets" $args
  report_victim_nets_cmd $min_coupling_sum_ratio $scene $min_max
}

################################################################

define_cmd_args "report_aggressors" {net\
                                       [-scene scene]\
                                       [-min] [-max]\
                                       [-min_coupling_ratio coupling_ratio]} \
  -help {Report the aggressort nets for a victim net.} \
  -arg_help {
    net {Report noise delays for net.}
    -min {Only report minimum delays.}
    -max {Only report maximum delays.}
    -scene {Report noise delays for scene. Required for multi-scene analysis.}
    -min_coupling_ratio {The minimum ratio of aggressor coupling capaictance to victim net capacitance. Nets with less than this capacitance ratio are not annotated. The default value is .7.}
  }

# Report nets with aggressors thatx
# coupling_cap(aggressor) / cap_ground > min_coupling_ratio
proc report_aggressors { args } {
  variable min_coupling_ratio_default

  parse_key_args "report_aggressors" args \
    keys {-min_coupling_ratio -scene} \
    flags {-min -max}

  check_argc_eq1 "report_aggressors" $args
  set net [get_net_arg "net" [lindex $args 0]]

  set min_coupling_ratio $min_coupling_ratio_default
  if { [info exists keys(-min_coupling_ratio)] } {
    set min_coupling_ratio $keys(-min_coupling_ratio)
    check_positive_float "-min_coupling_ratio" $min_coupling_ratio
  }
  set scene [parse_scene keys]
  set min_max [parse_min_max_flags flags]

  report_aggressors_cmd $net $min_coupling_ratio $scene $min_max
}

################################################################

define_cmd_args "write_delay_noise_case" {net file_basename -rise|-fall\
                                            [-scene scene]\
                                            [-min] [-max]\
                                            [-min_coupling_sum_ratio coupling_sum_ratio]\
                                            [-min_coupling_ratio coupling_ratio]\
                                            [-no_timestamp]} \
  -help {Write verilog and spef files for noise delays on a net. For debugging noise cases.} \
  -arg_help {
    -net {Write noise case for net.}
    file_basename {Base file name for the .v and .spef files.}
    -rise {Write files for net rising. }
    -fall {Write files for net falling. }
    -scene {Annotate delay calculation for a scene. Required for multi-scene analysis.}
    -min {Only annotate minimum delays.}
    -max { Only annotate maximum delays.}
    -min_coupling_sum_ratio {The minimum ratio of total coupling capaictance for all aggressor nets to victim net capacitance. Nets with less than this capacitance ratio are not annotated. The default value is .1.}
    -min_coupling_ratio {The minimum ratio of aggressor coupling capaictance to victim net capacitance. Nets with less than this capacitance ratio are not annotated. The default value is .7.}
    -no_timestamp {Do not include a time and date in the written files.}
  }

proc write_delay_noise_case { args } {
  variable min_coupling_sum_ratio_default
  variable min_coupling_ratio_default

  parse_key_args "write_delay_noise_case" args \
    keys {-min_coupling_sum_ratio -scene} \
    flags {-rise -fall -min -max -no_timestamp}

  check_argc_eq2 "write_delay_noise_case" $args
  set net [get_net_arg "net" [lindex $args 0]]
  set file_basename [lindex $args 1]
  set drvr_rf [parse_rise_fall_flags flags]

  set min_coupling_sum_ratio $min_coupling_sum_ratio_default
  if { [info exists keys(-min_coupling_sum_ratio)] } {
    set min_coupling_sum_ratio $keys(-min_coupling_sum_ratio)
    check_positive_float "-min_coupling_sum_ratio" $min_coupling_sum_ratio
  }
  set min_coupling_ratio $min_coupling_ratio_default
  if { [info exists keys(-min_coupling_ratio)] } {
    set min_coupling_ratio $keys(-min_coupling_ratio)
    check_positive_float "-min_coupling_ratio" $min_coupling_ratio
  }

  set scene [parse_scene keys]
  set min_max [parse_min_max_flags flags]
  set no_timestamp [info exists flags(-no_timestamp)]
  write_noise_delay_case_cmd $net $drvr_rf $file_basename \
    $min_coupling_sum_ratio $min_coupling_ratio $no_timestamp $scene $min_max
}

################################################################

# debugging command
define_hidden_cmd_args "report_gate_noise_delays" {gates [-scene scene] [-min] [-max]}

proc report_gate_noise_delays { args } {
  parse_key_args "report_gate_noise_delays" args \
    keys {-scene} \
    flags {-min -max}
  check_argc_eq1 "report_gate_noise_delays" $args
  set gates [lindex $args 0]
  set scene [parse_scene keys]
  set min_max [parse_min_max_flags flags]
  report_gate_noise_delays_cmd $gates $scene $min_max
}

# sta namespace end
}
