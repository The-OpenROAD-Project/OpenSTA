# OpenSTA, Static Timing Analyzer
# Copyright (c) 2018, Parallax Software, Inc.
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
      sta_warn "$cmd $arg is not a known keyword or flag."
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
# This rename provices a mechanism to refer to the original TCL
# command.
rename source builtin_source

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
      return [uplevel 1 builtin_unknown $args]
    }
  }
}

# Copied from init.tcl
proc builtin_unknown args {
    variable ::tcl::UnknownPending
    global auto_noexec auto_noload env tcl_interactive


    if {[info exists ::errorInfo]} {
	set savedErrorInfo $::errorInfo
    }
    if {[info exists ::errorCode]} {
	set savedErrorCode $::errorCode
    }

    set name [lindex $args 0]
    if {![info exists auto_noload]} {
	#
	# Make sure we're not trying to load the same proc twice.
	#
	if {[info exists UnknownPending($name)]} {
	    return -code error "self-referential recursion\
		    in \"unknown\" for command \"$name\""
	}
	set UnknownPending($name) pending
	set ret [catch {
		auto_load $name [uplevel 1 {::namespace current}]
	} msg opts]
	unset UnknownPending($name)
	if {$ret != 0} {
	    dict append opts -errorinfo "\n    (autoloading \"$name\")"
	    return -options $opts $msg
	}
	if {![array size UnknownPending]} {
	    unset UnknownPending
	}
	if {$msg} {
	    if {[info exists savedErrorCode]} {
		set ::errorCode $savedErrorCode
	    } else {
		unset -nocomplain ::errorCode
	    }
	    if {[info exists savedErrorInfo]} {
		set ::errorInfo $savedErrorInfo
	    } else {
		unset -nocomplain ::errorInfo
	    }
	    set code [catch {uplevel 1 $args} msg opts]
	    if {$code ==  1} {
		#
		# Compute stack trace contribution from the [uplevel].
		# Note the dependence on how Tcl_AddErrorInfo, etc.
		# construct the stack trace.
		#
		set errorInfo [dict get $opts -errorinfo]
		set errorCode [dict get $opts -errorcode]
		set cinfo $args
		if {[string bytelength $cinfo] > 150} {
		    set cinfo [string range $cinfo 0 150]
		    while {[string bytelength $cinfo] > 150} {
			set cinfo [string range $cinfo 0 end-1]
		    }
		    append cinfo ...
		}
		append cinfo "\"\n    (\"uplevel\" body line 1)"
		append cinfo "\n    invoked from within"
		append cinfo "\n\"uplevel 1 \$args\""
		#
		# Try each possible form of the stack trace
		# and trim the extra contribution from the matching case
		#
		set expect "$msg\n    while executing\n\"$cinfo"
		if {$errorInfo eq $expect} {
		    #
		    # The stack has only the eval from the expanded command
		    # Do not generate any stack trace here.
		    #
		    dict unset opts -errorinfo
		    dict incr opts -level
		    return -options $opts $msg
		}
		#
		# Stack trace is nested, trim off just the contribution
		# from the extra "eval" of $args due to the "catch" above.
		#
		set expect "\n    invoked from within\n\"$cinfo"
		set exlen [string length $expect]
		set eilen [string length $errorInfo]
		set i [expr {$eilen - $exlen - 1}]
		set einfo [string range $errorInfo 0 $i]
		#
		# For now verify that $errorInfo consists of what we are about
		# to return plus what we expected to trim off.
		#
		if {$errorInfo ne "$einfo$expect"} {
		    error "Tcl bug: unexpected stack trace in \"unknown\"" {} \
			[list CORE UNKNOWN BADTRACE $einfo $expect $errorInfo]
		}
		return -code error -errorcode $errorCode \
			-errorinfo $einfo $msg
	    } else {
		dict incr opts -level
		return -options $opts $msg
	    }
	}
    }

    if {([info level] == 1) && ([info script] eq "") \
	    && [info exists tcl_interactive] && $tcl_interactive} {
	if {![info exists auto_noexec]} {
	    set new [auto_execok $name]
	    if {$new ne ""} {
		set redir ""
		if {[namespace which -command console] eq ""} {
		    set redir ">&@stdout <@stdin"
		}
		uplevel 1 [list ::catch \
			[concat exec $redir $new [lrange $args 1 end]] \
			::tcl::UnknownResult ::tcl::UnknownOptions]
		dict incr ::tcl::UnknownOptions -level
		return -options $::tcl::UnknownOptions $::tcl::UnknownResult
	    }
	}
	if {$name eq "!!"} {
	    set newcmd [history event]
	} elseif {[regexp {^!(.+)$} $name -> event]} {
	    set newcmd [history event $event]
	} elseif {[regexp {^\^([^^]*)\^([^^]*)\^?$} $name -> old new]} {
	    set newcmd [history event -1]
	    catch {regsub -all -- $old $newcmd $new newcmd}
	}
	if {[info exists newcmd]} {
	    tclLog $newcmd
	    history change $newcmd 0
	    uplevel 1 [list ::catch $newcmd \
		    ::tcl::UnknownResult ::tcl::UnknownOptions]
	    dict incr ::tcl::UnknownOptions -level
	    return -options $::tcl::UnknownOptions $::tcl::UnknownResult
	}

	set ret [catch {set candidates [info commands $name*]} msg]
	if {$name eq "::"} {
	    set name ""
	}
	if {$ret != 0} {
	    dict append opts -errorinfo \
		    "\n    (expanding command prefix \"$name\" in unknown)"
	    return -options $opts $msg
	}
	# Filter out bogus matches when $name contained
	# a glob-special char [Bug 946952]
	if {$name eq ""} {
	    # Handle empty $name separately due to strangeness
	    # in [string first] (See RFE 1243354)
	    set cmds $candidates
	} else {
	    set cmds [list]
	    foreach x $candidates {
		if {[string first $name $x] == 0} {
		    lappend cmds $x
		}
	    }
	}
	if {[llength $cmds] == 1} {
	    uplevel 1 [list ::catch [lreplace $args 0 0 [lindex $cmds 0]] \
		    ::tcl::UnknownResult ::tcl::UnknownOptions]
	    dict incr ::tcl::UnknownOptions -level
	    return -options $::tcl::UnknownOptions $::tcl::UnknownResult
	}
	if {[llength $cmds]} {
	    return -code error "ambiguous command name \"$name\": [lsort $cmds]"
	}
    }
    return -code error "invalid command name \"$name\""
}

namespace unknown sta_unknown
