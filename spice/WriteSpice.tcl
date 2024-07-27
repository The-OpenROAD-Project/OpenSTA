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

define_cmd_args "write_path_spice" { -path_args path_args\
				       -spice_directory spice_directory\
				       -lib_subckt_file lib_subckts_file\
				       -model_file model_file\
				       -power power\
				       -ground ground\
                                       [-simulator hspice|ngspice|xyce]}

proc write_path_spice { args } {
  parse_key_args "write_path_spice" args \
    keys {-spice_directory -lib_subckt_file -model_file \
	    -power -ground -path_args -simulator} \
    flags {}

  if { [info exists keys(-spice_directory)] } {
    set spice_dir [file nativename $keys(-spice_directory)]
    if { ![file exists $spice_dir] } {
      sta_error 1920 "Directory $spice_dir not found."
    }
    if { ![file isdirectory $spice_dir] } {
      sta_error 1921 "$spice_dir is not a directory."
    }
    if { ![file writable $spice_dir] } {
      sta_error 1922 "Cannot write in $spice_dir."
    }
  } else {
    sta_error 1923 "No -spice_directory specified."
  }

  if { [info exists keys(-lib_subckt_file)] } {
    set lib_subckt_file [file nativename $keys(-lib_subckt_file)]
    if { ![file readable $lib_subckt_file] } {
      sta_error 1924 "-lib_subckt_file $lib_subckt_file is not readable."
    }
  } else {
    sta_error 1925 "No -lib_subckt_file specified."
  }

  if { [info exists keys(-model_file)] } {
    set model_file [file nativename $keys(-model_file)]
    if { ![file readable $model_file] } {
      sta_error 1926 "-model_file $model_file is not readable."
    }
  } else {
    sta_error 1927 "No -model_file specified."
  }

  if { [info exists keys(-power)] } {
    set power $keys(-power)
  } else {
    sta_error 1928 "No -power specified."
  }

  if { [info exists keys(-ground)] } {
    set ground $keys(-ground)
  } else {
    sta_error 1929 "No -ground specified."
  }

  set ckt_sim [parse_ckt_sim_key keys]

  if { ![info exists keys(-path_args)] } {
    sta_error 1930 "No -path_args specified."
  }
  set path_args $keys(-path_args)
  set path_ends [eval [concat find_timing_paths $path_args]]
  if { $path_ends == {} } {
    sta_error 1931 "No paths found for -path_args $path_args."
  } else {
    set path_index 1
    foreach path_end $path_ends {
      set path [$path_end path]
      set path_name "path_$path_index"
      set spice_file [file join $spice_dir "$path_name.sp"]
      set subckt_file [file join $spice_dir "$path_name.subckt"]
      write_path_spice_cmd $path $spice_file $subckt_file \
	$lib_subckt_file $model_file $power $ground $ckt_sim
      incr path_index
    }
  }
}

set ::ckt_sims {hspice ngspice xyce}

proc parse_ckt_sim_key { keys_var } {
  upvar 1 $keys_var keys
  global ckt_sims

  set ckt_sim "ngspice"
  if { [info exists keys(-simulator)] } {
    set ckt_sim [file nativename $keys(-simulator)]
    if { [lsearch $ckt_sims $ckt_sim] == -1 } {
      sta_error 1910 "Unknown circuit simulator"
    }
  }
  return $ckt_sim
}

################################################################

define_cmd_args "write_gate_spice" \
  { -gates {{instance input_port driver_port edge [delay]}...}\
      -spice_filename spice_filename\
      -lib_subckt_file lib_subckts_file\
      -model_file model_file\
      -power power\
      -ground ground\
      [-simulator hspice|ngspice|xyce]\
      [-corner corner]\
      [-min] [-max]}

