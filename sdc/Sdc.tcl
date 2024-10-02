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

################################################################
#
# SDC commands.
# Alphabetical within groups as in the SDC spec.
#
# Argument parsing and checking is done in TCL before calling a
# SWIG interface function.
#
################################################################

namespace eval sta {

define_cmd_args "read_sdc" {[-echo] filename}

proc_redirect read_sdc {
  parse_key_args "read_sdc" args keys {} flags {-echo}

  check_argc_eq1 "read_sdc" $args
  set echo [info exists flags(-echo)]
  set filename [file nativename [lindex $args 0]]
  source_ $filename $echo 0
}

################################################################

# The builtin Tcl "source" command is redefined by sta.
# This rename provides a mechanism to refer to the original TCL
# command.
# Protected so this file can be reloaded without blowing up.
if { ![info exists renamed_source] } {
  rename source builtin_source
  set renamed_source 1
}

set ::sta_continue_on_error 0

define_cmd_args "source" \
  {[-echo] [-verbose] filename [> filename] [>> filename]}

# Override source to support -echo and return codes.
proc_redirect source {
  parse_key_args "source" args keys {-encoding} flags {-echo -verbose}
  if { [llength $args] != 1 } {
    cmd_usage_error "source"
  }
  set echo [info exists flags(-echo)]
  set verbose [info exists flags(-verbose)]
  set filename [file nativename [lindex $args 0]]
  source_ $filename $echo $verbose
}

proc source_ { filename echo verbose } {
  global sta_continue_on_error
  variable sdc_file
  variable sdc_line
  if [catch {open $filename r} stream] {
    sta_error 340 "cannot open '$filename'."
  } else {
    if { [file extension $filename] == ".gz" } {
      zlib push gunzip $stream
    }
    # Save file and line in recursive call to source.
    if { [info exists sdc_file] } {
      set sdc_file_save $sdc_file
      set sdc_line_save $sdc_line
    }
    set sdc_file $filename
    set sdc_line 1
    set cmd ""
    set error {}
    while {![eof $stream]} {
      gets $stream line
      if { $line != "" } {
	if {$echo} {
	  report_line $line
	}
      }
      append cmd $line "\n"
      if { [string index $line end] != "\\" \
	     && [info complete $cmd] } {
	set error {}
	set error_code [catch {uplevel \#0 $cmd} result]
	# cmd consumed
	set cmd ""
	# Flush results printed outside tcl to stdout/stderr.
	fflush
	switch $error_code {
	  0 { if { $verbose && $result != "" } { report_line $result } }
	  1 { set error $result }
	  2 { set error {invoked "return" outside of a proc.} }
	  3 { set error {invoked "break" outside of a loop.} }
	  4 { set error {invoked "continue" outside of a loop.} }
	}
	if { $error != {} } {
	  if { $sta_continue_on_error } {
	    # Only prepend error message with file/line once.
	    if { [string first "Error" $error] == 0 } {
	      report_line $error
	    } else {
	      report_line "Error: [file tail $sdc_file], $sdc_line $error"
	    }
            set error {}
          } else {
	    break
	  }
	}
      }
      incr sdc_line
    }
    close $stream
    if { $cmd != {} } {
      sta_error 341 "incomplete command at end of file."
    }
    set error_sdc_file $sdc_file
    set error_sdc_line $sdc_line
    if { [info exists sdc_file_save] } {
      set sdc_file $sdc_file_save
      set sdc_line $sdc_line_save
    } else {
      unset sdc_file
      unset sdc_line
    }
    if { $error != {} } {
      # Only prepend error message with file/line once.
      if { [string first "Error" $error] == 0 } {
	error $error
      } else {
	error "Error: [file tail $error_sdc_file], $error_sdc_line $error"
      }
    }
  }
}

################################################################

define_cmd_args "write_sdc" \
  {[-map_hpins] [-digits digits] [-gzip] [-no_timestamp] filename}

proc write_sdc { args } {
  parse_key_args "write_sdc" args keys {-digits -significant_digits} \
    flags {-map_hpins -compatible -gzip -no_timestamp}
  check_argc_eq1 "write_sdc" $args

  set digits 4
  if { [info exists keys(-digits)] } {
    set digits $keys(-digits)
  }
  check_positive_integer "-digits" $digits

  set filename [file nativename [lindex $args 0]]
  set gzip [info exists flags(-gzip)]
  set no_timestamp [info exists flags(-no_timestamp)]
  set map_hpins [info exists flags(-map_hpins)]
  set native [expr ![info exists flags(-compatible)]]
  write_sdc_cmd $filename $map_hpins $native $digits $gzip $no_timestamp
}

################################################################
#
# General Purpose Commands
#
################################################################

define_cmd_args "current_instance" {[instance]}

proc current_instance { {inst ""} } {
  if { $inst == "" } {
    set current_instance [top_instance]
  } else {
    set current_instance [get_instance_error "instance" $inst]
  }
  set cell [get_name [$current_instance cell]]
  report_line "Current instance is $cell."
  # Current instance state variable must be part of the sta state so
  # the tcl interpreter can be shared by multiple sdc files.
  set_current_instance $current_instance
}

################################################################

define_cmd_args "set_hierarchy_separator" { seperator }

set ::hierarchy_separator "/"

proc set_hierarchy_separator { separator } {
  global hierarchy_separator
  check_path_divider $separator
  set_path_divider $separator
  set hierarchy_separator $separator
}

proc check_path_divider { divider } {
  set sdc_dividers "/@^#.|"
  if { !([string length $divider] == 1
	 && [string first $divider $sdc_dividers] != -1)} {
    sta_error 342 "hierarchy separator must be one of '$sdc_dividers'."
  }
}

################################################################

define_cmd_args "set_units" \
  {[-time time_unit] [-capacitance cap_unit] [-resistance res_unit]\
     [-voltage voltage_unit] [-current current_unit] [-power power_unit]\
     [-distance distance_unit]}

# Note that the set_units command does NOT actually set the units.
# It merely checks that the current units are the same as the
# units in the set_units command.
proc set_units { args } {
  parse_key_args "set_units" args \
    keys {-capacitance -resistance -time -voltage -current -power -distance} \
    flags {}
  check_argc_eq0 "set_units" $args
  check_unit "time" -time "s" keys
  check_unit "capacitance" -capacitance "f" keys
  check_unit "resistance" -resistance "ohm" keys
  check_unit "voltage" -voltage "v" keys
  check_unit "current" -current "A" keys
  check_unit "power" -power "W" keys
  check_unit "distance" -distance "m" keys
}

proc check_unit { unit key suffix key_var } {
  upvar 1 $key_var keys
  if { [info exists keys($key)] } {
    set value $keys($key)
    set suffix_length [string length $suffix]
    set arg_suffix [string range $value end-[expr $suffix_length - 1] end]
    if { [string match -nocase $arg_suffix $suffix] } {
      set arg_prefix [string range $value 0 end-$suffix_length]
      if { [regexp "^(10*)?(\[Mkmunpf\])?$" $arg_prefix ignore mult prefix] } {
        #puts "$arg_prefix '$mult' '$prefix'"
        if { $mult == "" } {
          set mult 1
        }
        set scale [unit_prefix_scale $unit $prefix ]
        check_unit_scale $unit [expr $scale * $mult]
      } else {
        sta_error 343 "unknown unit $unit prefix '${arg_prefix}'."
      }
    } else {
      sta_error 501 "incorrect unit suffix '$arg_suffix'."
    }
  }
}

proc unit_prefix_scale { unit prefix } {
  switch -exact -- $prefix {
    "M" { return 1E+6  }
    "k" { return 1E+3  }
    ""  { return 1E+0  }
    "m" { return 1E-3  }
    "u" { return 1E-6  }
    "n" { return 1E-9  }
    "p" { return 1E-12 }
    "f" { return 1E-15 }
  }
  sta_error 344 "unknown $unit prefix '$prefix'."
}

proc check_unit_scale { unit scale } {
  set unit_scale [unit_scale $unit]
  if { ![fuzzy_equal $scale $unit_scale] } {
    sta_warn 345 "$unit scale [format %.0e $scale] does not match library scale [format %.0e $unit_scale]."
  }
}

################################################################
#
# Object Access Commands
#
################################################################

define_cmd_args "all_clocks" {}

proc all_clocks { } {
  return [get_clocks -quiet *]
}

################################################################

define_cmd_args "all_inputs" {[-no_clocks]}

proc all_inputs { args } {
  parse_key_args "all_inputs" args keys {} flags {-no_clocks}
  set no_clks [info exists flags(-no_clocks)]
  return [all_inputs_cmd $no_clks]
}

################################################################

define_cmd_args "all_outputs" {}

proc all_outputs { args } {
  check_argc_eq0 "all_outputs" $args
  return [all_outputs_cmd]
}

################################################################

define_cmd_args all_registers \
  {[-clock clocks] [-rise_clock clocks] [-fall_clock clocks] [-cells] [-data_pins] [-clock_pins]\
     [-async_pins] [-output_pins] [-level_sensitive] [-edge_triggered]}

proc all_registers { args } {
  parse_key_args "all_registers" args keys {-clock -rise_clock -fall_clock} \
    flags {-cells -data_pins -clock_pins -async_pins -output_pins -level_sensitive -edge_triggered}
  check_argc_eq0 "all_registers" $args

  set clks {}
  set clk_rf "rise_fall"
  if [info exists keys(-clock)] {
    set clks [get_clocks_warn "clocks" $keys(-clock)]
  }
  if [info exists keys(-rise_clock)] {
    set clks [get_clocks_warn "clocks" $keys(-rise_clock)]
    set clk_rf "rise"
  }
  if [info exists keys(-fall_clock)] {
    set clks [get_clocks_warn "clocks" $keys(-fall_clock)]
    set clk_rf "fall"
  }

  if {[info exists flags(-edge_triggered)] \
	&& ![info exists flags(-level_sensitive)]} {
    set edge_triggered 1
    set level_sensitive 0
  } elseif {[info exists flags(-level_sensitive)] \
	      && ![info exists flags(-edge_triggered)]} {
    set level_sensitive 1
    set edge_triggered 0
  } else {
    set edge_triggered 1
    set level_sensitive 1
  }
  if { [expr [info exists flags(-cells)] \
          + [info exists flags(-data_pins)] \
          + [info exists flags(-clock_pins)] \
          +  [info exists flags(-async_pins)] \
          + [info exists flags(-output_pins)]] > 1 } {
    sta_error 346 "only one of -cells, -data_pins, -clock_pins, -async_pins, -output_pins are suppported."
  }
  if [info exists flags(-cells)] {
    return [find_register_instances $clks $clk_rf \
	      $edge_triggered $level_sensitive]
  } elseif [info exists flags(-data_pins)] {
    return [find_register_data_pins $clks $clk_rf \
	      $edge_triggered $level_sensitive]
  } elseif [info exists flags(-clock_pins)] {
    return [find_register_clk_pins $clks $clk_rf \
	      $edge_triggered $level_sensitive]
  } elseif [info exists flags(-async_pins)] {
    return [find_register_async_pins $clks $clk_rf \
	      $edge_triggered $level_sensitive]
  } elseif [info exists flags(-output_pins)] {
    return [find_register_output_pins $clks $clk_rf \
	      $edge_triggered $level_sensitive]
  } else {
    # -cells is the default.
    return [find_register_instances $clks $clk_rf \
	      $edge_triggered $level_sensitive]
  }
}

################################################################

define_cmd_args "current_design" {[design]}

variable current_design_name ""

proc current_design { {design ""} } {
  variable current_design_name

  if { $design == "" } {
    # top_instance errors if the network has not been linked.
    set current_design_name [get_name [get_object_property [top_instance] cell]]
  } elseif { ![network_is_linked] } {
    set current_design_name $design
    return $design
  } elseif { [network_is_linked] && $design == [get_name [get_object_property [top_instance] cell]] } {
    set current_design_name $design
    return $design
  } else {
    sta_warn 347 "current_design for other than top cell not supported."
    set current_design_name $design
    return $design
  }
}

################################################################

# Generic get_* filter.
proc filter_objs { filter objects filter_function object_type } {
  set filter_regexp1 {@?([a-zA-Z_]+) *((==|!=|=~|!~) *([0-9a-zA-Z_\*]+))?}
  set filter_or_regexp "($filter_regexp1) *\\|\\| *($filter_regexp1)"
  set filter_and_regexp "($filter_regexp1) *&& *($filter_regexp1)"
  set filtered_objects {}
  # Ignore sub-exprs in filter_regexp1 for expr2 match var.
  if { [regexp $filter_or_regexp $filter ignore expr1 ignore ignore ignore ignore expr2] } {
    set filtered_objects1 [filter_objs $expr1 $objects $filter_function $object_type]
    set filtered_objects2 [filter_objs $expr2 $objects $filter_function $object_type]
    set filtered_objects [concat $filtered_objects1 $filtered_objects2]
  } elseif { [regexp $filter_and_regexp $filter ignore expr1 ignore ignore ignore ignore expr2] } {
    set filtered_objects [filter_objs $expr1 $objects $filter_function $object_type]
    set filtered_objects [filter_objs $expr2 $filtered_objects $filter_function $object_type]
  } elseif { [regexp $filter_regexp1 $filter ignore attr_name ignore op arg] } {
    # If no op/arg, use <attr_name>==1 by default.
    set op [expr {($op == "") ? "==" : $op}]
    set arg [expr {($arg == "") ? "1" : $arg}]
    set filtered_objects [$filter_function $attr_name $op $arg $objects]
  } else {
    sta_error 350 "unsupported $object_type -filter expression."
  }
  return $filtered_objects
}

################################################################

define_cmd_args "get_cells" \
  {[-hierarchical] [-hsc separator] [-filter expr]\
     [-regexp] [-nocase] [-quiet] [-of_objects objects] [patterns]}

define_cmd_alias "get_cell" "get_cells"

# Really get_instances, but SDC calls instances "cells".
proc get_cells { args } {
  global hierarchy_separator

  parse_key_args "get_cells" args keys {-hsc -filter -of_objects} \
    flags {-hierarchical -regexp -nocase -quiet}
  check_nocase_flag flags

  set regexp [info exists flags(-regexp)]
  set nocase [info exists flags(-nocase)]
  set hierarchical [info exists flags(-hierarchical)]
  set quiet [info exists flags(-quiet)]
  # Copy backslashes that will be removed by foreach.
  if { $args == {} } {
    set patterns "*"
  } else {
    set patterns [string map {\\ \\\\} [lindex $args 0]]
  }
  set divider $hierarchy_separator
  if [info exists keys(-hsc)] {
    set divider $keys(-hsc)
    check_path_divider $divider
  }
  set insts {}
  if [info exists keys(-of_objects)] {
    if { $args != {} } {
      sta_warn 348 "patterns argument not supported with -of_objects."
    }
    parse_port_pin_net_arg $keys(-of_objects) pins nets
    foreach pin $pins {
      if { [$pin is_top_level_port] } {
	set net [get_nets [get_name $pin]]
	if { $net != "NULL" } {
	  lappend nets $net
	}
      } else {
	lappend insts [$pin instance]
      }
    }
    foreach net $nets {
      set pin_iter [$net pin_iterator]
      while { [$pin_iter has_next] } {
	set pin [$pin_iter next]
	lappend insts [$pin instance]
      }
      $pin_iter finish
    }
  } else {
    check_argc_eq0or1 "get_cells" $args
    foreach pattern $patterns {
      if { [is_object $pattern] } {
	if { [object_type $pattern] != "Instance" } {
	  sta_error 326 "object '$pattern' is not an instance."
	}
	set insts [concat $insts $pattern]
      } else {
	if { $divider != $hierarchy_separator } {
	  regsub $divider $pattern $hierarchy_separator pattern
	}
	if { $hierarchical } {
	  set matches [find_instances_hier_matching $pattern $regexp $nocase]
	} else {
	  set matches [find_instances_matching $pattern $regexp $nocase]
	}
	if { $matches == {} && !$quiet} {
	  sta_warn 349 "instance '$pattern' not found."
	}
	set insts [concat $insts $matches]
      }
    }
  }
  if [info exists keys(-filter)] {
    set insts [filter_objs $keys(-filter) $insts filter_insts "instance"]
  }
  return $insts
}

################################################################

define_cmd_args "get_clocks" {[-regexp] [-nocase] [-quiet] [-filter expr] [patterns]}

define_cmd_alias "get_clock" "get_clocks"

proc get_clocks { args } {
  parse_key_args "get_clocks" args keys {-filter} flags {-regexp -nocase -quiet}
  check_argc_eq0or1 "get_clocks" $args
  check_nocase_flag flags

  # Copy backslashes that will be removed by foreach.
  if { $args == {} } {
    set patterns "*"
  } else {
    set patterns [string map {\\ \\\\} [lindex $args 0]]
  }
  set regexp [info exists flags(-regexp)]
  set nocase [info exists flags(-nocase)]
  set clocks {}
  foreach pattern $patterns {
    if { [is_object $pattern] } {
      if { [object_type $pattern] != "Clock" } {
	sta_error 327 "object '$pattern' is not an clock."
      }
      set clocks [concat $clocks $pattern]
    } else {
      set matches [find_clocks_matching $pattern $regexp $nocase]
      if { $matches != {} } {
	set clocks [concat $clocks $matches]
      } else {
	if {![info exists flags(-quiet)]} {
	  sta_warn 351 "clock '$pattern' not found."
	}
      }
    }
  }
  if [info exists keys(-filter)] {
    set clocks [filter_objs $keys(-filter) $clocks filter_clocks "clock"]
  }
  return $clocks
}

################################################################

define_cmd_args "get_lib_cells" \
  {[-hsc separator] [-regexp] [-nocase] [-quiet] [-filter expr]\
     [-of_objects objects] [patterns]}

define_cmd_alias "get_lib_cell" "get_lib_cells"

proc get_lib_cells { args } {
  global hierarchy_separator
  parse_key_args "get_lib_cells" args keys {-hsc -of_objects -filter} \
    flags {-regexp -nocase -quiet}
  check_nocase_flag flags

  set regexp [info exists flags(-regexp)]
  set nocase [info exists flags(-nocase)]
  set cells {}
  if [info exists keys(-of_objects)] {
    if { $args != {} } {
      sta_warn 352 "positional arguments not supported with -of_objects."
    }
    set insts [get_instances_error "objects" $keys(-of_objects)]
    foreach inst $insts {
      lappend cells [$inst liberty_cell]
    }
  } else {
    check_argc_eq0or1 "get_lib_cells" $args
    # Copy backslashes that will be removed by foreach.
    if { $args == {} } {
      set patterns "*"
    } else {
      set patterns [string map {\\ \\\\} [lindex $args 0]]
    }
    # Parse library_name/pattern.
    set divider $hierarchy_separator
    if [info exists keys(-hsc)] {
      set divider $keys(-hsc)
      check_path_divider $divider
    }
    set cell_regexp [cell_regexp_hsc $divider]
    set quiet [info exists flags(-quiet)]
    foreach pattern $patterns {
      if { [is_object $pattern] } {
	if { [object_type $pattern] != "LibertyCell" } {
	  sta_error 328 "object '$pattern' is not a liberty cell."
	}
	set cells [concat $cells $pattern]
      } else {
	if { ![regexp $cell_regexp $pattern ignore lib_name cell_pattern]} {
	  set lib_name "*"
	  set cell_pattern $pattern
	}
	# Allow wildcards in the library name (incompatible).
	set libs [get_libs -quiet $lib_name]
	if { $libs == {} } {
	  if {!$quiet} {
	    sta_warn 353 "library '$lib_name' not found."
	  }
	} else {
	  foreach lib $libs {
	    set matches [$lib find_liberty_cells_matching $cell_pattern \
	  		 $regexp $nocase]
	    if {$matches != {}} {
	      set cells [concat $cells $matches]
	    }
	  }
	  if { $cells == {} } {
	    if {!$quiet} {
	      sta_warn 354 "cell '$cell_pattern' not found."
	    }
	  }
	}
      }
    }
  }
  if [info exists keys(-filter)] {
    set cells [filter_objs $keys(-filter) $cells filter_lib_cells "liberty cell"]
  }
  return $cells
}

################################################################

define_cmd_args "get_lib_pins" \
  {[-hsc separator] [-regexp] [-nocase] [-quiet] [-filter expr] [patterns]}

define_cmd_alias "get_lib_pin" "get_lib_pins"

# "get_lib_ports" in sta terminology.
proc get_lib_pins { args } {
  global hierarchy_separator
  parse_key_args "get_lib_pins" args keys {-hsc -filter} flags {-regexp -nocase -quiet}
  check_argc_eq0or1 "get_lib_pins" $args
  check_nocase_flag flags
  
  set regexp [info exists flags(-regexp)]
  set nocase [info exists flags(-nocase)]
  set quiet [info exists flags(-quiet)]
  # Copy backslashes that will be removed by foreach.
  if { $args == {} } {
    set patterns "*/*"
  } else {
    set patterns [string map {\\ \\\\} [lindex $args 0]]
  }
  # Parse library_name/cell_name/pattern.
  set divider $hierarchy_separator
  if [info exists keys(-hsc)] {
    set divider $keys(-hsc)
    check_path_divider $divider
  }
  set port_regexp1 [port_regexp_hsc $divider]
  set port_regexp2 [cell_regexp_hsc $divider]
  set ports {}
  foreach pattern $patterns {
    if { [is_object $pattern] } {
      if { [object_type $pattern] != "LibertyPort" } {
	sta_error 329 "object '$pattern' is not a liberty pin."
      }
      set ports [concat $ports $pattern]
    } else {
      # match library/cell/port
      set libs {}
      if { [regexp $port_regexp1 $pattern ignore lib_name cell_name port_pattern] } {
	set libs [get_libs -quiet $lib_name]
	# match cell/port
      } elseif { [regexp $port_regexp2 $pattern ignore cell_name port_pattern] } {
	set libs [get_libs *]
      } else {
	if { !$quiet } {
	  sta_warn 355 "library/cell/port '$pattern' not found."
	}
	return {}
      }
      if { $libs != {} } {
	set found_match 0
	set cells {}
	foreach lib $libs {
	  set cells [$lib find_liberty_cells_matching $cell_name $regexp $nocase]
	  foreach cell $cells {
	    set matches [$cell find_liberty_ports_matching $port_pattern \
	  		 $regexp $nocase]
	    foreach match $matches {
	      lappend ports $match
	      set found_match 1
	    }
	  }
	}
	if { !$found_match } {
	  if { !$quiet } {
	    sta_warn 356 "port '$port_pattern' not found."
	  }
	}
      } else {
	if { !$quiet } {
	  sta_warn 357 "library '$lib_name' not found."
	}
      }
    }
  }
  if [info exists keys(-filter)] {
    set ports [filter_objs $keys(-filter) $ports filter_lib_pins "liberty port"]
  }
  return $ports
}

proc check_nocase_flag { flags_var } {
  upvar 1 $flags_var flags
  if { [info exists flags(-nocase)] && ![info exists flags(-regexp)] } {
    sta_warn 358 "-nocase ignored without -regexp."
  }
}

################################################################

define_cmd_args "get_libs" {[-regexp] [-nocase] [-quiet] [-filter expr] [patterns]}

define_cmd_alias "get_lib" "get_libs"

proc get_libs { args } {
  parse_key_args "get_libs" args keys {-filter} flags {-regexp -nocase -quiet}
  check_argc_eq0or1 "get_libs" $args
  check_nocase_flag flags
  
  # Copy backslashes that will be removed by foreach.
  if { $args == {} } {
    set patterns "*"
  } else {
    set patterns [string map {\\ \\\\} [lindex $args 0]]
  }
  set regexp [info exists flags(-regexp)]
  set nocase [info exists flags(-nocase)]
  set libs {}
  foreach pattern $patterns {
    if { [is_object $pattern] } {
      if { [object_type $pattern] != "LibertyLibrary" } {
	sta_error 330 "object '$pattern' is not a liberty library."
      }
      set libs [concat $libs $pattern]
    } else {
      set matches [find_liberty_libraries_matching $pattern $regexp $nocase]
      if {$matches != {}} {
	set libs [concat $libs $matches]
      } else {
	if {![info exists flags(-quiet)]} {
	  sta_warn 359 "library '$pattern' not found."
	}
      }
    }
  }
  if [info exists keys(-filter)] {
    set libs [filter_objs $keys(-filter) $libs filter_liberty_libraries "liberty library"]
  }
  return $libs
}

proc find_liberty_libraries_matching { pattern regexp nocase } {
  # Remove "lib.db:" reference from the library name.
  set ix [string last ".db:" $pattern]
  if { $ix != -1 } {
    set pattern2 [string range $pattern [expr $ix + 4] end]
  } else {
    set pattern2 $pattern
  }
  if { $regexp } {
    # Anchor the regexp pattern.
    set pattern2 "^${pattern}$"
  }
  set lib_iter [liberty_library_iterator]
  set matches {}
  while { [$lib_iter has_next] } {
    set lib [$lib_iter next]
    set lib_name [get_name $lib]
    if { (!$regexp && [string match $pattern2 $lib_name]) \
	   || ($regexp && $nocase && [regexp -nocase $pattern2 $lib_name]) \
	   || ($regexp && !$nocase && [regexp $pattern2 $lib_name]) } {
      lappend matches $lib
    }
  }
  $lib_iter finish
  return $matches
}

################################################################

define_cmd_args "get_nets" \
  {[-hierarchical] [-hsc separator] [-regexp] [-nocase] [-quiet] [-filter expr]\
     [-of_objects objects] [patterns]}

define_cmd_alias "get_net" "get_nets"

proc get_nets { args } {
  global hierarchy_separator
  
  parse_key_args get_nets args keys {-hsc -of_objects -filter} \
    flags {-hierarchical -regexp -nocase -quiet}
  check_nocase_flag flags
  
  set regexp [info exists flags(-regexp)]
  set nocase [info exists flags(-nocase)]
  set hierarchical [info exists flags(-hierarchical)]
  set quiet [info exists flags(-quiet)]
  # Copy backslashes that will be removed by foreach.
  if { $args == {} } {
    set patterns "*"
  } else {
    set patterns [string map {\\ \\\\} [lindex $args 0]]
  }
  if [info exists keys(-hsc)] {
    set divider $keys(-hsc)
    check_path_divider $divider
    set patterns [string map "$divider $hierarchy_separator" $patterns]
  }
  set nets {}
  if [info exists keys(-of_objects)] {
    if { $args != {} } {
      sta_warn 360 "patterns argument not supported with -of_objects."
    }
    parse_inst_pin_arg $keys(-of_objects) insts pins
    foreach inst $insts {
      set pin_iter [$inst pin_iterator]
      while { [$pin_iter has_next] } {
	set pin [$pin_iter next]
	lappend nets [$pin net]
      }
      $pin_iter finish
    }
    foreach pin $pins {
      lappend nets [$pin net]
    }
  } else {
    check_argc_eq0or1 "get_nets" $args
    foreach pattern $patterns {
      if { [is_object $pattern] } {
	if { [object_type $pattern] != "Net" } {
	  sta_error 331 "object '$pattern' is not a net."
	}
	set nets [concat $nets $pattern]
      } else {
	if { $hierarchical } {
	  set matches [find_nets_hier_matching $pattern $regexp $nocase]
	} else {
	  set matches [find_nets_matching $pattern $regexp $nocase]
	}
	set nets [concat $nets $matches]
	if { $matches == {} && !$quiet } {
	  sta_warn 361 "net '$pattern' not found."
	}
      }
    }
  }
  if [info exists keys(-filter)] {
    set nets [filter_objs $keys(-filter) $nets filter_nets "net"]
  }
  return $nets
}

################################################################

define_cmd_args "get_pins" \
  {[-hierarchical] [-hsc separator] [-quiet] [-filter expr]\
     [-regexp] [-nocase] [-of_objects objects] [patterns]}

define_cmd_alias "get_pin" "get_pins"

proc get_pins { args } {
  global hierarchy_separator
  
  parse_key_args "get_pins" args keys {-hsc -of_objects -filter} \
    flags {-hierarchical -regexp -nocase -quiet}
  check_nocase_flag flags
  
  set regexp [info exists flags(-regexp)]
  set nocase [info exists flags(-nocase)]
  set hierarchical [info exists flags(-hierarchical)]
  set quiet [info exists flags(-quiet)]
  set pins {}
  if [info exists keys(-of_objects)] {
    if { $args != {} } {
      sta_warn 362 "patterns argument not supported with -of_objects."
    }
    parse_inst_net_arg $keys(-of_objects) insts nets
    foreach inst $insts {
      set pin_iter [$inst pin_iterator]
      while { [$pin_iter has_next] } {
	set pin [$pin_iter next]
	lappend pins $pin
      }
      $pin_iter finish
    }
    foreach net $nets {
      set pin_iter [$net pin_iterator]
      while { [$pin_iter has_next] } {
	set pin [$pin_iter next]
	lappend pins $pin
      }
      $pin_iter finish
    }
  } else {
    check_argc_eq0or1 "get_pins" $args
    if { $args == {} } {
      set patterns "*"
    } else {
      set patterns [lindex $args 0]
    }
    if [info exists keys(-hsc)] {
      set divider $keys(-hsc)
      check_path_divider $divider
      set patterns [string map "$divider $hierarchy_separator" $patterns]
    }
    # Copy backslashes that will be removed by foreach.
    set patterns [string map {\\ \\\\} $patterns]
    foreach pattern $patterns {
      if { [is_object $pattern] } {
	if { [object_type $pattern] != "Pin" } {
	  sta_error 332 "object '$pattern' is not a pin."
	}
	set pins [concat $pins $pattern]
      } else {
	if { $hierarchical } {
	  set matches [find_pins_hier_matching $pattern $regexp $nocase]
	} else {
	  set matches [find_pins_matching $pattern $regexp $nocase]
	}
	set pins [concat $pins $matches]
	if { $matches == {} && !$quiet } {
	  sta_warn 363 "pin '$pattern' not found."
	}
      }
    }
  }
  if [info exists keys(-filter)] {
    set pins [filter_objs $keys(-filter) $pins filter_pins "pin"]
  }
  return $pins
}

################################################################

define_cmd_args "get_ports" \
  {[-quiet] [-filter expr] [-regexp] [-nocase] [-of_objects objects] [patterns]}

define_cmd_alias "get_port" "get_ports"

# Find top level design ports matching pattern.
proc get_ports { args } {
  parse_key_args "get_ports" args keys {-of_objects -filter} \
    flags {-regexp -nocase -quiet}
  check_nocase_flag flags
  
  set regexp [info exists flags(-regexp)]
  set nocase [info exists flags(-nocase)]
  # Copy backslashes that will be removed by foreach.
  if { $args == {} } {
    set patterns "*"
  } else {
    set patterns [string map {\\ \\\\} [lindex $args 0]]
  }
  set ports {}
  if [info exists keys(-of_objects)] {
    if { $args != {} } {
      sta_warn 365 "patterns argument not supported with -of_objects."
    }
    set nets [get_nets_arg "objects" $keys(-of_objects)]
    foreach net $nets {
      set ports [concat $ports [$net ports]]
    }
  } else {
    check_argc_eq0or1 "get_ports" $args
    foreach pattern $patterns {
      if { [is_object $pattern] } {
	if { [object_type $pattern] != "Port" } {
	  sta_error 333 "object '$pattern' is not a port."
	}
	set ports [concat $ports $pattern]
      } else {
        set matches [find_ports_matching $pattern $regexp $nocase]
        if { $matches != {} } {
	  set ports [concat $ports $matches]
        } else {
	  if {![info exists flags(-quiet)]} {
	    sta_warn 366 "port '$pattern' not found."
	  }
        }
      }
    }
  }
  if [info exists keys(-filter)] {
    set ports [filter_objs $keys(-filter) $ports filter_ports "port"]
  }
  return $ports
}

################################################################
#
# Timing Constraints
#
################################################################

define_cmd_args "create_clock" \
  {[-name name] [-period period] [-waveform waveform] [-add]\
     [-comment comment] [pins]}

proc create_clock { args } {
  parse_key_args "create_clock" args \
    keys {-name -period -waveform -comment} \
    flags {-add}
  
  check_argc_eq0or1 "create_clock" $args
  set argc [llength $args]
  if { $argc == 0 } {
    set pins {}
  } elseif { $argc == 1 } {
    set pins [get_port_pins_error "pins" [lindex $args 0]]
  }
  
  set add [info exists flags(-add)]
  if [info exists keys(-name)] {
    set name $keys(-name)
  } elseif { $pins != {} } {
    if { $add } {
      sta_error 368 "-add requires -name."
    }
    # Default clock name is the first pin name.
    set name [get_full_name [lindex $pins 0]]
  } else {
    sta_error 369 "-name or port_pin_list must be specified."
  }
  
  if [info exists keys(-period)] {
    set period $keys(-period)
    check_positive_float "period" $period
    set period [time_ui_sta $period]
  } else {
    sta_error 370 "missing -period argument."
  }
  
  if [info exists keys(-waveform)] {
    set wave_arg $keys(-waveform)
    if { [expr [llength $wave_arg] % 2] != 0 } {
      sta_error 371 "-waveform edge_list must have an even number of edge times."
    }
    set first_edge 1
    set prev_edge 0
    set waveform {}
    foreach edge $wave_arg {
      check_float "-waveform edge" $edge
      set edge [time_ui_sta $edge]
      if { !$first_edge && $edge < $prev_edge } {
	sta_error 372 "non-increasing clock -waveform edge times."
      }
      if { $edge > [expr $period * 2] } {
	sta_error 373 "-waveform time greater than two periods."
      }
      lappend waveform $edge
      set prev_edge $edge
      set first_edge 0
    }
  } else {
    set waveform [list 0 [expr $period / 2.0]]
  }
  
  set comment [parse_comment_key keys]
  
  make_clock $name $pins $add $period $waveform $comment
}

################################################################

define_cmd_args "delete_clock" {[-all] clocks}

proc delete_clock { args } {
  parse_key_args "delete_clock" args keys {} flags {-all}
  if { [info exists flags(-all)] } {
    check_argc_eq0 "delete_clock" $args
    set clks [all_clocks]
  } else {
    check_argc_eq1 "delete_clock" $args
    set clks [get_clocks_warn "clocks" [lindex $args 0]]
  }
  foreach clk $clks {
    remove_clock_cmd $clk
  }
}

################################################################

define_cmd_args "create_generated_clock" \
  {[-name clock_name] -source master_pin [-master_clock clock]\
     [-divide_by divisor | -multiply_by multiplier]\
     [-duty_cycle duty_cycle] [-invert] [-edges edge_list]\
     [-edge_shift edge_shift_list] [-combinational] [-add]\
     [-comment comment] port_pin_list}

proc create_generated_clock { args } {
  parse_key_args "create_generated_clock" args keys \
    {-name -source -master_clock -divide_by -multiply_by \
       -duty_cycle -edges -edge_shift -comment} \
    flags {-invert -combinational -add}
  
  check_argc_eq1 "create_generated_clock" $args
  parse_port_pin_net_arg [lindex $args 0] pins nets
  # Convert net args to net driver pin.
  foreach net $nets {
    set drivers [net_driver_pins $net]
    if { $drivers != {} } {
      lappend pins [lindex $drivers 0]
    }
  }
  if { $pins == {} } {
    sta_error 374 "empty ports/pins/nets argument."
  }
  set add [info exists flags(-add)]
  
  if [info exists keys(-name)] {
    set name $keys(-name)
  } elseif { $pins != {} } {
    if { $add } {
      sta_error 375 "-add requires -name."
    }
    # Default clock name is the first pin name.
    set name [get_full_name [lindex $pins 0]]
  } else {
    sta_error 376 "name or port_pin_list must be specified."
  }
  
  if [info exists keys(-source)] {
    set source $keys(-source)
    set source_pin [get_port_pin_error "master_pin" $source]
  } else {
    sta_error 377 "missing -source argument."
  }
  
  set master_clk "NULL"
  set divide_by 0
  set multiply_by 0
  set duty_cycle 0
  set edges {}
  set edge_shifts {}
  
  set combinational [info exists flags(-combinational)]
  
  if {[info exists keys(-master_clock)]} {
    set master_clk [get_clock_error "-master_clk" $keys(-master_clock)]
    if { $master_clk == "NULL" } {
      sta_error 378 "-master_clock argument empty."
    }
  } elseif { $add } {
    sta_error 379 "-add requireds -master_clock."
  }
  
  if {[info exists keys(-divide_by)] && [info exists keys(-multiply_by)]} {
    sta_error 380 "-multiply_by and -divide_by options are exclusive."
  } elseif {[info exists keys(-divide_by)]} {
    set divide_by $keys(-divide_by)
    if {![string is integer $divide_by] || $divide_by < 1} {
      sta_error 381 "-divide_by is not an integer greater than one."
    }
    if {$combinational && $divide_by != 1} {
      sta_error 382 "-combinational implies -divide_by 1."
    }
  } elseif {[info exists keys(-multiply_by)]} {
    set multiply_by $keys(-multiply_by)
    if {![string is integer $multiply_by] || $multiply_by < 1} {
      sta_error 383 "-multiply_by is not an integer greater than one."
    }
    if {[info exists keys(-duty_cycle)]} {
      set duty_cycle $keys(-duty_cycle)
      if {![string is double $duty_cycle] \
	    || $duty_cycle < 0.0 || $duty_cycle > 100.0} {
	sta_error 384 "-duty_cycle is not a float between 0 and 100."
      }
    }
  } elseif {[info exists keys(-edges)]} {
    set edges $keys(-edges)
    if { [llength $edges] != 3 } {
      sta_error 385 "-edges only supported for three edges."
    }
    set prev_edge [expr [lindex $edges 0] - 1]
    foreach edge $edges {
      check_cardinal "-edges" $edge
      if { $edge <= $prev_edge } {
	sta_error 386 "edges times are not monotonically increasing."
      }
    }
    if [info exists keys(-edge_shift)] {
      foreach shift $keys(-edge_shift) {
	check_float "-edge_shift" $shift
	lappend edge_shifts [time_ui_sta $shift]
      }
      if { [llength $edge_shifts] != [llength $edges] } {
	sta_error 387 "-edge_shift length does not match -edges length."
      }
    }
  } elseif { $combinational } {
    set divide_by 1
  } else {
    sta_error 388 "missing -multiply_by, -divide_by, -combinational or -edges argument."
  }
  
  set invert 0
  if {[info exists flags(-invert)]} {
    if {!([info exists keys(-divide_by)] \
	    || [info exists keys(-multiply_by)] \
	    || [info exists flags(-combinational)])} {
      sta_error 389 "cannot specify -invert without -multiply_by, -divide_by or -combinational."
    }
    set invert 1
  }
  
  if {[info exists keys(-duty_cycle)] && ![info exists keys(-multiply_by)]} {
    sta_error 390 "-duty_cycle requires -multiply_by value."
  }
  
  set comment [parse_comment_key keys]
  
  make_generated_clock $name $pins $add $source_pin $master_clk \
    $divide_by $multiply_by $duty_cycle $invert $combinational \
    $edges $edge_shifts $comment
}

################################################################

define_cmd_args "delete_generated_clock" {[-all] clocks}

proc delete_generated_clock { args } {
  remove_gclk_cmd "delete_generated_clock" $args
}

proc remove_gclk_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {} flags {-all}
  if { [info exists flags(-all)] } {
    check_argc_eq0 $cmd $cmd_args
    set clks [all_clocks]
  } else {
    check_argc_eq1 $cmd $cmd_args
    set clks [get_clocks_warn "clocks" [lindex $cmd_args 0]]
  }
  foreach clk $clks {
    if { [$clk is_generated] } {
      remove_clock_cmd $clk
    }
  }
}

################################################################

define_cmd_args "group_path" \
  {-name group_name [-weight weight] [-critical_range range]\
     [-default] [-comment comment]\
     [-from from_list] [-rise_from from_list] [-fall_from from_list]\
     [-through through_list] [-rise_through through_list]\
     [-fall_through through_list] [-to to_list] [-rise_to to_list]\
     [-fall_to to_list]}

# The -weight and -critical_range arguments are ignored.
proc group_path { args } {
  parse_key_args "group_path" args \
    keys {-name -weight -critical_range \
	    -from -rise_from -fall_from \
	    -to -rise_to -fall_to -comment} \
    flags {-default} 0
  
  set cmd "group_path"
  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg args arg_error]
  set to [parse_to_arg1 keys "rise_fall" arg_error]
  check_exception_pins $from $to
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
    sta_error 391 "group_path command failed."
    return 0
  }
  
