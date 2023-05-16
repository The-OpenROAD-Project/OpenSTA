# OpenSTA, Static Timing Analyzer
# Copyright (c) 2023, Parallax Software, Inc.
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
				       -ground ground}

proc write_path_spice { args } {
  parse_key_args "write_path_spice" args \
    keys {-spice_directory -lib_subckt_file -model_file \
	    -power -ground -path_args} \
    flags {}

  if { [info exists keys(-spice_directory)] } {
    set spice_dir [file nativename $keys(-spice_directory)]
    if { ![file exists $spice_dir] } {
      sta_error 496 "Directory $spice_dir not found."
    }
    if { ![file isdirectory $spice_dir] } {
      sta_error 497 "$spice_dir is not a directory."
    }
    if { ![file writable $spice_dir] } {
      sta_error 498 "Cannot write in $spice_dir."
    }
  } else {
    sta_error 499 "No -spice_directory specified."
  }

  if { [info exists keys(-lib_subckt_file)] } {
    set lib_subckt_file [file nativename $keys(-lib_subckt_file)]
    if { ![file readable $lib_subckt_file] } {
      sta_error 500 "-lib_subckt_file $lib_subckt_file is not readable."
    }
  } else {
    sta_error 501 "No -lib_subckt_file specified."
  }

  if { [info exists keys(-model_file)] } {
    set model_file [file nativename $keys(-model_file)]
    if { ![file readable $model_file] } {
      sta_error 502 "-model_file $model_file is not readable."
    }
  } else {
    sta_error 503 "No -model_file specified."
  }

  if { [info exists keys(-power)] } {
    set power $keys(-power)
  } else {
    sta_error 504 "No -power specified."
  }

  if { [info exists keys(-ground)] } {
    set ground $keys(-ground)
  } else {
    sta_error 505 "No -ground specified."
  }

  if { ![info exists keys(-path_args)] } {
    sta_error 506 "No -path_args specified."
  }
  set path_args $keys(-path_args)
  set path_ends [eval [concat find_timing_paths $path_args]]
  if { $path_ends == {} } {
    sta_error 507 "No paths found for -path_args $path_args."
  } else {
    set path_index 1
    foreach path_end $path_ends {
      set path [$path_end path]
      set path_name "path_$path_index"
      set spice_file [file join $spice_dir "$path_name.sp"]
      set subckt_file [file join $spice_dir "$path_name.subckt"]
      write_path_spice_cmd $path $spice_file $subckt_file \
	$lib_subckt_file $model_file {} $power $ground
      incr path_index
    }
  }
}

# sta namespace end.
}
