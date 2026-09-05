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

################################################################
#  
# Utility commands
#
################################################################

define_cmd_args "help" {[-verbose] [pattern]} \
  -help {Print command usage. With a single match, print the description and options. Use `-verbose` to print full `help` for every match.} \
  -arg_help {
    -verbose {Print full descriptions even when multiple commands match.}
  }

proc_redirect help {
  variable cmd_args
  variable var_help

  parse_key_args "help" args keys {} flags {-verbose}
  set verbose [info exists flags(-verbose)]
  set arg_count [llength $args]
  if { $arg_count == 0 } {
    set pattern "*"
  } elseif { $arg_count == 1 } {
    set pattern [lindex $args 0]
  } else {
    cmd_usage_error "help"
  }

  set cmd_matches [lsort [array names cmd_args $pattern]]
  set var_matches {}
  if { $pattern != "*" } {
    set var_matches [lsort [array names var_help $pattern]]
  }
  set match_count [expr { [llength $cmd_matches] + [llength $var_matches] }]
  if { $match_count == 0 } {
    sta_warn 160 "no commands match '$pattern'."
    return
  }

  set full [expr { $verbose || $match_count == 1 }]
  foreach cmd $cmd_matches {
    if { $full } {
      show_cmd_help $cmd
    } else {
      show_cmd_args $cmd
    }
  }
  foreach var $var_matches {
    show_var_help $var $full
  }
}