  check_for_key_args $cmd args
  if { $args != {} } {
    delete_from_thrus_to $from $thrus $to
    sta_error 392 "positional arguments not supported."
  }
  if { ($from == "NULL" && $thrus == "" && $to == "NULL") } {
    delete_from_thrus_to $from $thrus $to
    sta_error 393 "-from, -through or -to required."
  }
  
  set default [info exists flags(-default)]
  set name_exists [info exists keys(-name)]
  if { $default && $name_exists } {
    sta_error 394 "-name and -default are mutually exclusive."
  } elseif { !$name_exists && !$default } {
    sta_error 395 "-name or -default option is required."
  } elseif { $default } {
    set name ""
  } else {
    set name $keys(-name)
  }
  
  set comment [parse_comment_key keys]
  
  make_group_path $name $default $from $thrus $to $comment
}

proc check_exception_pins { from to } {
  variable sdc_file
  variable sdc_line
  if { [info exists sdc_file] } {
    set file $sdc_file
    set line $sdc_line
  } else {
    set file ""
    set line 0
  }
  check_exception_from_pins $from $file $line
  check_exception_to_pins $to $file $line
}

################################################################

define_cmd_args "set_clock_gating_check" \
  {[-setup setup_time] [-hold hold_time] [-rise] [-fall]\
     [-low] [-high] [objects]}

