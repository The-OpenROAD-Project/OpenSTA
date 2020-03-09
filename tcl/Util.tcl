# OpenSTA, Static Timing Analyzer
# Copyright (c) 2020, Parallax Software, Inc.
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

# The sta namespace is used for all commands defined by the sta.
# Use define_cmd_args to define command arguments for the help
# command, and export the command name to the global namespace.
# Global variables must be defined as
#  set global_var init_value
# File local variables must be defined as
#  variable sta_var init_value

namespace eval sta {

# Parse arg_var for keyword/values pairs and flags.
# $key_var(key) -> key_value
# $flag_var(flag) -> 1 if the flag is present
# Keys and flags are removed from arg_var in the caller.
proc parse_key_args { cmd arg_var key_var keys {flag_var ""} {flags {}} \
			{unknown_key_is_error 1} } {
  upvar 1 $arg_var args
  upvar 1 $key_var key_value
  upvar 1 $flag_var flag_present
  set args_rtn {}
  while { $args != "" } {
    set arg [lindex $args 0]
    if { [is_keyword_arg $arg] } {
      set key_index [lsearch -exact $keys $arg]
      if { $key_index >= 0 } {
	set key $arg
	if { [llength $args] == 1 } {
	  sta_error "$cmd $key missing value."
	}
	set key_value($key) [lindex $args 1]
	set args [lrange $args 1 end]
      } else {
	set flag_index [lsearch -exact $flags $arg]
	if { $flag_index >= 0 } {
	  set flag_present($arg) 1
	} else {
	  # No exact keyword/flag match found.
	  # Try finding a keyword/flag that begins with
	  # the same substring.
	  set wild_arg "${arg}*"
	  set key_index [lsearch -glob $keys $wild_arg]
	  if { $key_index >= 0 } {
	    set key [lindex $keys $key_index]
	    if { [llength $args] == 1 } {
	      sta_error "$cmd $key missing value."
	    }
	    set key_value($key) [lindex $args 1]
	    set args [lrange $args 1 end]
	  } else {
	    set flag_index [lsearch -glob $flags $wild_arg]
	    if { $flag_index >= 0 } {
	      set flag [lindex $flags $flag_index]
	      set flag_present($flag) 1
	    } elseif { $unknown_key_is_error } {
	      sta_error "$cmd $arg is not a known keyword or flag."
	    } else {
	      lappend args_rtn $arg
	    }
	  }
	}
      }
    } else {
      lappend args_rtn $arg
    }
    set args [lrange $args 1 end]
  }
  set args $args_rtn
}

# Check for keyword args in arg_var.
proc check_for_key_args { cmd arg_var } {
  upvar 1 $arg_var args
  set args_rtn {}
  while { $args != "" } {
    set arg [lindex $args 0]
    if { [is_keyword_arg $arg] } {
      sta_error "$cmd $arg is not a known keyword or flag."
    } else {
      lappend args_rtn $arg
    }
    set args [lrange $args 1 end]
  }
  set args $args_rtn
}

proc is_keyword_arg { arg } {
  if { [string length $arg] >= 2 \
	 && [string index $arg 0] == "-" \
	 && [string is alpha [string index $arg 1]] } {
    return 1
  } else {
    return 0
  }
}

################################################################

# Define a procedure that checks the args for redirection using unix
# shell redirection syntax.
# The value of the last expression in the body is returned.
proc proc_redirect { proc_name body } {
  set proc_body [concat "proc $proc_name { args } {" \
		   "global errorCode errorInfo;" \
		   "set redirect \[parse_redirect_args args\];" \
		   "set code \[catch {" $body "} ret \];" \
		   "if {\$redirect} { redirect_file_end };" \
		   "if {\$code == 1} {return -code \$code -errorcode \$errorCode -errorinfo \$errorInfo \$ret} else {return \$ret} }" ]
  eval $proc_body
}

proc parse_redirect_args { arg_var } {
  upvar 1 $arg_var args
  set argc [llength $args]
  if { $argc >= 1 } {
    set last_arg [lindex $args [expr $argc - 1]]
    # arg >>filename
    if { [string range $last_arg 0 1] == ">>" } {
      set redirect_file [file nativename [string range $last_arg 2 end]]
      set args [lrange $args 0 [expr $argc - 2]]
      redirect_file_append_begin $redirect_file
      return 1
    # arg >filename
    } elseif { [string range $last_arg 0 0] == ">" } {
      set redirect_file [file nativename [string range $last_arg 1 end]]
      set args [lrange $args 0 [expr $argc - 2]]
      redirect_file_begin $redirect_file
      return 1
    }
  }
  if { $argc >= 2 } {
    set next_last_arg [lindex $args [expr $argc - 2]]
    # arg > filename
    if { $next_last_arg == ">" } {
      set redirect_file [file nativename [lindex $args end]]
      set args [lrange $args 0 [expr $argc - 3]]
      redirect_file_begin $redirect_file
      return 1
    # arg >> filename
    } elseif { $next_last_arg == ">>" } {
      set redirect_file [file nativename [lindex $args end]]
      set args [lrange $args 0 [expr $argc - 3]]
      redirect_file_append_begin $redirect_file
      return 1
    }
  }
  return 0
}

################################################################

proc define_cmd_args { cmd arglist } {
  variable cmd_args

  set cmd_args($cmd) $arglist
  namespace export $cmd
}

# Hidden commands are exported to the global namespace but are not
# shown by the "help" command.
proc define_hidden_cmd_args { cmd arglist } {
  namespace export $cmd
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
    sta_error "Usage: $cmd $cmd_args($cmd)"
  } else {
    sta_error "Usage: $cmd argument error"
  }
}

