# OpenSTA, Static Timing Analyzer
# Copyright (c) 2025, Parallax Software, Inc.
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
	  sta_error 560 "$cmd $key missing value."
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
	      sta_error 561 "$cmd $key missing value."
	    }
	    set key_value($key) [lindex $args 1]
	    set args [lrange $args 1 end]
	  } else {
	    set flag_index [lsearch -glob $flags $wild_arg]
	    if { $flag_index >= 0 } {
	      set flag [lindex $flags $flag_index]
	      set flag_present($flag) 1
	    } elseif { $unknown_key_is_error } {
	      sta_error 562 "$cmd $arg is not a known keyword or flag."
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
      sta_error 563 "$cmd $arg is not a known keyword or flag."
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

################################################################

proc sta_warn { msg_id msg } {
  if { [sdc_filename] != "" } {
    report_file_warn $msg_id [file tail [sdc_filename]] [sdc_file_line] $msg
  } else {
    report_warn $msg_id $msg
  }
}

proc sta_error { msg_id msg } {
  if { ! [is_suppressed $msg_id] } {
    if { [sdc_filename] != "" } {
      error "Error: [file tail [sdc_filename]] line [sdc_file_line], $msg"
    } else {
      error "Error: $msg"
    }
  }
}

proc sdc_filename {} {
  return [info script]
}

proc sdc_file_line { } {
  variable include_line
  for { set fr [info frame] } { $fr >= 0 } { incr fr -1 } {
    set type [dict get [info frame $fr] type]
    if { $type == "source" } {
      return [dict get [info frame $fr] line]
    }
    if { $type == "proc" \
           && [lindex [dict get [info frame $fr] cmd] 0] == "include_file" } {
      return $include_line
    }
  }
  return 1
}

proc sta_warn_error { msg_id warn_error msg } {
  if { $warn_error == "warn" } {
    sta_warn $msg_id $msg
  } else {
    sta_error $msg_id $msg
  }
}

define_cmd_args "suppress_msg" msg_ids

proc suppress_msg { args } {
  foreach msg_id $args {
    check_integer "msg_id" $msg_id
    suppress_msg_id $msg_id
  }
}

define_cmd_args "unsuppress_msg" msg_ids

proc unsuppress_msg { args } {
  foreach msg_id $args {
    check_integer "msg_id" $msg_id
    unsuppress_msg_id $msg_id
  }
}

# Defined by StaTcl.i
define_cmd_args "elapsed_run_time" {}
define_cmd_args "user_run_time" {}

# Write run time statistics to filename.
proc write_stats { filename } {
  if { ![catch {open $filename w} stream] } {
    puts $stream "[elapsed_run_time] [user_run_time] [memory_usage]"
    close $stream
  }
}

################################################################

# Begin/end logging all output to a file.
define_cmd_args "log_begin" { filename }

proc log_begin { filename } {
  log_begin_cmd [file nativename $filename]
}

# Defined by StaTcl.i
define_cmd_args "log_end" {}

# set_debug is NOT in the global namespace
# because it isn't intended for nosy users.

################################################################

proc check_argc_eq0 { cmd arglist } {
  if { $arglist != {} } {
    sta_error 564 "$cmd positional arguments not supported."
  }
}

proc check_argc_eq1 { cmd arglist } {
  if { [llength $arglist] != 1 } {
    sta_error 565 "$cmd requires one positional argument."
  }
}

proc check_argc_eq0or1 { cmd arglist } {
  set argc [llength $arglist]
  if { $argc != 0 && $argc != 1 } {
    sta_error 566 "$cmd requires zero or one positional arguments."
  }
}

proc check_argc_eq2 { cmd arglist } {
  if { [llength $arglist] != 2 } {
    sta_error 567 "$cmd requires two positional arguments."
  }
}

proc check_argc_eq1or2 { cmd arglist } {
  set argc [llength $arglist]
  if { $argc != 1 && $argc != 2 } {
    sta_error 568 "$cmd requires one or two positional arguments."
  }
}

proc check_argc_eq3 { cmd arglist } {
  if { [llength $arglist] != 3 } {
    sta_error 569 "$cmd requires three positional arguments."
  }
}

proc check_argc_eq4 { cmd arglist } {
  if { [llength $arglist] != 4 } {
    sta_error 570 "$cmd requires four positional arguments."
  }
}

################################################################

proc check_float { cmd_arg arg } {
  if {![string is double $arg]} {
    sta_error 571 "$cmd_arg '$arg' is not a float."
  }
}

proc check_positive_float { cmd_arg arg } {
  if {!([string is double $arg] && $arg >= 0.0)} {
    sta_error 572 "$cmd_arg '$arg' is not a positive float."
  }
}

proc check_integer { cmd_arg arg } {
  if {!([string is integer $arg])} {
    sta_error 573 "$cmd_arg '$arg' is not an integer."
  }
}

proc check_positive_integer { cmd_arg arg } {
  if {!([string is integer $arg] && $arg >= 0)} {
    sta_error 574 "$cmd_arg '$arg' is not a positive integer."
  }
}

proc check_cardinal { cmd_arg arg } {
  if {!([string is integer $arg] && $arg >= 1)} {
    sta_error 575 "$cmd_arg '$arg' is not an integer greater than or equal to one."
  }
}

proc check_percent { cmd_arg arg } {
  if {!([string is double $arg] && $arg >= 0.0 && $arg <= 100.0)} {
    sta_error 576 "$cmd_arg '$arg' is not between 0 and 100."
  }
}

################################################################

set ::sta_continue_on_error 0

define_cmd_args "include" \
  {[-echo] filename [> filename] [>> filename]}

# Tcl "source" command analog to support -echo and -verbose return values.
proc_redirect include  {
  parse_key_args "include" args keys {-encoding} flags {-echo -verbose}
  if { [llength $args] != 1 } {
    cmd_usage_error "include"
  }
  set echo [info exists flags(-echo)]
  set verbose [info exists flags(-verbose)]
  set filename [file nativename [lindex $args 0]]
  include_file $filename $echo $verbose
}

proc include_file { filename echo verbose } {
  global sta_continue_on_error
  variable include_line
  
  set prev_filename [info script]
  if { [info exists include_line] } {
    set prev_line $include_line
  }
  try {
    # set filename/line for sta_warn/error
    info script $filename
    set include_line 1
    if [catch {open $filename r} stream] {
      sta_error 340 "cannot open '$filename'."
    } else {
      if { [file extension $filename] == ".gz" } {
        if { [info commands zlib] == "" } {
          sta_error 339 "tcl version > 8.6 required for zlib support."
        }
        zlib push gunzip $stream
      }
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
                report_line "Error: [file tail $filename], $include_line $error"
              }
              set error {}
            } else {
              break
            }
          }
        }
        incr include_line
      }
      close $stream
      if { $cmd != {} } {
        sta_error 341 "incomplete command at end of file."
      }
      if { $error != {} } {
        # Only prepend error message with file/line once.
        if { [string first "Error" $error] == 0 } {
          error $error
        } else {
          error "Error: [file tail $filename], $include_line $error"
        }
      }
    }
  } finally {
    if { $prev_filename != "" } {
      info script $prev_filename
    }
    if { [info exists prev_line] } {
      set include_line $prev_line
    } else {
      unset include_line
    }
  }
}