proc set_clock_gating_check { args } {
  parse_key_args "set_clock_gating_check" args keys {-setup -hold } \
    flags {-rise -fall -high -low}
  
  check_argc_eq0or1 "set_clock_gating_check" $args
  set rf [parse_rise_fall_flags flags]
  
  set active_value ""
  if {[info exists flags(-high)] && [info exists flags(-low)]} {
    sta_error 396 "cannot specify both -high and -low."
  } elseif [info exists flags(-low)] {
    set active_value "0"
  } elseif [info exists flags(-high)] {
    set active_value "1"
  }
  
  if { !([info exists keys(-hold)] || [info exists keys(-setup)]) } {
    sta_error 397 "missing -setup or -hold argument."
  }
  if [info exists keys(-hold)] {
    set_clock_gating_check1 $args $rf "min" $keys(-hold) $active_value
  }
  if [info exists keys(-setup)] {
    set_clock_gating_check1 $args $rf "max" $keys(-setup) $active_value
  }
}

proc set_clock_gating_check1 { args rf setup_hold margin active_value } {
  set margin [time_ui_sta $margin]
  if { [llength $args] == 0 } {
    if { $active_value != "" } {
      sta_error 398 "-high and -low only permitted for pins and instances."
    }
    set_clock_gating_check_cmd $rf $setup_hold $margin
  } elseif { [llength $args] == 1 } {
    parse_clk_inst_port_pin_arg [lindex $args 0] clks insts pins
    
    if { $clks != {} && $active_value != "" } {
      sta_error 399 "-high and -low only permitted for pins and instances."
    }
    foreach clk $clks {
      set_clock_gating_check_clk_cmd $clk $rf $setup_hold $margin
    }
    
    if { $active_value == "" } {
      set active_value "X"
    }
    foreach pin $pins {
      set_clock_gating_check_pin_cmd $pin $rf $setup_hold \
	$margin $active_value
    }
    foreach inst $insts {
      set_clock_gating_check_instance_cmd $inst $rf $setup_hold \
	$margin $active_value
    }
  }
}