################################################################

define_cmd_args "help" {[pattern]}

proc_redirect help {
  variable cmd_args

  set arg_count [llength $args]
  if { $arg_count == 0 } {
    set pattern "*"
  } elseif { $arg_count == 1 } {
    set pattern [lindex $args 0]
  } else {
    cmd_usage_error "help"
  }
  set matches [array names cmd_args $pattern]
  if { $matches != {} } {
    foreach cmd [lsort $matches] {
      show_cmd_args $cmd
    }
  } else {
    sta_warn "no commands match '$pattern'."
  }
}

proc show_cmd_args { cmd } {
  variable cmd_args

  set max_col 80
  set indent 2
  set indent_str "  "
  puts -nonewline $cmd
  set col [string length $cmd]
  set arglist $cmd_args($cmd)
  # Break the arglist up into max_col length lines.
  while {1} {
    if {[regexp {(^ *)([a-zA-Z0-9>_\|\-]+|\[.*\])(.*)} \
	   $arglist ignore space arg rest]} {
      set arg_length [string length $arg]
      if { $col + $arg_length < $max_col } {
	puts -nonewline " $arg"
	set col [expr $col + $arg_length + 1]
      } else {
	puts ""
	puts -nonewline "$indent_str $arg"
	set col [expr $indent + $arg_length + 1]
      }
      set arglist $rest
    } else {
      puts ""
      break
    }
  }
}

################################################################

proc sta_warn { msg } {
  variable sdc_file
  variable sdc_line
  if { [info exists sdc_file] } {
    puts "Warning: [file tail $sdc_file], $sdc_line $msg"
  } else {
    puts "Warning: $msg"
  }
}

proc sta_error { msg } {
  variable sdc_file
  variable sdc_line
  if { [info exists sdc_file] } {
    error "Error: [file tail $sdc_file], $sdc_line $msg"
  } else {
    error "Error: $msg"
  }
}

proc sta_warn_error { warn_error msg } {
  if { $warn_error == "warn" } {
    sta_warn $msg
  } else {
    sta_error $msg
  }
}

# Defined by StaTcl.i
define_cmd_args "elapsed_run_time" {}
define_cmd_args "user_run_time" {}

# Write run time statistics to filename.
proc write_stats { filename } {
  if { ![catch {open $filename w} stream] } {
    puts $stream "[elapsed_run_time] [user_run_time] [mem]"
    close $stream
  }
}

################################################################

# Begin/end logging all output to a file.
# Defined by StaTcl.i
define_cmd_args "log_begin" {filename}
define_cmd_args "log_end" {}

# set_debug is NOT in the global namespace
# because it isn't intended for nosy users.

################################################################