proc show_cmd_args { cmd } {
  variable cmd_args

  set max_col 80
  set indent 2
  set indent_str "  "
  set line $cmd
  set col [string length $cmd]
  set arglist $cmd_args($cmd)
  # Break the arglist up into max_col length lines.
  while {1} {
    if {[regexp {(^[\n ]*)([a-zA-Z0-9_\\\|\-]+|\[[^\[]+\])(.*)} \
           $arglist ignore space arg rest]} {
      set arg_length [string length $arg]
      if { $col + $arg_length < $max_col } {
        set line "$line $arg"
        set col [expr $col + $arg_length + 1]
      } else {
        report_line $line
        set line "$indent_str $arg"
        set col [expr $indent + $arg_length + 1]
      }
      set arglist $rest
    } else {
      report_line "$line $arglist"
      break
    }
  }
}

proc show_cmd_help { cmd } {
  variable cmd_args

  show_cmd_args $cmd
  set desc [md_help_to_text [cmd_help_text $cmd]]
  if { $desc != "" } {
    report_wrapped $desc 80 2
  }
  foreach opt [cmd_synopsis_options $cmd_args($cmd)] {
    set opt_desc [md_help_to_text [cmd_arg_help_text $cmd $opt]]
    if { $opt_desc != "" } {
      report_opt_help $opt $opt_desc
    }
  }
}

proc show_var_help { var full } {
  set values [var_help_values $var]
  if { $values != "" } {
    report_line "$var $values"
  } else {
    report_line $var
  }
  if { $full } {
    set desc [md_help_to_text [var_help_text $var]]
    if { $desc != "" } {
      report_wrapped $desc 80 2
    }
  }
}

proc report_opt_help { opt desc } {
  set indent 2
  set hang 4
  set width 80
  set prefix "[string repeat " " $indent]$opt  "
  set col [string length $prefix]
  set line $prefix
  foreach word [split $desc] {
    if { $word == "" } {
      continue
    }
    set word_len [string length $word]
    if { $col + $word_len + 1 > $width && $col > $hang } {
      report_line [string trimright $line]
      set line "[string repeat " " $hang]$word "
      set col [expr { $hang + $word_len + 1 }]
    } else {
      append line "$word "
      set col [expr { $col + $word_len + 1 }]
    }
  }
  report_line [string trimright $line]
}

proc report_wrapped { text width indent } {
  set prefix [string repeat " " $indent]
  foreach para [split $text "\n"] {
    if { [string trim $para] == "" } {
      report_line ""
      continue
    }
    set line $prefix
    set col $indent
    foreach word [split $para] {
      if { $word == "" } {
        continue
      }
      set word_len [string length $word]
      if { $col + $word_len + 1 > $width && $col > $indent } {
        report_line [string trimright $line]
        set line "$prefix$word "
        set col [expr { $indent + $word_len + 1 }]
      } else {
        append line "$word "
        set col [expr { $col + $word_len + 1 }]
      }
    }
    report_line [string trimright $line]
  }
}

# Approximate Markdown as wrap-friendly plain text for the help command.
proc md_help_to_text { md } {
  if { $md == "" } {
    return ""
  }
  set text $md
  # Fenced code blocks become indented lines.
  set text [regsub -all {```[a-zA-Z0-9_]*\n} $text ""]
  set text [string map {``` ""} $text]
  # Inline code, bold, italics.
  set text [regsub -all {`([^`]+)`} $text {\1}]
  set text [regsub -all {\*\*([^*]+)\*\*} $text {\1}]
  set text [regsub -all {\*([^*]+)\*} $text {\1}]
  return [string trim $text]
}

# This is used in lieu of command completion to make sdc commands
# like get_ports be abbreviated get_port.
proc define_cmd_alias { alias cmd } {
  eval "proc $alias { args } { eval [concat $cmd \$args] }"
  namespace export $alias
}

proc cmd_usage_error { cmd } {
  variable cmd_args

  if [info exists cmd_args($cmd)] {
    sta_error 161 "Usage: $cmd $cmd_args($cmd)"
  } else {
    sta_error 162 "Usage: $cmd argument error"
  }
}

################################################################

define_cmd_args "with_output_to_variable" { var { cmds }} \
  -help {The `with_output_to_variable` command redirects the output of Tcl commands to a variable.} \
  -arg_help {
    var {The name of a variable to save the output of commands to.}
    commands {Tcl commands that the output will be redirected from.}
  }

# with_output_to_variable variable { command args... }
proc with_output_to_variable { var_name args } {
  upvar 1 $var_name var

  set body [lindex $args 0]
  sta::redirect_string_begin;
  catch $body ret
  set var [sta::redirect_string_end]
  return $ret
}

################################################################

define_cmd_args "report_units" {} \
  -help {Report the units used for command arguments and reporting.

```
report_units
 time 1ns
 capacitance 1pF
 resistance 1kohm
 voltage 1v
 current 1A
 power 1pW
 distance 1um
```}

proc report_units { args } {
  check_argc_eq0 "report_units" $args
  foreach unit {"time" "capacitance" "resistance" "voltage" "current" "power" "distance"} {
    report_line " $unit [unit_scale_suffix $unit]"
  }
}

proc write_units_json { jsonfile } {
  set f [open $jsonfile w]
  puts $f "{"
  foreach unit {"time" "capacitance" "resistance" "voltage" "current" "power"} {
    puts $f "  \"$unit\": \"[unit_scale_suffix $unit]\","
  }
  puts $f "  \"distance\": \"[unit_scale_suffix distance]\""
  puts $f "}"
  close $f
}

################################################################

define_cmd_args "set_cmd_units" \
  {[-capacitance cap_unit] [-resistance res_unit] [-time time_unit]\
     [-voltage voltage_unit] [-current current_unit] [-power power_unit]\
     [-distance distance_unit]} \
  -help {The `set_cmd_units` command is used to change the units used by the STA command interpreter when parsing commands and reporting results. The default units are the units specified in the first Liberty library file that is read.

Units are specified as a scale factor followed by a unit name. The scale factors are as follows.

```
M 1E+6
k 1E+3
m 1E-3
u 1E-6
n 1E-9
p 1E-12
f 1E-15
```

An example of the `set_units` command is shown below.

```
set_cmd_units -time ns -capacitance pF -current mA -voltage V
              -resistance kOhm -distance um
```} \
  -arg_help {
    -capacitance {`cap_unit`: The capacitance scale factor followed by 'f'.}
    -resistance {`res_unit`: The resistance scale factor followed by 'ohm'.}
    -time {`time_unit`: The time scale factor followed by 's'.}
    -voltage {`voltage_unit`: The voltage scale factor followed by 'v'.}
    -current {`current_unit`: The current scale factor followed by 'A'.}
    -power {`power_unit`: The power scale factor followed by 'w'.}
    -distance {`distance_unit`: The distance scale factor followed by 'm'.}
  }

proc set_cmd_units { args } {
  parse_key_args "set_cmd_units" args \
    keys {-capacitance -resistance -time -voltage -current -power \
            -distance -digits -suffix} \
    flags {}

  check_argc_eq0 "set_cmd_units" $args
  set_unit_values "capacitance" -capacitance "f" keys
  set_unit_values "time" -time "s" keys
  set_unit_values "voltage" -voltage "v" keys
  set_unit_values "current" -current "A" keys
  set_unit_values "resistance" -resistance "ohm" keys
  set_unit_values "distance" -distance "m" keys
}

proc set_unit_values { unit key suffix key_var } {
  upvar 1 $key_var keys
  if { [info exists keys($key)] } {
    set value $keys($key)
    set suffix_length [string length $suffix]
    set arg_suffix [string range $value end-[expr $suffix_length - 1] end]
    if { [string match -nocase $arg_suffix $suffix] } {
      set arg_prefix [string range $value 0 end-$suffix_length]
      if { [regexp "^(10*\\\.?0*)?(\[Mkmunpf\])?$" $arg_prefix ignore mult prefix] } {
        #puts "$arg_prefix '$mult' '$prefix'"
        if { $mult == "" } {
          set mult 1
        }
        set scale [unit_prefix_scale $unit $prefix ]
        set_cmd_unit_scale $unit [expr $scale * $mult]
      } else {
        sta_error 166 "unknown unit $unit prefix '${arg_prefix}'."
      }
    } else {
      sta_error 167 "incorrect unit suffix '$arg_suffix'."
    }
    if [info exists keys(-digits)] {
      set_cmd_unit_digits $unit $keys(-digits)
    }
    if [info exists keys(-suffix)] {
      set_cmd_unit_suffix $unit $keys(-suffix)
    }
  }
}

################################################################

define_cmd_args "delete_from_list" {list delete} \
  -help {Remove objects from a list.} \
  -arg_help {
    list {A list of objects.}
    objects {A list of objects to delete from list.}
  }

proc delete_from_list { list delete } {
  delete_objects_from_list_cmd $list $delete
}

proc delete_objects_from_list_cmd { list delete } {
  if { $list != {} } {
    set list0 [lindex $list 0]
    set list_is_objects [is_object $list0]
    foreach obj $delete {
      # If the list is a collection of tcl objects (returned by get_*),
      # convert the obj to be removed from a name to an object of the same
      # type.
      if {$list_is_objects && ![is_object $obj]} {
        set list_type [object_type $list0]
        if {$list_type == "Clock"} {
          set obj [find_clock $obj]
        } elseif {$list_type == "Port"} {
          set top_instance [top_instance]
          set top_cell [$top_instance cell]
          set obj [$top_cell find_port $obj]
        } elseif {$list_type == "Pin"} {
          set obj [find_pin $obj]
        } elseif {$list_type == "Instance"} {
          set obj [find_instance $obj]
        } elseif {$list_type == "Net"} {
          set obj [find_net $obj]
        } elseif {$list_type == "LibertyLibrary"} {
          set obj [find_liberty $obj]
        } elseif {$list_type == "LibertyCell"} {
          set obj [find_liberty_cell $obj]
        } elseif {$list_type == "LibertyPort"} {
          set obj [get_lib_pins $obj]
        } else {
          sta_error 164 "unsupported object type $list_type."
        }
      }
      set index [lsearch $list $obj]
      if { $index != -1 } {
        set list [lreplace $list $index $index]
      }
    }
  }
  return $list
}
  
################################################################
  
proc set_cmd_namespace { namespc } {
  if { $namespc == "sdc" || $namespc == "sta" } {
    set_cmd_namespace_cmd $namespc
  } else {
    sta_error 165 "unknown namespace $namespc."
  }
}
  
################################################################

define_cmd_args "report_object_full_names" {objects} \
  -help {The `report_object_full_names` command prints the hierarchical name of each object, sorted by full name.} \
  -arg_help {
    objects {A list of objects returned by a `get_*` command.}
  }

proc report_object_full_names { objects } {
  foreach obj [sort_by_full_name $objects] {
    report_line [get_full_name $obj]
  }
}

define_cmd_args "report_object_names" {objects} \
  -help {The `report_object_names` command prints the name of each object, sorted by name.} \
  -arg_help {
    objects {A list of objects returned by a `get_*` command.}
  }

proc report_object_names { objects } {
  foreach obj [sort_by_name $objects] {
    report_line [get_name $obj]
  }
}

################################################################

define_cmd_args "get_name" {object} \
  -help {Return the name of object. Equivalent to [`get_property` object name].} \
  -arg_help {
    object {A library, cell, port, instance, pin or timing arc object.}
  }
define_cmd_args "get_full_name" {object} \
  -help {Return the name of object. Equivalent to [`get_property` object full_name].} \
  -arg_help {
    object {A library, cell, port, instance, pin or timing arc object.}
  }

################################################################

proc get_name { object } {
  return [get_object_property $object "name"]
}

proc get_full_name { object } {
  return [get_object_property $object "full_name"]
}

proc sort_by_name { objects } {
  return [lsort -command name_cmp $objects]
}

proc name_cmp { obj1 obj2 } {
  return [string compare [get_name $obj1] [get_name $obj2]]
}

proc sort_by_full_name { objects } {
  return [lsort -command full_name_cmp $objects]
}

proc full_name_cmp { obj1 obj2 } {
  return [string compare [get_full_name $obj1] [get_full_name $obj2]]
}

# namespace sta
}