################################################################

define_cmd_args "set_clock_groups" \
  {[-name name] [-logically_exclusive] [-physically_exclusive]\
     [-asynchronous] [-allow_paths] [-comment comment] -group clocks}

proc set_clock_groups { args } {
  parse_key_args "set_clock_groups" args \
    keys {-name -comment} \
    flags {-logically_exclusive -physically_exclusive \
	     -asynchronous -allow_paths} 0
  
  if {[info exists keys(-name)]} {
    set name $keys(-name)
  } else {
    set name ""
  }
  set logically_exclusive [info exists flags(-logically_exclusive)]
  set physically_exclusive [info exists flags(-physically_exclusive)]
  set asynchronous [info exists flags(-asynchronous)]
  set allow_paths [info exists flags(-allow_paths)]
  
  if { ($logically_exclusive+$physically_exclusive+$asynchronous) == 0 } {
    sta_error 400 "one of -logically_exclusive, -physically_exclusive or -asynchronous is required."
  }
  if { ($logically_exclusive+$physically_exclusive+$asynchronous) > 1 } {
    sta_error 401 "the keywords -logically_exclusive, -physically_exclusive and -asynchronous are mutually exclusive."
  }
  
  set comment [parse_comment_key keys]
  
  set clk_groups [make_clock_groups $name $logically_exclusive \
		    $physically_exclusive $asynchronous $allow_paths \
		    $comment]
  
  while { $args != "" } {
    set arg [lindex $args 0]
    if {[string match $arg "-group"]} {
      set group_clks [get_clocks_warn "clocks" [lindex $args 1]]
      if { $group_clks != {} } {
	clock_groups_make_group $clk_groups $group_clks
      }
      set args [lrange $args 2 end]
    } else {
      if {[is_keyword_arg $arg]} {
	sta_warn 402 "unknown keyword argument $arg."
      } else {
	sta_warn 403 "extra positional argument $arg."
      }
      set args [lrange $args 1 end]
    }
  }
}

################################################################

define_cmd_args "unset_clock_groups" \
  {[-logically_exclusive] [-physically_exclusive]\
     [-asynchronous] [-name names] [-all]}
				
proc unset_clock_groups { args } {
  unset_clk_groups_cmd "unset_clock_groups" $args
}

proc unset_clk_groups_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-name} \
    flags {-logically_exclusive -physically_exclusive -asynchronous -all}

  set all [info exists flags(-all)]
  set names {}
  if {[info exists keys(-name)]} {
    set names $keys(-name)
  }

  if { $all && $names != {} } {
    sta_error 404 "the -all and -name options are mutually exclusive."
  }
  if { !$all && $names == {} } {
    sta_error 405 "either -all or -name options must be specified."
  }

  set logically_exclusive [info exists flags(-logically_exclusive)]
  set physically_exclusive [info exists flags(-physically_exclusive)]
  set asynchronous [info exists flags(-asynchronous)]

  if { ($logically_exclusive+$physically_exclusive+$asynchronous) == 0 } {
    sta_error 406 "one of -logically_exclusive, -physically_exclusive or -asynchronous is required."
  }
  if { ($logically_exclusive+$physically_exclusive+$asynchronous) > 1 } {
    sta_error 407 "the keywords -logically_exclusive, -physically_exclusive and -asynchronous are mutually exclusive."
  }

  if { $all } {
    if { $logically_exclusive } {
      unset_clock_groups_logically_exclusive "NULL"
    } elseif { $physically_exclusive } {
      unset_clock_groups_physically_exclusive "NULL"
    } elseif { $asynchronous } {
      unset_clock_groups_asynchronous "NULL"
    }
  } else {
    foreach name $names {
      if { $logically_exclusive } {
	unset_clock_groups_logically_exclusive $name
      } elseif { $physically_exclusive } {
	unset_clock_groups_physically_exclusive $name
      } elseif { $asynchronous } {
	unset_clock_groups_asynchronous $name
      }
    }
  }
}

################################################################

define_cmd_args "set_clock_latency" \
  {[-source] [-clock clock] [-rise] [-fall] [-min] [-max]\
     [-early] [-late] delay objects}

proc set_clock_latency { args } {
  parse_key_args "set_clock_latency" args keys {-clock} \
    flags {-rise -fall -min -max -source -late -early}
  
  check_argc_eq2 "set_clock_latency" $args
  
  set delay [lindex $args 0]
  check_float "delay" $delay
  set delay [time_ui_sta $delay]
  set objects [lindex $args 1]
  
  parse_clk_port_pin_arg $objects clks pins
  
  set rf [parse_rise_fall_flags flags]
  set min_max [parse_min_max_all_flags flags]
  
  set pin_clk "NULL"
  if { [info exists keys(-clock)] } {
    set pin_clk [get_clock_warn "clock" $keys(-clock)]
    if { $clks != {} } {
      sta_warn 408 "-clock ignored for clock objects."
    }
  }
  
  if {[info exists flags(-source)]} {
    # Insertion delay (source latency).
    set early_late [parse_early_late_all_flags flags]
    
    foreach clk $clks {
      set_clock_insertion_cmd $clk "NULL" $rf $min_max $early_late $delay
    }
    foreach pin $pins {
      # Source only allowed on clocks and clock pins.
      if { ![is_clock_src $pin] } {
	sta_error 409 "-source '[get_full_name $pin]' is not a clock pin."
      }
      set_clock_insertion_cmd $pin_clk $pin $rf $min_max $early_late $delay
    }
  } else {
    # Latency.
    if {[info exists flags(-early)] || [info exists flags(-late)]} {
      sta_error 410 "-early/-late is only allowed with -source."
    }
    
    foreach clk $clks {
      set_clock_latency_cmd $clk "NULL" $rf $min_max $delay
    }
    foreach pin $pins {
      set_clock_latency_cmd $pin_clk $pin $rf $min_max $delay
    }
  }
}

################################################################

define_cmd_args "unset_clock_latency" {[-source] [-clock clock] objects}

proc unset_clock_latency { args } {
  unset_clk_latency_cmd "unset_clock_latency" $args
}

proc unset_clk_latency_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {-clock} flags {-source}
  check_argc_eq1 $cmd $cmd_args
  set objects [lindex $cmd_args 0]
  parse_clk_port_pin_arg $objects clks pins
  set pin_clk "NULL"
  if { [info exists keys(-clock)] } {
    set pin_clk [get_clock_warn "clock" $keys(-clock)]
    if { $clks != {} } {
      sta_warn 411 "-clock ignored for clock objects."
    }
  }

  if {[info exists flags(-source)]} {
    # Source latency.
    foreach clk $clks {
      unset_clock_insertion_cmd $clk "NULL"
    }
    foreach pin $pins {
      # Source only allowed on clocks and clock pins.
      if { ![is_clock_pin $pin] } {
	sta_error 412 "-source '[$pin path_name]' is not a clock pin."
      }
      unset_clock_insertion_cmd $pin_clk $pin
    }
  } else {
    # Latency.
    foreach clk $clks {
      unset_clock_latency_cmd $clk "NULL"
    }
    foreach pin $pins {
      unset_clock_latency_cmd $pin_clk $pin
    }
  }
}

################################################################

define_cmd_args "set_sense" \
  {[-type clock|data] [-positive] [-negative] [-pulse pulse_type]\
     [-stop_propagation] [-clocks clocks] pins}

proc set_sense { args } {
  parse_key_args "set_sense" args keys {-type} flags {} 0
  
  set type "clock"
  if { [info exists keys(-type)] } {
    set type $keys(-type)
    if { $type == "data" } {
      sta_warn 413 "set_sense -type data not supported."
    } elseif { $type == "clock" } {
      set_clock_sense_cmd1 "set_sense" $args
    } else {
      sta_error 414 "set_sense -type clock|data"
    }
  }
}

# deprecated in SDC 2.1
define_cmd_args "set_clock_sense" \
  {[-positive] [-negative] [-pulse pulse_type] [-stop_propagation] \
     [-clock clocks] pins}

proc set_clock_sense { args } {
  sta_warn 415 "set_clock_sense is deprecated as of SDC 2.1. Use set_sense -type clock."
  set_clock_sense_cmd1 "set_clock_sense" $args
}

proc set_clock_sense_cmd1 { cmd cmd_args } {
  # SDC uses -clock, OT, OC use -clocks
  parse_key_args $cmd cmd_args keys {-clock -clocks -pulse} \
    flags {-positive -negative -stop_propagation} 0
  check_argc_eq1 $cmd $cmd_args
  
  set pulse [info exists keys(-pulse)]
  if { $pulse } {
    sta_warn 416 "-pulse argument not supported."
  }
  set positive [info exists flags(-positive)]
  set negative [info exists flags(-negative)]
  set stop_propagation [info exists flags(-stop_propagation)]
  if { ($positive && ($negative || $stop_propagation || $pulse)) \
	 || ($negative && ($positive || $stop_propagation || $pulse)) \
	 || ($stop_propagation && ($positive || $negative || $pulse))
       || ($pulse && ($positive || $negative || $stop_propagation)) } {
    sta_warn 417 "-positive, -negative, -stop_propagation and -pulse are mutually exclusive."
  }
  
  set pins [get_port_pins_error "pins" [lindex $cmd_args 0]]
  set clks {}
  if {[info exists keys(-clock)]} {
    set clks [get_clocks_warn "clock" $keys(-clock)]
  }
  if {[info exists keys(-clocks)]} {
    set clks [get_clocks_warn "clocks" $keys(-clocks)]
  }
  foreach pin $pins {
    if {[$pin is_hierarchical]} {
      sta_warn 418 "hierarchical pin '[get_full_name $pin]' not supported."
    }
  }
  set_clock_sense_cmd $pins $clks $positive $negative $stop_propagation
}

################################################################

define_cmd_args "set_clock_transition" \
  {[-rise] [-fall] [-min] [-max] transition clocks}

proc set_clock_transition { args } {
  parse_key_args "set_clock_transition" args keys {} \
    flags {-rise -fall -max -min}
  
  set rf [parse_rise_fall_flags flags]
  set min_max [parse_min_max_all_flags flags]
  check_argc_eq2 "set_clock_transition" $args
  
  set slew [lindex $args 0]
  set clks [get_clocks_warn "clocks" [lindex $args 1]]
  
  foreach clk $clks {
    if { [$clk is_virtual] } {
      sta_warn 419 "transition time can not be specified for virtual clocks."
    } else {
      set_clock_slew_cmd $clk $rf $min_max [time_ui_sta $slew]
    }
  }
}

################################################################

define_cmd_args "unset_clock_transition" {clocks}

proc unset_clock_transition { args } {
  check_argc_eq1 "unset_clock_transition" $args
  set clks [get_clocks_warn "clocks" [lindex $args 0]]
  foreach clk $clks {
    unset_clock_slew_cmd $clk
  }
}

################################################################

# -rise/-fall are obsolete.
define_cmd_args "set_clock_uncertainty" \
  {[-from|-rise_from|-fall_from from_clock]\
     [-to|-rise_to|-fall_to to_clock] [-rise] [-fall]\
     [-setup] [-hold] uncertainty [objects]}

proc set_clock_uncertainty { args } {
  parse_key_args "set_clock_uncertainty" args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to} \
    flags {-rise -fall -setup -hold}
  
  if { [llength $args] == 0 } {
    sta_error 420 "missing uncertainty value."
  }
  set uncertainty [lindex $args 0]
  check_float "uncertainty" $uncertainty
  set uncertainty [time_ui_sta $uncertainty]
  
  set min_max "min_max"
  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set min_max "max"
  }
  if { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set min_max "min"
  }
  
  if { [info exists keys(-from)] } {
    set from_key "-from"
    set from_rf "rise_fall"
  } elseif { [info exists keys(-rise_from)] } {
    set from_key "-rise_from"
    set from_rf "rise"
  } elseif { [info exists keys(-fall_from)] } {
    set from_key "-fall_from"
    set from_rf "fall"
  } else {
    set from_key "none"
  }
  
  if { [info exists keys(-to)] } {
    set to_key "-to"
    if { [info exists flags(-rise)] } {
      set to_rf "rise"
    } elseif { [info exists flags(-fall)] } {
      set to_rf "fall"
    } else {
      set to_rf "rise_fall"
    }
  } elseif { [info exists keys(-rise_to)] } {
    set to_key "-rise_to"
    set to_rf "rise"
  } elseif { [info exists keys(-fall_to)] } {
    set to_key "-fall_to"
    set to_rf "fall"
  } else {
    set to_key "none"
  }
  
  if { $from_key != "none" && $to_key == "none" \
	 || $from_key == "none" && $to_key != "none" } {
    sta_error 421 "-from/-to must be used together."
  } elseif { $from_key != "none" && $to_key != "none" } {
    # Inter-clock uncertainty.
    check_argc_eq1 "-from/-to" $args
    
    # -from/-to can be lists.
    set from_clks [get_clocks_warn "from_clocks" $keys($from_key)]
    set to_clks [get_clocks_warn "to_clocks" $keys($to_key)]
    
    foreach from_clk $from_clks {
      foreach to_clk $to_clks {
	set_inter_clock_uncertainty $from_clk $from_rf \
	  $to_clk $to_rf $min_max $uncertainty
      }
    }
  } else {
    # Single clock uncertainty.
    check_argc_eq2 "set_clock_uncertainty" $args
    if { [info exists flags(-rise)] \
	   || [info exists flags(-fall)] } {
      sta_error 422 "-rise, -fall options not allowed for single clock uncertainty."
    }
    set objects [lindex $args 1]
    parse_clk_port_pin_arg $objects clks pins
    
    foreach clk $clks {
      set_clock_uncertainty_clk $clk $min_max $uncertainty
    }
    foreach pin $pins {
      set_clock_uncertainty_pin $pin $min_max $uncertainty
    }
  }
}

################################################################

define_cmd_args "unset_clock_uncertainty" \
  {[-from|-rise_from|-fall_from from_clock]\
     [-to|-rise_to|-fall_to to_clock] [-rise] [-fall]\
     [-setup] [-hold] [objects]}

proc unset_clock_uncertainty { args } {
  unset_clk_uncertainty_cmd "unset_clock_uncertainty" $args
}