proc check_argc_eq0 { cmd arglist } {
  if { $arglist != {} } {
    sta_error "$cmd positional arguments not supported."
  }
}

proc check_argc_eq1 { cmd arglist } {
  if { [llength $arglist] != 1 } {
    sta_error "$cmd requires one positional argument."
  }
}

proc check_argc_eq0or1 { cmd arglist } {
  set argc [llength $arglist]
  if { $argc != 0 && $argc != 1 } {
    sta_error "$cmd requires zero or one positional arguments."
  }
}

proc check_argc_eq2 { cmd arglist } {
  if { [llength $arglist] != 2 } {
    sta_error "$cmd requires two positional arguments."
  }
}

proc check_argc_eq1or2 { cmd arglist } {
  set argc [llength $arglist]
  if { $argc != 1 && $argc != 2 } {
    sta_error "$cmd requires one or two positional arguments."
  }
}

proc check_argc_eq3 { cmd arglist } {
  if { [llength $arglist] != 3 } {
    sta_error "$cmd requires three positional arguments."
  }
}

proc check_argc_eq4 { cmd arglist } {
  if { [llength $arglist] != 4 } {
    sta_error "$cmd requires four positional arguments."
  }
}

################################################################

proc check_float { cmd_arg arg } {
  if {![string is double $arg]} {
    sta_error "$cmd_arg '$arg' is not a float."
  }
}

proc check_positive_float { cmd_arg arg } {
  if {!([string is double $arg] && $arg >= 0.0)} {
    sta_error "$cmd_arg '$arg' is not a positive float."
  }
}

proc check_integer { cmd_arg arg } {
  if {!([string is integer $arg])} {
    sta_error "$cmd_arg '$arg' is not an integer."
  }
}

proc check_positive_integer { cmd_arg arg } {
  if {!([string is integer $arg] && $arg >= 0)} {
    sta_error "$cmd_arg '$arg' is not a positive integer."
  }
}

proc check_cardinal { cmd_arg arg } {
  if {!([string is integer $arg] && $arg >= 1)} {
    sta_error "$cmd_arg '$arg' is not an integer greater than or equal to one."
  }
}

proc check_percent { cmd_arg arg } {
  if {!([string is double $arg] && $arg >= 0.0 && $arg <= 100.0)} {
    sta_error "$cmd_arg '$arg' is not between 0 and 100."
  }
}

# sta namespace end
}

################################################################

# The builtin Tcl "source" and "unknown" commands are redefined by sta.
# This rename provides a mechanism to refer to the original TCL
# command.
# Protected so this file can be reloaded without blowing up.
if { ![info exists renamed_source] } {
  rename source builtin_source
  rename unknown builtin_unknown
  set renamed_source 1
}

# Numeric expressions eval to themselves so braces aren't required
# around bus names like foo[2] or foo[*].
proc sta_unknown { args } {
  global errorCode errorInfo
  
  set name [lindex $args 0]
  if { [llength $args] == 1 \
	 && ([string is integer $name] || [string equal $name "*"]) } {
    return "\[$args\]"
  } else {
    # Implement command name abbreviation from init.tcl/unknown.
    # Remove restrictions in that version that prevent it from
    # running in non-interactive interpreters.
    
    set ret [catch {set cmds [info commands $name*]} msg]
    if {[string equal $name "::"]} {
      set name ""
    }
    if {$ret != 0} {
      return -code $ret -errorcode $errorCode \
	"error in unknown while checking if \"$name\" is a unique command abbreviation: $msg"
    }
    if {[llength $cmds] == 1} {
      return [uplevel 1 [lreplace $args 0 0 $cmds]]
    }
    if {[llength $cmds]} {
      if {[string equal $name ""]} {
	return -code error "empty command name \"\""
      } else {
	return -code error \
	  "ambiguous command name \"$name\": [lsort $cmds]"
      }
    } else {
      # I cannot figure out why the first call to ::history add
      # that presumably loads history.tcl causes the following error:
      # Error: history.tcl, 306 invoked "return" outside of a proc.
      # But this squashes the error.
      if { [lindex $args 0] == "::history" } {
	return ""
      } else {
	return [uplevel 1 builtin_unknown $args]
      }
    }
  }
}

namespace unknown sta_unknown