proc write_gate_spice { args } {
  parse_key_args "write_gate_spice" args \
    keys {-gates -spice_filename -lib_subckt_file -model_file \
            -power -ground -simulator -corner}\
    flags {-measure_stmts -min -max}

  if { [info exists keys(-gates)] } {
    set gates $keys(-gates)
  } else {
    sta_error 1932 "Missing -gates argument."
  }
  if { [info exists keys(-spice_filename)] } {
    set spice_file [file nativename $keys(-spice_filename)]
    set spice_dir [file dirname $spice_file]
    if { ![file writable $spice_dir] } {
      sta_error 1903 "Cannot write $spice_dir."
    }
  } else {
    sta_error 1904 "No -spice_filename specified."
  }

  if { [info exists keys(-lib_subckt_file)] } {
    set lib_subckt_file [file nativename $keys(-lib_subckt_file)]
    if { ![file readable $lib_subckt_file] } {
      sta_error 1905 "-lib_subckt_file $lib_subckt_file is not readable."
    }
  } else {
    sta_error 1906 "No -lib_subckt_file specified."
  }

  if { [info exists keys(-model_file)] } {
    set model_file [file nativename $keys(-model_file)]
    if { ![file readable $model_file] } {
      sta_error 1907 "-model_file $model_file is not readable."
    }
  } else {
    sta_error 1908 "No -model_file specified."
  }

  if { [info exists keys(-power)] } {
    set power $keys(-power)
  } else {
    sta_error 1909 "No -power specified."
  }

  if { [info exists keys(-ground)] } {
    set ground $keys(-ground)
  } else {
    sta_error 1915 "No -ground specified."
  }

  set ckt_sim [parse_ckt_sim_key keys]

  set corner [parse_corner keys]
  set min_max [parse_min_max_flags flags]
  check_argc_eq0 "write_gate_spice" $args

  set spice_dir [file dirname $spice_file]
  set spice_root [file rootname [file tail $spice_file]]
  set subckt_file [file join $spice_dir "$spice_root.subckt"]
  write_gate_spice_cmd $gates $spice_file $subckt_file \
    $lib_subckt_file $model_file $power $ground $ckt_sim \
    $corner $min_max
}

################################################################

# plot_pins defaults to input_pin, driver_pina and load pins for each driver.
define_cmd_args "write_gate_gnuplot" \
  { -gates {{instance input_port driver_port edge [delay]}...}\
      -plot_pins plot_pins\
      -plot_basename plot_basename\
      [-corner corner] [-min] [-max]}

proc write_gate_gnuplot { args } {
  parse_key_args "write_gate_gnuplot" args \
    keys {-gates -plot_pins -plot_basename -spice_waveforms -corner} \
    flags {-min -max}

  if { [info exists keys(-gates)] } {
    set gates $keys(-gates)
  } else {
    sta_error 1933 "Missing -gates argument."
  }
  if { [info exists keys(-plot_pins)] } {
    set plot_pins [get_port_pins_error "-plot_pins" $keys(-plot_pins)]
  } else {
    set plot_pins {}
    set plot_all_loads 0
    set gate_idx 0
    foreach gate $gates {
      set in_pin [parse_gate_in_pin $gate]
      set drvr_pin [parse_gate_drvr_pin $gate]
      lappend plot_pins $in_pin
      lappend plot_pins $drvr_pin
      # Only plot driver loads.
      if { $plot_all_loads || $gate_idx == 0 } {
        set pin_iter [$drvr_pin connected_pin_iterator]
        while { [$pin_iter has_next] } {
          set pin [$pin_iter next]
          if { [$pin is_load] } {
            lappend plot_pins $pin
          }
        }
        $pin_iter finish
      }
      incr gate_idx
    }
  }

  if { [info exists keys(-plot_basename)] } {
    set plot_base [file nativename $keys(-plot_basename)]
    set plot_dir [file dirname $plot_base]
    if { ![file writable $plot_dir] } {
      sta_error 1913 "Cannot write $plot_dir."
    }
  } else {
    sta_error 1914 "No -plot_basename specified."
  }
  set gnuplot_filename "${plot_base}.gnuplot"
  set csv_filename "${plot_base}.csv"

  set sim_wave_filename ""
  if { [info exists keys(-spice_waveforms)] } {
    set sim_wave_filename $keys(-spice_waveforms)
  }

  set corner [parse_corner keys]
  set min_max [parse_min_max_flags flags]

  write_gate_gnuplot_cmd $gates $plot_pins $sim_wave_filename \
    $gnuplot_filename $csv_filename $corner $min_max
}

proc parse_gate_drvr_pin { gate_arg } {
  lassign $gate_arg inst_name in_port_name in_rf drvr_port_name drvr_rf
  set inst [get_instance_error "instance" $inst_name]
  set drvr_pin [$inst find_pin $drvr_port_name]
  return $drvr_pin
}

proc parse_gate_in_pin { gate_arg } {
  lassign $gate_arg inst_name in_port_name in_rf drvr_port_name drvr_rf
  set inst [get_instance_error "instance" $inst_name]
  set in_pin [$inst find_pin $in_port_name]
  return $in_pin
}

# sta namespace end.
}