proc unset_clk_uncertainty_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to} \
    flags {-rise -fall -setup -hold}

  set min_max "min_max"
  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set min_max "max"
  }
  if { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set min_max "min"
  }

  if { [info exists keys(-from)] } {
    set from_key "-from"
    set from_rf "rise_fall"
  } elseif { [info exists keys(-rise_from)] } {
    set from_key "-rise_from"
    set from_rf "rise"
  } elseif { [info exists keys(-fall_from)] } {
    set from_key "-fall_from"
    set from_rf "fall"
  } else {
    set from_key "none"
  }

  if { [info exists keys(-to)] } {
    set to_key "-to"
    set to_rf "rise_fall"
  } elseif { [info exists keys(-rise_to)] } {
    set to_key "-rise_to"
    set to_rf "rise"
  } elseif { [info exists keys(-fall_to)] } {
    set to_key "-fall_to"
    set to_rf "fall"
  } else {
    set to_key "none"
  }

  if { $from_key != "none" && $to_key == "none" \
	 || $from_key == "none" && $to_key != "none" } {
    sta_error 423 "-from/-to must be used together."
  } elseif { $from_key != "none" && $to_key != "none" } {
    # Inter-clock uncertainty.
    check_argc_eq0 "unset_clock_uncertainty" $cmd_args

    # -from/-to can be lists.
    set from_clks [get_clocks_warn "from_clocks" $keys($from_key)]
    set to_clks [get_clocks_warn "to_clocks" $keys($to_key)]

    foreach from_clk $from_clks {
      foreach to_clk $to_clks {
	unset_inter_clock_uncertainty $from_clk $from_rf \
	  $to_clk $to_rf $min_max
      }
    }
  } else {
    # Single clock uncertainty.
    check_argc_eq1 $cmd $cmd_args
    if { [info exists keys(-rise)] \
	   || [info exists keys(-fall)] } {
      sta_error 424 "-rise, -fall options not allowed for single clock uncertainty."
    }
    set objects [lindex $cmd_args 0]
    parse_clk_port_pin_arg $objects clks pins

    foreach clk $clks {
      unset_clock_uncertainty_clk $clk $min_max
    }
    foreach pin $pins {
      unset_clock_uncertainty_pin $pin $min_max
    }
  }
}

################################################################

define_cmd_args "set_data_check" \
  {[-from from_pin] [-rise_from from_pin] [-fall_from from_pin]\
     [-to to_pin] [-rise_to to_pin] [-fall_to to_pin]\
     [-setup | -hold] [-clock clock] margin}

proc set_data_check { args } {
  parse_key_args "set_data_check" args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to -clock} \
    flags {-setup -hold}
  check_argc_eq1 "set_data_check" $args
  
  set margin [time_ui_sta $args]
  set from_rf "rise_fall"
  set to_rf "rise_fall"
  set clk "NULL"
  
  if [info exists keys(-from)] {
    set from [get_port_pin_error "from_pin" $keys(-from)]
  } elseif [info exists keys(-rise_from)] {
    set from [get_port_pin_error "from_pin" $keys(-rise_from)]
    set from_rf "rise"
  } elseif [info exists keys(-fall_from)] {
    set from [get_port_pin_error "from_pin" $keys(-fall_from)]
    set from_rf "fall"
  } else {
    sta_error 425 "missing -from, -rise_from or -fall_from argument."
  }
  
  if [info exists keys(-to)] {
    set to [get_port_pin_error "to_pin" $keys(-to)]
  } elseif [info exists keys(-rise_to)] {
    set to [get_port_pin_error "to_pin" $keys(-rise_to)]
    set to_rf "rise"
  } elseif [info exists keys(-fall_to)] {
    set to [get_port_pin_error "to_pin" $keys(-fall_to)]
    set to_rf "fall"
  } else {
    sta_error 426 "missing -to, -rise_to or -fall_to argument."
  }
  
  if [info exists keys(-clock)] {
    set clk [get_clock_warn "clock" $keys(-clock)]
  }
  
  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set setup_hold "setup"
  } elseif { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set setup_hold "hold"
  } else {
    set setup_hold "setup_hold"
  }
  
  set_data_check_cmd $from $from_rf $to $to_rf $clk $setup_hold $margin
}

################################################################

define_cmd_args "unset_data_check" \
  {[-from from_pin] [-rise_from from_pin] [-fall_from from_pin]\
     [-to to_pin] [-rise_to to_pin] [-fall_to to_pin]\
     [-setup | -hold] [-clock clock]}

proc unset_data_check { args } {
  unset_data_checks_cmd "unset_data_check" $args
}

proc unset_data_checks_cmd { cmd cmd_args } {
  parse_key_args cmd cmd_args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to -clock} \
    flags {-setup -hold}
  check_argc_eq0 $cmd $cmd_args

  set from_rf "rise_fall"
  set to_rf "rise_fall"
  set clk "NULL"
  set setup_hold "max"
  if [info exists keys(-from)] {
    set from [get_port_pin_error "from_pin" $keys(-from)]
  } elseif [info exists keys(-rise_from)] {
    set from [get_port_pin_error "from_pin" $keys(-rise_from)]
    set from_rf "rise"
  } elseif [info exists keys(-fall_from)] {
    set from [get_port_pin_error "from_pin" $keys(-fall_from)]
    set from_rf "fall"
  } else {
    sta_error 427 "missing -from, -rise_from or -fall_from argument."
  }

  if [info exists keys(-to)] {
    set to [get_port_pin_error "to_pin" $keys(-to)]
  } elseif [info exists keys(-rise_to)] {
    set to [get_port_pin_error "to_pin" $keys(-rise_to)]
    set to_rf "rise"
  } elseif [info exists keys(-fall_to)] {
    set to [get_port_pin_error "to_pin" $keys(-fall_to)]
    set to_rf "fall"
  } else {
    sta_error 428 "missing -to, -rise_to or -fall_to argument."
  }

  if [info exists keys(-clock)] {
    set clk [get_clock_warn "clock" $keys(-clock)]
  }

  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set setup_hold "setup"
  } elseif { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set setup_hold "hold"
  } else {
    set setup_hold "setup_hold"
  }

  unset_data_check_cmd $from $from_rf $to $to_rf $clk $setup_hold
}

################################################################

define_cmd_args "set_disable_timing" \
  {[-from from_port] [-to to_port] objects}

# Parallax supports -from or -to alone.
# OT requires both -from and -to args.
proc set_disable_timing { args } {
  parse_key_args "set_disable_timing" args keys {-from -to} flags {}
  check_argc_eq1 "set_disable_timing" $args
  
  set from {}
  if { [info exists keys(-from)] } {
    set from $keys(-from)
  }
  set to {}
  if { [info exists keys(-to)] } {
    set to $keys(-to)
  }
  parse_libcell_libport_inst_port_pin_edge_timing_arc_set_arg $args \
    libcells libports insts ports pins edges timing_arc_sets
  
  if { ([info exists keys(-from)] || [info exists keys(-to)]) \
	 && ($libports != {} || $pins != {} || $ports != {}) } {
    sta_warn 429 "-from/-to keywords ignored for lib_pin, port and pin arguments."
  }
  
  foreach libcell $libcells {
    set_disable_timing_cell $libcell $from $to
  }
  foreach libport $libports {
    disable_lib_port $libport
  }
  foreach inst $insts {
    set_disable_timing_instance $inst $from $to
  }
  foreach pin $pins {
    disable_pin $pin
  }
  foreach port $ports {
    disable_port $port
  }
  foreach edge $edges {
    disable_edge $edge
  }
  foreach timing_arc_set $timing_arc_sets {
    disable_timing_arc_set $timing_arc_set
  }
}

proc set_disable_timing_instance { inst from to } {
  set from_ports [parse_disable_inst_ports $inst $from]
  set to_ports [parse_disable_inst_ports $inst $to]
  if { ![$inst is_leaf] } {
    sta_error 430 "-from/-to hierarchical instance not supported."
  }
  if { $from_ports == "NULL" && $to_ports == "NULL" } {
    disable_instance $inst "NULL" "NULL"
  } elseif { $from_ports == "NULL" } {
    foreach to_port $to_ports {
      disable_instance $inst "NULL" $to_port
    }
  } elseif { $to_ports == "NULL" } {
    foreach from_port $from_ports {
      disable_instance $inst $from_port "NULL"
    }
  } else {
    foreach from_port $from_ports {
      foreach to_port $to_ports {
	disable_instance $inst $from_port $to_port
      }
    }
  }
}

# Find ports in inst's cell that have pins.
# Bus ports are expanded into a list.
proc parse_disable_inst_ports { inst port_name } {
  global hierarchy_separator
  
  if { $port_name == "" } {
    set ports "NULL"
  } else {
    set cell [instance_property $inst cell]
    set port [$cell find_port $port_name]
    if { $port == "NULL" } {
      sta_error 431 "pin '[get_full_name $inst]${hierarchy_separator}${port_name}' not found."
    } else {
      set lib_port [get_property $port liberty_port]
      set ports [port_members $lib_port]
    }
  }
  return $ports
}

proc port_members { port } {
  if [$port is_bus] {
    # Expand bus ports.
    set ports {}
    set member_iter [$port member_iterator]
    while {[$member_iter has_next]} {
      set bit_port [$member_iter next]
      lappend ports $bit_port
    }
    $member_iter finish
    return $ports
  } else {
    return $port
  }
}

proc set_disable_timing_cell { cell from to } {
  set from_ports [parse_disable_cell_ports $cell $from]
  set to_ports [parse_disable_cell_ports $cell $to]
  if { $from_ports == "NULL" && $to_ports == "NULL" } {
    disable_cell $cell "NULL" "NULL"
  } elseif { $from_ports == "NULL" } {
    foreach to_port $to_ports {
      disable_cell $cell "NULL" $to_port
    }
  } elseif { $to_ports == "NULL" } {
    foreach from_port $from_ports {
      disable_cell $cell $from_port "NULL"
    }
  } else {
    foreach from_port $from_ports {
      foreach to_port $to_ports {
        disable_cell $cell $from_port $to_port
      }
    }
  }
}

# Find cell ports.
# Bus ports are expanded into a list.
proc parse_disable_cell_ports { cell port_name } {
  global hierarchy_separator
  
  if { $port_name == "" } {
    set ports "NULL"
  } else {
    set port [$cell find_liberty_port $port_name]
    if { $port == "NULL" } {
      sta_error 432 "pin '[get_name $cell]${hierarchy_separator}${port_name}' not found."
    } else {
      set ports [port_members $port]
    }
  }
  return $ports
}

################################################################

define_cmd_args "unset_disable_timing" \
  {[-from from_port] [-to to_port] objects}

proc unset_disable_timing { args } {
  unset_disable_cmd "unset_disable_timing" $args
}

proc unset_disable_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args keys {-from -to} flags {}
  check_argc_eq1 $cmd $cmd_args
  
  set from ""
  if { [info exists keys(-from)] } {
    set from $keys(-from)
  }
  set to ""
  if { [info exists keys(-to)] } {
    set to $keys(-to)
  }
  parse_libcell_libport_inst_port_pin_edge_timing_arc_set_arg $cmd_args \
    libcells libports insts ports pins edges timing_arc_sets
  
  if { ([info exists keys(-from)] || [info exists keys(-to)]) \
         && ($libports != {} || $pins != {} || $ports != {}) } {
    sta_warn 434 "-from/-to keywords ignored for lib_pin, port and pin arguments."
  }
  
  foreach libcell $libcells {
    unset_disable_timing_cell $libcell $from $to
  }
  foreach libport $libports {
    unset_disable_lib_port $libport
  }
  foreach inst $insts {
    unset_disable_timing_instance $inst $from $to
  }
  foreach pin $pins {
    unset_disable_pin $pin
  }
  foreach port $ports {
    unset_disable_port $port
  }
  foreach edge $edges {
    unset_disable_edge $edge
  }
  foreach timing_arc_set $timing_arc_sets {
    unset_disable_timing_arc_set $timing_arc_set
  }
}

proc unset_disable_timing_cell { cell from to } {
  set from_ports [parse_disable_cell_ports $cell $from]
  set to_ports [parse_disable_cell_ports $cell $to]
  if { $from_ports == "NULL" && $to_ports == "NULL" } {
    unset_disable_cell $cell "NULL" "NULL"
  } elseif { $from_ports == "NULL" } {
    foreach to_port $to_ports {
      unset_disable_cell $cell "NULL" $to_port
    }
  } elseif { $to_ports == "NULL" } {
    foreach from_port $from_ports {
      unset_disable_cell $cell $from_port "NULL"
    }
  } else {
    foreach from_port $from_ports {
      foreach to_port $to_ports {
        unset_disable_cell $cell $from_port $to_port
      }
    }
  }
}

proc unset_disable_timing_instance { inst from to } {
  set from_ports [parse_disable_inst_ports $inst $from]
  set to_ports [parse_disable_inst_ports $inst $to]
  if { ![$inst is_leaf] } {
    sta_error 435 "-from/-to hierarchical instance not supported."
  }
  if { $from_ports == "NULL" && $to_ports == "NULL" } {
    unset_disable_instance $inst "NULL" "NULL"
  } elseif { $from_ports == "NULL" } {
    foreach to_port $to_ports {
      unset_disable_instance $inst "NULL" $to_port
    }
  } elseif { $to_ports == "NULL" } {
    foreach from_port $from_ports {
      unset_disable_instance $inst $from_port "NULL"
    }
  } else {
    foreach from_port $from_ports {
      foreach to_port $to_ports {
        unset_disable_instance $inst $from_port $to_port
      }
    }
  }
}

################################################################

define_cmd_args "set_false_path" \
  {[-setup] [-hold] [-rise] [-fall] [-reset_path] [-comment comment]\
     [-from from_list] [-rise_from from_list] [-fall_from from_list]\
     [-through through_list] [-rise_through through_list]\
     [-fall_through through_list] [-to to_list] [-rise_to to_list]\
     [-fall_to to_list]}

proc set_false_path { args } {
  parse_key_args "set_false_path" args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to -comment} \
    flags {-setup -hold -rise -fall -reset_path} 0
  
  set min_max "min_max"
  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set min_max "max"
  }
  if { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set min_max "min"
  }
  
  set cmd "set_false_path"
  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg args arg_error]
  set to [parse_to_arg keys flags arg_error]
  check_exception_pins $from $to
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
  } else {
    check_for_key_args $cmd args
    if { $args != {} } {
      sta_warn 436 "'$args' ignored."
    }
    if { ($from == "NULL" && $thrus == "" && $to == "NULL") } {
      delete_from_thrus_to $from $thrus $to
      sta_warn 437 "-from, -through or -to required."
    } else {
      if [info exists flags(-reset_path)] {
        reset_path_cmd $from $thrus $to $min_max
      }
      
      set comment [parse_comment_key keys]
      
      make_false_path $from $thrus $to $min_max $comment
    }
  }
}

################################################################

define_cmd_args "set_ideal_latency" \
  {[-rise] [-fall] [-min] [-max] delay objects}