# sta namespace end
}

################################################################

# Bus signal names like foo[2] or bar[31:0] use brackets that
# look like "eval" to TCL. Catch the numeric "function" with the
# namespace's unknown handler and return the value instead of an error.
proc sta_unknown { args } {
  global errorCode errorInfo
  
  set name [lindex $args 0]
  if { [llength $args] == 1 && [is_bus_subscript $args] } {
    return "\[$args\]"
  }
  
  # Command name abbreviation support.
  set ret [catch {set cmds [info commands $name*]} msg]
  if {[string equal $name "::"]} {
    set name ""
  }
  if { $ret != 0 } {
    return -code $ret -errorcode $errorCode \
      "Error in unknown while checking if \"$name\" is a unique command abbreviation: $msg."
  }
  if { [llength $cmds] == 1 } {
    return [uplevel 1 [lreplace $args 0 0 $cmds]]
  }
  if { [llength $cmds] > 1 } {
    if {[string equal $name ""]} {
      return -code error "Empty command name \"\""
    } else {
      return -code error \
        "Ambiguous command name \"$name\": [lsort $cmds]."
    }
  }
  return [uplevel 1 [::unknown {*}$args]]
}

proc is_bus_subscript { subscript } {
  return [expr [string is integer $subscript] \
            || [string match $subscript "*"] \
            || [regexp {[0-9]+:[0-9]} $subscript]]
}

namespace unknown sta_unknown