proc set_ideal_latency { args } {
  # ignored
}

################################################################

# Specifies net has zero weight for placement.
define_cmd_args "set_ideal_net" { nets }

################################################################

define_cmd_args "set_ideal_network" {[-no_propagation] objects}

proc set_ideal_network { args } {
  # ignored
}

################################################################

define_cmd_args "set_ideal_transition" \
  {[-rise] [-fall] [-min] [-max] transition_time objects}

proc set_ideal_transition { args } {
  # ignored
}

################################################################

define_cmd_args "set_input_delay" \
  {[-rise] [-fall] [-max] [-min]\
     [-clock clock] [-clock_fall]\
     [-reference_pin ref_pin]\
     [-source_latency_included] [-network_latency_included]\
     [-add_delay] delay port_pin_list}

proc set_input_delay { args } {
  set_port_delay "set_input_delay" "set_input_delay_cmd" $args \
    {"input" "bidirect"}
}

proc set_port_delay { cmd sta_cmd cmd_args port_dirs } {
  parse_key_args $cmd cmd_args \
    keys {-clock -reference_pin} \
    flags {-rise -fall -max -min -clock_fall -add_delay \
             -source_latency_included -network_latency_included}
  check_argc_eq2 $cmd $cmd_args
  
  set delay_arg [lindex $cmd_args 0]
  check_float "delay" $delay_arg
  set delay [time_ui_sta $delay_arg]
  set pins [get_port_pins_error "pins" [lindex $cmd_args 1]]
  
  set clk "NULL"
  if [info exists keys(-clock)] {
    set clk [get_clock_warn "clock" $keys(-clock)]
  }
  
  set ref_pin "NULL"
  if [info exists keys(-reference_pin)] {
    set ref_pin [get_port_pin_error "ref_pin" $keys(-reference_pin)]
    if { [info exists flags(-source_latency_included)] } {
      sta_warn 438 "-source_latency_included ignored with -reference_pin."
    }
    if { [info exists flags(-network_latency_included)] } {
      sta_warn 439 "-network_latency_included ignored with -reference_pin."
    }
  }
  
  if [info exists flags(-clock_fall)] {
    set clk_rf "fall"
  } else {
    set clk_rf "rise"
  }
  
  set rf [parse_rise_fall_flags flags]
  set min_max [parse_min_max_all_flags flags]
  set add [info exists flags(-add_delay)]
  set source_latency_included [info exists flags(-source_latency_included)]
  set network_latency_included [info exists flags(-network_latency_included)]
  
  foreach pin $pins {
    if { [$pin is_top_level_port] \
           && [lsearch $port_dirs [pin_direction $pin]] == -1 } {
      sta_warn 440 "$cmd not allowed on [pin_direction $pin] port '[get_full_name $pin]'."
    } elseif { $clk != "NULL" && [lsearch [$clk sources] $pin] != -1 } {
      sta_warn 441 "$cmd relative to a clock defined on the same port/pin not allowed."
    } else {
      $sta_cmd $pin $rf $clk $clk_rf $ref_pin\
        $source_latency_included $network_latency_included \
        $min_max $add $delay
    }
  }
}

################################################################

define_cmd_args "unset_input_delay" \
  {[-rise] [-fall] [-max] [-min]\
     [-clock clock] [-clock_fall]\
     port_pin_list}

proc unset_input_delay { args } {
  unset_port_delay "unset_input_delay" "unset_input_delay_cmd" $args
}

################################################################

define_cmd_args "set_max_delay" \
  {[-rise] [-fall] [-ignore_clock_latency] [-reset_path] [-comment comment]\
     [-from from_list] [-rise_from from_list] [-fall_from from_list]\
     [-through through_list] [-rise_through through_list]\
     [-fall_through through_list]\
     [-to to_list] [-rise_to to_list] [-fall_to to_list] delay}

proc set_max_delay { args } {
  set_path_delay "set_max_delay" $args max
}

proc set_path_delay { cmd args min_max } {
  parse_key_args $cmd args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to -comment} \
    flags {-rise -fall -ignore_clock_latency -reset_path} 0
  
  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg args arg_error]
  set to [parse_to_arg keys flags arg_error]
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
  } else {
    check_for_key_args $cmd args
    if { [llength $args] == 0 } {
      delete_from_thrus_to $from $thrus $to
      sta_error 442 "missing delay argument."
    } elseif { [llength $args] == 1 } {
      set delay $args
      check_float "$cmd delay" $delay
      set delay [time_ui_sta $delay]
    } else {
      sta_warn 443 "'$args' ignored."
    }
    
    set ignore_clk_latency [info exists flags(-ignore_clock_latency)]
    
    if [info exists flags(-reset_path)] {
      reset_path_cmd $from $thrus $to "all"
    }
    
    set comment [parse_comment_key keys]
    
    make_path_delay $from $thrus $to $min_max $ignore_clk_latency \
      $delay $comment
  }
}

################################################################

define_cmd_args "set_max_time_borrow" {limit objects}

proc set_max_time_borrow { limit objects } {
  check_positive_float "borrow_limit" $limit
  set limit [time_ui_sta $limit]
  parse_clk_inst_pin_arg $objects clks insts pins
  foreach pin $pins {
    set_latch_borrow_limit_pin $pin $limit
  }
  foreach inst $insts {
    set_latch_borrow_limit_inst $inst $limit
  }
  foreach clk $clks {
    set_latch_borrow_limit_clk $clk $limit
  }
}

################################################################

define_cmd_args "set_min_delay" \
  {[-rise] [-fall] [-ignore_clock_latency] [-reset_path] [-comment comment]\
     [-from from_list] [-rise_from from_list] [-fall_from from_list]\
     [-through through_list] [-rise_through through_list]\
     [-fall_through through_list]\
     [-to to_list] [-rise_to to_list] [-fall_to to_list] delay}

proc set_min_delay { args } {
  set_path_delay "set_min_delay" $args min
}

################################################################

define_cmd_args "set_min_pulse_width" {[-low] [-high] value [objects]}

proc set_min_pulse_width { args } {
  parse_key_args "set_min_pulse_width" args keys {} flags {-low -high}
  check_argc_eq1or2 "set_min_pulse_width" $args
  
  if { [info exists flags(-low)] } {
    set hi_low "fall"
  } elseif { [info exists flags(-high)] } {
    set hi_low "rise"
  } else {
    set hi_low "rise_fall"
  }
  
  set min_width [lindex $args 0]
  check_positive_float "min pulse width" $min_width
  set min_width [time_ui_sta $min_width]
  
  if { [llength $args] == 2 } {
    set objects [lindex $args 1]
    parse_clk_inst_pin_arg $objects clks insts pins
    foreach pin $pins {
      set_min_pulse_width_pin $pin $hi_low $min_width
    }
    foreach inst $insts {
      set_min_pulse_width_inst $inst $hi_low $min_width
    }
    foreach clk $clks {
      set_min_pulse_width_clk $clk $hi_low $min_width
    }
  } else {
    set_min_pulse_width_global $hi_low $min_width
  }
}

################################################################

define_cmd_args "set_multicycle_path" \
  {[-setup] [-hold] [-rise] [-fall] [-start] [-end]\
     [-reset_path] [-comment comment]\
     [-from from_list] [-rise_from from_list]\
     [-fall_from from_list] [-through through_list]\
     [-rise_through through_list] [-fall_through through_list]\
     [-to to_list] [-rise_to to_list] [-fall_to to_list] path_multiplier}

proc set_multicycle_path { args } {
  parse_key_args "set_multicycle_path" args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to -comment} \
    flags {-setup -hold -rise -fall -start -end -reset_path} 0
  
  set min_max "min_max"
  set use_end_clk 1
  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    # -setup
    set min_max "max"
    set use_end_clk 1
  }
  if { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    # -hold
    set min_max "min"
    set use_end_clk 0
  }
  
  set cmd "set_multicycle_path"
  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg args arg_error]
  set to [parse_to_arg keys flags arg_error]
  check_exception_pins $from $to
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
  } else {
    check_for_key_args $cmd args
    if { [llength $args] == 0 } {
      delete_from_thrus_to $from $thrus $to
      sta_error 444 "missing path multiplier argument."
    } elseif { [llength $args] == 1 } {
      set path_multiplier $args
      check_integer "path multiplier" $path_multiplier
    } else {
      sta_warn 445 "'$args' ignored."
    }
    
    set start [info exists flags(-start)]
    set end [info exists flags(-end)]
    if { $start && $end } {
      delete_from_thrus_to $from $thrus $to
      sta_error 446 "cannot use -start with -end."
    } elseif { $start } {
      set use_end_clk 0
    } elseif { $end } {
      set use_end_clk 1
    }
    
    if [info exists flags(-reset_path)] {
      reset_path_cmd $from $thrus $to $min_max
    }
    
    set comment [parse_comment_key keys]
    
    make_multicycle_path $from $thrus $to $min_max $use_end_clk \
      $path_multiplier $comment
  }
}

################################################################

define_cmd_args "unset_path_exceptions" \
  {[-setup] [-hold] [-rise] [-fall] [-from from_list]\
     [-rise_from from_list] [-fall_from from_list]\
     [-through through_list] [-rise_through through_list]\
     [-fall_through through_list] [-to to_list] [-rise_to to_list]\
     [-fall_to to_list]}

proc unset_path_exceptions { args } {
  unset_path_exceptions_cmd "unset_path_exceptions" $args
}

proc unset_path_exceptions_cmd { cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-from -rise_from -fall_from -to -rise_to -fall_to} \
    flags {-setup -hold -rise -fall} 0
  
  set min_max "min_max"
  if { [info exists flags(-setup)] && ![info exists flags(-hold)] } {
    set min_max "max"
  }
  if { [info exists flags(-hold)] && ![info exists flags(-setup)] } {
    set min_max "min"
  }
  
  set arg_error 0
  set from [parse_from_arg keys arg_error]
  set thrus [parse_thrus_arg cmd_args arg_error]
  set to [parse_to_arg keys flags arg_error]
  if { $arg_error } {
    delete_from_thrus_to $from $thrus $to
    sta_error 447 "$cmd command failed."
    return 0
  }
  
  check_for_key_args $cmd cmd_args
  if { $cmd_args != {} } {
    delete_from_thrus_to $from $thrus $to
    sta_error 448 "positional arguments not supported."
  }
  if { ($from == "NULL" && $thrus == "" && $to == "NULL") } {
    delete_from_thrus_to $from $thrus $to
    sta_error 449 "-from, -through or -to required."
  }
  
  reset_path_cmd $from $thrus $to $min_max
  delete_from_thrus_to $from $thrus $to
}

################################################################

define_cmd_args "set_output_delay" \
  {[-rise] [-fall] [-max] [-min]\
     [-clock clock] [-clock_fall]\
     [-reference_pin ref_pin]\
     [-source_latency_included] [-network_latency_included]\
     [-add_delay] delay port_pin_list}

proc set_output_delay { args } {
  set_port_delay "set_output_delay" "set_output_delay_cmd" $args \
    {"output" "tristate" "bidirect"}
}

################################################################

define_cmd_args "unset_output_delay" \
  {[-rise] [-fall] [-max] [-min]\
     [-clock clock] [-clock_fall]\
     port_pin_list}

proc unset_output_delay { args } {
  unset_port_delay "unset_output_delay" "unset_output_delay_cmd" $args
}

proc unset_port_delay { cmd swig_cmd cmd_args } {
  parse_key_args $cmd cmd_args \
    keys {-clock -reference_pin} \
    flags {-rise -fall -max -min -clock_fall }
  check_argc_eq1 $cmd $cmd_args
  
  set pins [get_port_pins_error "pins" [lindex $cmd_args 0]]
  
  set clk "NULL"
  if [info exists keys(-clock)] {
    set clk [get_clock_warn "clock" $keys(-clock)]
  }
  
  if [info exists flags(-clock_fall)] {
    set clk_rf "fall"
  } else {
    set clk_rf "rise"
  }
  
  set rf [parse_rise_fall_flags flags]
  set min_max [parse_min_max_all_flags flags]
  
  foreach pin $pins {
    $swig_cmd $pin $rf $clk $clk_rf $min_max
  }
}

################################################################

define_cmd_args "set_propagated_clock" {objects}

proc set_propagated_clock { objects } {
  parse_clk_port_pin_arg $objects clks pins
  foreach clk $clks {
    if { [$clk is_virtual] } {
      sta_warn 450 "virtual clock [get_name $clk] can not be propagated."
    } else {
      set_propagated_clock_cmd $clk
    }
  }
  foreach pin $pins {
    set_propagated_clock_pin_cmd $pin
  }
}

################################################################

define_cmd_args "unset_propagated_clock" {objects}

proc unset_propagated_clock { objects } {
  parse_clk_port_pin_arg $objects clks pins
  foreach clk $clks {
    unset_propagated_clock_cmd $clk
  }
  foreach pin $pins {
    unset_propagated_clock_pin_cmd $pin
  }
}

################################################################
#
# Environment Commands
#
################################################################

define_cmd_args "set_case_analysis" \
  {0|1|zero|one|rise|rising|fall|falling pins}

proc set_case_analysis { value pins } {
  if { !($value == "0" \
           || $value == "1" \
           || $value == "zero" \
           || $value == "one" \
           || $value == "rise" \
           || $value == "rising" \
           || $value == "fall" \
           || $value == "falling") } {
    sta_error 451 "value must be 0, zero, 1, one, rise, rising, fall, or falling."
  }
  set pins1 [get_port_pins_error "pins" $pins]
  foreach pin $pins1 {
    set_case_analysis_cmd $pin $value
  }
}

################################################################

define_cmd_args "unset_case_analysis" {pins}

proc unset_case_analysis { pins } {
  set pins1 [get_port_pins_error "pins" $pins]
  foreach pin $pins1 {
    unset_case_analysis_cmd $pin
  }
}

################################################################

define_cmd_args "set_drive" {[-rise] [-fall] [-min] [-max] \
                               resistance ports}

proc set_drive { args } {
  parse_key_args "set_drive" args keys {} flags {-rise -fall -min -max}
  set rf [parse_rise_fall_flags flags]
  set min_max [parse_min_max_all_check_flags flags]
  
  check_argc_eq2 "set_drive" $args
  
  set res [lindex $args 0]
  check_positive_float "resistance" $res
  set res [resistance_ui_sta $res]
  set ports [get_ports_error "ports" [lindex $args 1]]
  foreach port $ports {
    set_drive_resistance_cmd $port $rf $min_max $res
  }
}

################################################################

define_cmd_args "set_driving_cell" \
  {[-lib_cell cell] [-library library]\
     [-rise] [-fall] [-min] [-max]\
     [-pin pin] [-from_pin from_pin]\
     [-input_transition_rise trans_rise] [-input_transition_fall trans_fall]\
     [-multiply_by factor] [-dont_scale] [-no_design_rule] ports}

proc set_driving_cell { args } {
  parse_key_args "set_driving_cell" args \
    keys {-lib_cell -cell -library -pin -from_pin -multiply_by \
            -input_transition_rise -input_transition_fall} \
    flags {-rise -fall -min -max -dont_scale -no_design_rule}
  
  set rf [parse_rise_fall_flags flags]
  set min_max [parse_min_max_all_flags flags]
  
  # -cell is an undocumented non-sdc alias for -lib_cell.
  if { [info exists keys(-cell)] } {
    set keys(-lib_cell) $keys(-cell)
  }
  
  if { [info exists keys(-lib_cell)] } {
    set cell_name $keys(-lib_cell)
    if { [info exists keys(-library)] } {
      set library [get_liberty_error "library" $keys(-library)]
      set cell [$library find_liberty_cell $cell_name]
      if { $cell == "NULL" } {
        sta_error 452 "cell '$lib_name:$cell_name' not found."
      }
    } else {
      set library "NULL"
      if { [is_object $cell_name] } {
	if { [object_type $cell_name] != "LibertyCell" } {
	  sta_error 334 "object '$cell_name' is not a liberty cell."
	}
	set cell $cell_name
      } else {
        set cell [find_liberty_cell $cell_name]
      }
      if { $cell == "NULL" } {
        sta_error 453 "'$cell_name' not found."
      }
    }
  } else {
    sta_error 454 "missing -lib_cell argument."
  }
  
  set to_port "NULL"
  if [info exists keys(-pin)] {
    set to_port_name $keys(-pin)
    set to_port [$cell find_liberty_port $to_port_name]
    if { $to_port == "NULL" } {
      sta_error 455 "port '$to_port_name' not found."
    }
  } else {
    set port_iter [$cell liberty_port_iterator]
    set output_count 0
    while {[$port_iter has_next]} {
      set port [$port_iter next]
      set dir [liberty_port_direction $port]
      if { [port_direction_any_output $dir] } {
        incr output_count
        if { $output_count > 1 } {
          $port_iter finish
          sta_error 456 "-pin argument required for cells with multiple outputs."
        }
        set to_port $port
        # No break.  Keep looking for output ports to make sure there
        # is only one.
      }
    }
    $port_iter finish
  }
  
  set from_port "NULL"
  if [info exists keys(-from_pin)] {
    set from_port_name $keys(-from_pin)
    set from_port [$cell find_liberty_port $from_port_name]
    if { $from_port == "NULL" } {
      sta_error 457 "port '$from_port_name' not found."
    }
  }
  
  set from_slew_rise 0
  if [info exists keys(-input_transition_rise)] {
    set from_slew_rise $keys(-input_transition_rise)
    check_positive_float "-input_transition_rise" $from_slew_rise
    set from_slew_rise [time_ui_sta $from_slew_rise]
  }
  set from_slew_fall 0
  if [info exists keys(-input_transition_fall)] {
    set from_slew_fall $keys(-input_transition_fall)
    check_positive_float "-input_transition_fall" $from_slew_fall
    set from_slew_fall [time_ui_sta $from_slew_fall]
  }
  
  if [info exists keys(-multiply_by)] {
    sta_warn 458 "-multiply_by ignored."
  }
  if [info exists flags(-dont_scale)] {
    sta_warn 459 "-dont_scale ignored."
  }
  if [info exists flags(-no_design_rule)] {
    sta_warn 460 "-no_design_rule ignored."
  }
  
  check_argc_eq1 "set_driving_cell" $args
  
  set ports [get_ports_error "ports" [lindex $args 0]]
  foreach port $ports {
    set_drive_cell_cmd $library $cell $port $from_port \
      $from_slew_rise $from_slew_fall $to_port $rf $min_max
  }
}

proc port_direction_any_output { dir } {
  return [expr { $dir == "output" \
                   || $dir == "bidirect" \
                   || $dir == "tristate" } ]
}

################################################################

define_cmd_args "set_fanout_load" {fanout ports}

proc set_fanout_load { fanout port_list } {
  sta_warn 461 "set_fanout_load not supported."
}

################################################################

define_cmd_args "set_input_transition" \
  {[-rise] [-fall] [-min] [-max] transition ports}

proc set_input_transition { args } {
  parse_key_args "set_input_transition" args keys {-clock} \
    flags {-rise -fall -max -min -clock_fall}
  
  set rf [parse_rise_fall_flags flags]
  set min_max [parse_min_max_all_flags flags]
  
  
  check_argc_eq2 "set_input_transition" $args
  
  set slew [lindex $args 0]
  check_positive_float "transition" $slew
  set slew [time_ui_sta $slew]
  set ports [get_ports_error "ports" [lindex $args 1]]
  
  if [info exists keys(-clock)] {
    sta_warn 462 "-clock not supported."
  }
  if [info exists flags(-clock_fall)] {
    sta_warn 463 "-clock_fall not supported."
  }
  
  foreach port $ports {
    set_input_slew_cmd $port $rf $min_max $slew
  }
}

################################################################

# set_load -wire_load port  external wire load
# set_load -pin_load port   external pin load
# set_load port             same as -pin_load
# set_load net              overrides parasitics
define_cmd_args "set_load" \
  {[-corner corner] [-rise] [-fall] [-max] [-min] [-subtract_pin_load]\
     [-pin_load] [-wire_load] capacitance objects}

proc set_load { args } {
  parse_key_args "set_load" args keys {-corner} \
    flags {-rise -fall -min -max -subtract_pin_load -pin_load -wire_load}\
    
  check_argc_eq2 "set_load" $args
  
  set pin_load [info exists flags(-pin_load)]
  set wire_load [info exists flags(-wire_load)]
  set subtract_pin_load [info exists flags(-subtract_pin_load)]
  set corner [parse_corner_or_all keys]
  set min_max [parse_min_max_all_check_flags flags]
  set rf [parse_rise_fall_flags flags]
  
  set cap [lindex $args 0]
  check_positive_float "capacitance" $cap
  set cap [capacitance_ui_sta $cap]
  parse_port_net_args [lindex $args 1] ports nets
  
  if { $ports != {} } {
    # -pin_load is the default.
    if { $pin_load || (!$pin_load && !$wire_load) } {
      foreach port $ports {
        set_port_ext_pin_cap $port $rf $corner $min_max $cap
      }
    } elseif { $wire_load } {
      foreach port $ports {
        set_port_ext_wire_cap $port $subtract_pin_load $rf $corner $min_max $cap
      }
    }
  }
  if { $nets != {} } {
    if { $pin_load } {
      sta_warn 464 "-pin_load not allowed for net objects."
    }
    if { $wire_load } {
      sta_warn 465 "-wire_load not allowed for net objects."
    }
    if { $rf != "rise_fall" } {
      sta_warn 466 "-rise/-fall not allowed for net objects."
    }
    foreach net $nets {
      set_net_wire_cap $net $subtract_pin_load $corner $min_max $cap
    }
  }
}

################################################################

define_cmd_args "set_logic_dc" {port_list}

proc set_logic_dc { port_list } {
  set_logic_value $port_list "X"
}

# OT supports set_logic cmds on pins.
# OC only supports them on ports.
proc set_logic_value { port_list value } {
  set pins [get_port_pins_error "pins" $port_list]
  foreach pin $pins {
    set_logic_value_cmd $pin $value
  }
}

################################################################

define_cmd_args "set_logic_one" {port_list}

proc set_logic_one { port_list } {
  set_logic_value $port_list "1"
}

################################################################

define_cmd_args "set_logic_zero" {port_list}

proc set_logic_zero { port_list } {
  set_logic_value $port_list "0"
}

################################################################

define_cmd_args "set_max_area" {area}

proc set_max_area { area } {
  check_positive_float "area" $area
  set_max_area_cmd $area
}

################################################################

define_cmd_args "set_max_capacitance" {cap objects}

proc set_max_capacitance { cap objects } {
  set_capacitance_limit $cap "max" $objects
}

proc set_capacitance_limit { cap min_max objects } {
  parse_cell_port_pin_args $objects cells ports pins
  check_positive_float "limit" $cap
  set cap [capacitance_ui_sta $cap]
  foreach cell $cells {
    set_cell_capacitance_limit $cell $min_max $cap
  }
  foreach port $ports {
    set_port_capacitance_limit $port $min_max $cap
  }
  foreach pin $pins {
    set_pin_capacitance_limit $pin $min_max $cap
  }
}

################################################################

define_cmd_args "set_max_fanout" {fanout objects}

proc set_max_fanout { fanout objects } {
  set_fanout_limit $fanout "max" $objects
}

proc set_fanout_limit { fanout min_max objects } {
  check_positive_float "limit" $fanout
  parse_cell_port_args $objects cells ports
  foreach port $ports {
    set dir [port_direction $port]
    if { !($dir == "input" || $dir == "bidirect") } {
      sta_error 467 "port '[get_name $port]' is not an input."
    }
    set_port_fanout_limit $port $min_max $fanout
  }
  foreach cell $cells {
    set_cell_fanout_limit $cell $min_max $fanout
  }
}

################################################################

define_cmd_args "set_max_transition" \
  {[-clock_path] [-data_path] [-rise] [-fall] slew objects}

proc set_max_transition { args } {
  parse_key_args "set_max_transition" args keys {} \
    flags {-clock_path -data_path -rise -fall}
  check_argc_eq2 "set_max_transition" $args
  
  set slew [lindex $args 0]
  check_positive_float "transition" $slew
  set slew [time_ui_sta $slew]
  
  set objects [lindex $args 1]
  parse_clk_cell_port_args $objects clks cells ports
  
  set rf [parse_rise_fall_flags flags]
  
  set path_types {}
  if { ![info exists flags(-clock_path)] \
         && ![info exists flags(-data_path)] } {
    # Derate clk and data if neither -clock_path or -data_path.
    set path_types {"clk" "data"}
  }
  if { [info exists flags(-clock_path)] } {
    lappend path_types "clk"
  }
  if { [info exists flags(-data_path)] } {
    lappend path_types "data"
  }
  
  if { ($ports != {} || $cells != {}) \
         && ([info exists flags(-clock_path)] \
               || [info exists flags(-data_path)]
             || [info exists flags(-rise)]
             || [info exists flags(-fall)]) } {
    sta_warn 468 "-data_path, -clock_path, -rise, -fall ignored for ports and designs."
  }
  
  # -clock_path/-data_path and transition only apply to clock objects.
  foreach path_type $path_types {
    foreach clk $clks {
      set_slew_limit_clk $clk $rf $path_type "max" $slew
    }
  }
  foreach cell $cells {
    set_slew_limit_cell $cell "max" $slew
  }
  foreach port $ports {
    set_slew_limit_port $port "max" $slew
  }
}

################################################################

define_cmd_args "set_port_fanout_number" \
  {[-corner corner] [-max] [-min] fanout ports}

proc set_port_fanout_number { args } {
  parse_key_args "set_port_fanout_number" args keys {-corner} flags {-max -min}
  set min_max [parse_min_max_all_check_flags flags]
  
  check_argc_eq2 "set_port_fanout_number" $args
  
  set fanout [lindex $args 0]
  check_positive_integer "fanout" $fanout
  set ports [get_ports_error "ports" [lindex $args 1]]
  set corner [parse_corner_or_all keys]
  foreach port $ports {
    set_port_ext_fanout_cmd $port $fanout $corner $min_max
  }
}

################################################################

define_cmd_args "set_resistance" {[-min] [-max] resistance nets}

proc set_resistance { args } {
  parse_key_args "set_resistance" args keys {} flags {-max -min}
  set min_max [parse_min_max_all_check_flags flags]
  
  check_argc_eq2 "set_resistance" $args
  
  set res [lindex $args 0]
  check_positive_float "resistance" $res
  set res [resistance_ui_sta $res]
  set nets [get_nets_arg "nets" [lindex $args 1]]
  foreach net $nets {
    set_net_resistance $net $min_max $res
  }
}

################################################################

define_cmd_args "set_timing_derate" \
  {-early|-late [-rise] [-fall] [-clock] [-data] \
     [-net_delay] [-cell_delay] [-cell_check] derate [objects]}

proc set_timing_derate { args } {
  parse_key_args "set_timing_derate" args keys {} \
    flags {-rise -fall -early -late -clock -data \
             -net_delay -cell_delay -cell_check}
  check_argc_eq1or2 "set_timing_derate" $args
  
  set derate [lindex $args 0]
  check_float "derate" $derate
  if { $derate > 2.0 } {
    sta_warn 469 "derating factor greater than 2.0."
  }
  
  set rf [parse_rise_fall_flags flags]
  set early_late [parse_early_late_flags flags]
  
  set path_types {}
  if { ![info exists flags(-clock)] \
         && ![info exists flags(-data)] } {
    # Derate clk and data if neither -clock or -data.
    lappend path_types "clk"
    lappend path_types "data"
  }
  if { [info exists flags(-clock)] } {
    lappend path_types "clk"
  }
  if { [info exists flags(-data)] } {
    lappend path_types "data"
  }
  
  set derate_types {}
  if { [info exists flags(-net_delay)] } {
    lappend derate_types "net_delay"
  }
  if { [info exists flags(-cell_delay)] } {
    lappend derate_types "cell_delay"
  }
  if { [info exists flags(-cell_check)] } {
    lappend derate_types "cell_check"
  }
  
  if { [llength $args] == 2 } {
    set objects [lindex $args 1]
    parse_libcell_inst_net_arg $objects libcells insts nets
    if { $nets != {} } {
      if { [info exists flags(-cell_delay)] \
             || [info exists flags(-cell_check)] } {
        sta_warn 470 "-cell_delay and -cell_check flags ignored for net objects."
      }
      foreach net $nets {
        foreach path_type $path_types {
          set_timing_derate_net_cmd $net $path_type $rf $early_late $derate
        }
      }
    }
    if { ![info exists flags(-cell_delay)] \
           && ![info exists flags(-cell_check)] } {
      # Cell checks are not derated if no flags are specified.
      set derate_types {cell_delay}
    }
    foreach derate_type $derate_types {
      foreach path_type $path_types {
        foreach inst $insts {
          set_timing_derate_inst_cmd $inst $derate_type $path_type \
            $rf $early_late $derate
        }
        foreach libcell $libcells {
          set_timing_derate_cell_cmd $libcell $derate_type $path_type \
            $rf $early_late $derate
        }
      }
    }
  } else {
    if { ![info exists flags(-net_delay)] \
           && ![info exists flags(-cell_delay)] \
           && ![info exists flags(-cell_check)] } {
      # Cell checks are not derated if no flags are specified.
      set derate_types {net_delay cell_delay}
    }
    foreach derate_type $derate_types {
      foreach path_type $path_types {
        set_timing_derate_cmd $derate_type $path_type $rf $early_late $derate
      }
    }
  }
}

################################################################

define_cmd_args "unset_timing_derate" {}

proc unset_timing_derate { args } {
  check_argc_eq0 "unset_timing_derate" $args
  unset_timing_derate_cmd
}

################################################################

proc parse_from_arg { keys_var arg_error_var } {
  upvar 1 $keys_var keys
  
  if [info exists keys(-from)] {
    set key "-from"
    set rf "rise_fall"
  } elseif [info exists keys(-rise_from)] {
    set key "-rise_from"
    set rf "rise"
  } elseif [info exists keys(-fall_from)] {
    set key "-fall_from"
    set rf "fall"
  } else {
    return "NULL"
  }
  parse_clk_inst_port_pin_arg $keys($key) from_clks from_insts from_pins
  if {$from_pins == {} && $from_insts == {} && $from_clks == {}} {
    upvar 1 $arg_error_var arg_error
    set arg_error 1
    sta_warn 471 "no valid objects specified for $key."
    return "NULL"
  }
  return [make_exception_from $from_pins $from_clks $from_insts $rf]
}

# "arg_error" is set to notify the caller to cleanup and post error.
proc parse_thrus_arg { args_var arg_error_var } {
  upvar 1 $args_var args
  
  set thrus {}
  set args_rtn {}
  while { $args != {} } {
    set arg [lindex $args 0]
    set rf ""
    if { $arg == "-through" } {
      set rf "rise_fall"
      set key "-through"
    } elseif { $arg == "-rise_through" } {
      set rf "rise"
      set key "-rise_through"
    } elseif { $arg == "-fall_through" } {
      set rf "fall"
      set key "-fall_through"
    }
    if { $rf != "" } {
      if { [llength $args] > 1 } {
        set args [lrange $args 1 end]
        set arg [lindex $args 0]
        parse_inst_port_pin_net_arg $arg insts pins nets
        if {$pins == {} && $insts == {} && $nets == {}} {
          upvar 1 $arg_error_var arg_error
          set arg_error 1
          sta_warn 472 "no valid objects specified for $key"
        } else {
          lappend thrus [make_exception_thru $pins $nets $insts $rf]
        }
      }
    } else {
      lappend args_rtn $arg
    }
    set args [lrange $args 1 end]
  }
  set args $args_rtn
  return $thrus
}

# Parse -to|-rise_to|-fall_to keywords.
proc parse_to_arg { keys_var flags_var arg_error_var } {
  upvar 1 $keys_var keys
  upvar 1 $flags_var flags
  upvar 1 $arg_error_var arg_error
  
  set end_rf [parse_rise_fall_flags flags]
  return [parse_to_arg1 keys $end_rf arg_error]
}

proc parse_to_arg1 { keys_var end_rf arg_error_var } {
  upvar 1 $keys_var keys
  upvar 1 $arg_error_var arg_error
  
  if [info exists keys(-to)] {
    set key "-to"
    set to_rf "rise_fall"
  } elseif [info exists keys(-rise_to)] {
    set key "-rise_to"
    set to_rf "rise"
  } elseif [info exists keys(-fall_to)] {
    set key "-fall_to"
    set to_rf "fall"
  } else {
    # -rise/-fall without -to/-rise_to/-fall_to (no objects).
    if { $end_rf != "rise_fall" } {
      return [make_exception_to {} {} {} "rise_fall" $end_rf]
    } else {
      return "NULL"
    }
  }
  parse_clk_inst_port_pin_arg $keys($key) to_clks to_insts to_pins
  if {$to_pins == {} && $to_insts == {} && $to_clks == {}} {
    upvar 1 $arg_error_var arg_error
    set arg_error 1
    sta_warn 473 "no valid objects specified for $key."
    return "NULL"
  }
  return [make_exception_to $to_pins $to_clks $to_insts $to_rf $end_rf]
}

proc delete_from_thrus_to { from thrus to } {
  if { $from != "NULL" } {
    delete_exception_from $from
  }
  if { $thrus != {} } {
    foreach thru $thrus {
      delete_exception_thru $thru
    }
  }
  if { $to != "NULL" } {
    delete_exception_to $to
  }
}

proc parse_comment_key { keys_var } {
  upvar 1 $keys_var keys
  
  set comment ""
  if { [info exists keys(-comment)] } {
    set comment $keys(-comment)
  }
  return $comment
}

################################################################

define_cmd_args "set_min_capacitance" {cap objects}

proc set_min_capacitance { cap objects } {
  set_capacitance_limit $cap "min" $objects
}

################################################################

define_cmd_args "set_operating_conditions" \
  {[-analysis_type single|bc_wc|on_chip_variation] [-library lib]\
     [condition] [-min min_condition] [-max max_condition]\
     [-min_library min_lib] [-max_library max_lib]}

proc set_operating_conditions { args } {
  parse_key_args "set_operating_conditions" args \
    keys {-analysis_type -library -min -max -min_library -max_library} flags {}
  parse_op_cond_analysis_type keys
  check_argc_eq0or1 set_operating_conditions $args
  if { [llength $args] == 1 } {
    set op_cond_name $args
    parse_op_cond $op_cond_name "-library" "all" keys
  }
  if [info exists keys(-min)] {
    parse_op_cond $keys(-min) "-min_library" "min" keys
  }
  if [info exists keys(-max)] {
    parse_op_cond $keys(-max) "-max_library" "max" keys
  }
}

proc parse_op_cond { op_cond_name lib_key min_max key_var } {
  upvar 1 $key_var keys
  if [info exists keys($lib_key)] {
    set liberty [get_liberty_error $lib_key $keys($lib_key)]
    set op_cond [$liberty find_operating_conditions $op_cond_name]
    if { $op_cond == "NULL" } {
      sta_error 474 "operating condition '$op_cond_name' not found."
    } else {
      set_operating_conditions_cmd $op_cond $min_max
    }
  } else {
    set found 0
    set lib_iter [liberty_library_iterator]
    while {[$lib_iter has_next]} {
      set lib [$lib_iter next]
      set op_cond [$lib find_operating_conditions $op_cond_name]
      if { $op_cond != "NULL" } {
        set_operating_conditions_cmd $op_cond $min_max
        set found 1
        break
      }
    }
    $lib_iter finish
    if { !$found } {
      sta_error 475 "operating condition '$op_cond_name' not found."
    }
  }
}

proc parse_op_cond_analysis_type { key_var } {
  upvar 1 $key_var keys
  if [info exists keys(-analysis_type)] {
    set analysis_type $keys(-analysis_type)
    if { $analysis_type == "single" \
           || $analysis_type == "bc_wc" \
           || $analysis_type == "on_chip_variation" } {
      set_analysis_type_cmd $analysis_type
    } else {
      sta_error 476 "-analysis_type must be single, bc_wc or on_chip_variation."
    }
  } elseif { [info exists keys(-min)] && [info exists keys(-max)] } {
    set_analysis_type_cmd "bc_wc"
  }
}

################################################################

define_cmd_args "set_wire_load_min_block_size" {block_size}

proc set_wire_load_min_block_size { block_size } {
  sta_warn 477 "set_wire_load_min_block_size not supported."
}

################################################################

define_cmd_args "set_wire_load_mode" "top|enclosed|segmented"

proc set_wire_load_mode { mode } {
  if { $mode == "top" \
         || $mode == "enclosed" \
         || $mode == "segmented" } {
    set_wire_load_mode_cmd $mode
  } else {
    sta_error 478 "mode must be top, enclosed or segmented."
  }
}

################################################################

define_cmd_args "set_wire_load_model" \
  {-name model_name [-library lib_name] [-min] [-max] [objects]}

proc set_wire_load_model { args } {
  parse_key_args "set_wire_load_model" args keys {-name -library} \
    flags {-min -max}
  
  check_argc_eq0or1 "set_wire_load_model" $args
  if { ![info exists keys(-name)] } {
    sta_error 479 "no wire load model specified."
  }
  
  set model_name $keys(-name)
  set min_max [parse_min_max_all_check_flags flags]
  
  set wireload "NULL"
  if [info exists keys(-library)] {
    set library [get_liberty_error "library" $keys(-library)]
    set wireload [$library find_wireload $model_name]
  } else {
    set lib_iter [liberty_library_iterator]
    while {[$lib_iter has_next]} {
      set lib [$lib_iter next]
      set wireload [$lib find_wireload $model_name]
      if {$wireload != "NULL"} {
        break;
      }
    }
    $lib_iter finish
  }
  if {$wireload == "NULL"} {
    sta_error 480 "wire load model '$model_name' not found."
  }
  set objects $args
  set_wire_load_cmd $wireload $min_max
}

################################################################

define_cmd_args "set_wire_load_selection_group" \
  {[-library lib] [-min] [-max] group_name [objects]}

proc set_wire_load_selection_group { args } {
  parse_key_args "set_wire_load_selection_group" args keys {-library} \
    flags {-min -max}
  
  set argc [llength $args]
  check_argc_eq1or2 "wire_load_selection_group" $args
  
  set selection_name [lindex $args 0]
  set objects [lindex $args 1]
  
  set min_max [parse_min_max_all_check_flags flags]
  
  set selection "NULL"
  if [info exists keys(-library)] {
    set library [get_liberty_error "library" $keys(-library)]
    set selection [$library find_wireload_selection $selection_name]
  } else {
    set lib_iter [liberty_library_iterator]
    while {[$lib_iter has_next]} {
      set lib [$lib_iter next]
      set selection [$lib find_wireload_selection $selection_name]
      if {$selection != "NULL"} {
        break;
      }
    }
    $lib_iter finish
  }
  if {$selection == "NULL"} {
    sta_error 481 "wire load selection group '$selection_name' not found."
  }
  set_wire_load_selection_group_cmd $selection $min_max
}

################################################################
#
# Multivoltage and Power Optimization Commands
#
################################################################

define_cmd_args "set_voltage" \
  {[-min min_case_value] [-object_list list_of_power_nets] max_case_voltage}

proc set_voltage { args } {
  parse_key_args "set_voltage" args keys {-min -object_list} flags {}
  check_argc_eq1 "set_voltage" $args
  set max_case_voltage [lindex $args 0]
  check_float "max_case_voltage" $max_case_voltage
  
  set nets {}
  if { [info exists keys(-object_list)] } {
    set nets [get_nets_arg "-object_list" $keys(-object_list)]
  }
  set_voltage_global "max" $max_case_voltage
  foreach net $nets {
    set_voltage_net $net "max" $max_case_voltage
  }
  if { [info exists keys(-min)] } {
    set min_case_voltage $keys(-min)
    check_float "-min" $min_case_voltage
    set_voltage_global "min" $min_case_voltage
    foreach net $nets {
      set_voltage_net $net "min" $min_case_voltage
    }
  }
}

define_cmd_args "create_voltage_area" \
  {[-name name] [-coordinate coordinates] [-guard_band_x guard_x]\
     [-guard_band_y guard_y] cells }

proc create_voltage_area { args } {
  # ignored
}

################################################################

define_cmd_args "set_level_shifter_strategy" {[-rule rule_type]}

proc set_level_shifter_strategy { args } {
  # ignored
}

################################################################

define_cmd_args "set_level_shifter_threshold" {[-voltage volt]}

proc set_level_shifter_threshold { args } {
  # ignored
}

################################################################

define_cmd_args "set_max_dynamic_power" {power [unit]}

proc set_max_dynamic_power { power {unit {}} } {
  # ignored
}

################################################################

define_cmd_args "set_max_leakage_power" {power [unit]}

proc set_max_leakage_power { power {unit {}} } {
  # ignored
}

################################################################
#
# Non-SDC commands
#
################################################################

define_cmd_args "define_corners" { corner1 [corner2]... }

proc define_corners { args } {
  if { [get_libs -quiet *] != {} } {
    sta_error 482 "define_corners must be called before read_liberty."
  }
  if { [llength $args] == 0 } { 
    sta_error 577 "define_corners must define at least one corner."
  }
  define_corners_cmd $args
}

################################################################

define_cmd_args "set_pvt"\
  {insts [-min] [-max] [-process process] [-voltage voltage]\
     [-temperature temperature]}

proc set_pvt { args } {
  parse_key_args "set_pvt" args \
    keys {-process -voltage -temperature} flags {-min -max}
  
  set min_max [parse_min_max_all_flags flags]
  check_argc_eq1 "set_pvt" $args
  set insts [get_instances_error "instances" [lindex $args 0]]
  
  if { $min_max == "min_max" } {
    set_pvt_min_max $insts "min" keys
    set_pvt_min_max $insts "max" keys
  } else {
    set_pvt_min_max $insts $min_max keys
  }
}

proc set_pvt_min_max { insts min_max keys_var } {
  upvar 1 $keys_var keys
  set op_cond [operating_conditions $min_max]
  if { $op_cond == "NULL" } {
    set op_cond [default_operating_conditions]
  }
  if [info exists keys(-process)] {
    set process $keys(-process)
    check_float "-process" $process
  } else {
    set process [$op_cond process]
  }
  if [info exists keys(-voltage)] {
    set voltage $keys(-voltage)
    check_float "-voltage" $voltage
  } else {
    set voltage [$op_cond voltage]
  }
  if [info exists keys(-temperature)] {
    set temperature $keys(-temperature)
    check_float "-temperature" $temperature
  } else {
    set temperature [$op_cond temperature]
  }
  
  foreach inst $insts {
    set_instance_pvt $inst $min_max $process $voltage $temperature
  }
}

proc default_operating_conditions {} {
  set found 0
  set lib_iter [liberty_library_iterator]
  while {[$lib_iter has_next]} {
    set lib [$lib_iter next]
    set op_cond [$lib default_operating_conditions]
    if { $op_cond != "NULL" } {
      set found 1
      break
    }
  }
  $lib_iter finish
  if { !$found } {
    sta_error 500 "no default operating conditions found."
  }
  return $op_cond
}

################################################################

proc cell_regexp {} {
  global hierarchy_separator
  return [cell_regexp_hsc $hierarchy_separator]
}

proc cell_regexp_hsc { hsc } {
  return "^(\[^${hsc}\]+)${hsc}(\[^${hsc}\]+)$"
}

proc port_regexp {} {
  global hierarchy_separator
  return [port_regexp_hsc $hierarchy_separator]
}

proc port_regexp_hsc { hsc } {
  return "^(\[^${hsc}\]+)${hsc}(\[^${hsc}\]+)${hsc}(\[^${hsc}\]+)$"
}

# sta namespace end.
}
