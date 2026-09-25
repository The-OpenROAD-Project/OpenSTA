#!/bin/sh
# The next line is executed by /bin/sh, but not Tcl \
exec sta -no_splash -no_init -exit $0 ${1+"$@"}

# OpenSTA, Static Timing Analyzer
# Copyright (c) 2026, Parallax Software, Inc.
#
# Write doc/Commands.md, doc/Variables.md, and doc/CommandLine.md
# from the live command/variable help registry. These files are not
# checked in; CMake and Read the Docs regenerate them.
# Usage: sta -no_splash -no_init -exit etc/WriteCmdDocs.tcl

namespace eval sta {

proc write_cmd_docs {} {
  set script [info script]
  set sta_home [file dirname [file dirname [file normalize $script]]]
  write_commands_md [file join $sta_home doc Commands.md]
  write_variables_md [file join $sta_home doc Variables.md]
  write_command_line_md [file join $sta_home doc CommandLine.md]
}

proc write_commands_md { path } {
  variable cmd_args

  set f [open $path w]
  puts $f "# Commands"
  puts $f ""
  puts $f "This page is generated from the live command registry."
  puts $f "Do not edit it by hand; rebuild `sta` to regenerate."
  puts $f ""
  puts $f "Use `help <command>` in the Tcl interpreter for the same text."
  puts $f ""

  foreach cmd [lsort [array names cmd_args]] {
    set desc [cmd_help_text $cmd]
    if { $desc == "" } {
      # Hidden commands (etc/CheckCmdHelp.tcl hidden_cmds) have no prose.
      continue
    }
    puts $f "## $cmd"
    puts $f ""
    # HTML <pre> so option flags can link to the descriptions below.
    # A markdown fence cannot contain links.
    puts $f "<pre><code>[cmd_synopsis_html $cmd]</code></pre>"
    puts $f ""
    puts $f $desc
    puts $f ""
    set opts [cmd_synopsis_options $cmd_args($cmd)]
    set any 0
    foreach opt $opts {
      set opt_desc [cmd_arg_help_text $cmd $opt]
      if { $opt_desc != "" } {
        if { !$any } {
          puts $f "### Options"
          puts $f ""
          set any 1
        }
        write_opt_help_md $f $cmd $opt $opt_desc
      }
    }
  }
  close $f
}

# Split "`min`: foo. `max`: bar." style help into one list item per value.
proc enum_help_items { desc } {
  set re {`[^`]+`: }
  set spans [regexp -all -inline -indices $re $desc]
  if { [llength $spans] < 2 } {
    return {}
  }
  set items {}
  set n [llength $spans]
  for { set i 0 } { $i < $n } { incr i } {
    lassign [lindex $spans $i] start end
    set marker [string range $desc $start $end]
    regexp {`([^`]+)`: } $marker -> token
    if { $i + 1 < $n } {
      lassign [lindex $spans [expr { $i + 1 }]] next_start
      set text [string range $desc [expr { $end + 1 }] \
                  [expr { $next_start - 1 }]]
    } else {
      set text [string range $desc [expr { $end + 1 }] end]
    }
    lappend items [list $token [string trim $text]]
  }
  return $items
}

proc write_opt_help_md { f cmd opt desc } {
  puts $f "`$opt` \{: #[cmd_opt_anchor $cmd $opt] \}"
  set items [enum_help_items $desc]
  if { $items != {} } {
    set first 1
    foreach item $items {
      lassign $item token text
      if { $first } {
        puts $f ": - `$token`: $text"
        set first 0
      } else {
        puts $f "  - `$token`: $text"
      }
    }
  } else {
    set lines [split $desc "\n"]
    puts $f ": [lindex $lines 0]"
    foreach line [lrange $lines 1 end] {
      puts $f "  $line"
    }
  }
  puts $f ""
}

proc html_escape { s } {
  return [string map {& &amp; < &lt; > &gt; \" &quot;} $s]
}

# Fragment for a documented option, unique per command (many share -from).
proc cmd_opt_anchor { cmd opt } {
  return "opt-$cmd-[string range $opt 1 end]"
}

# One option/argument token per line so long synopses stay readable.
# Documented flags are links to the option description.
proc cmd_synopsis_html { cmd } {
  variable cmd_args

  set arglist [string trim $cmd_args($cmd)]
  set html [html_escape $cmd]
  if { $arglist == "" } {
    return $html
  }
  set tokens [cmd_synopsis_tokens $arglist]
  if { $tokens == {} } {
    append html " " [html_escape $arglist]
    return $html
  }
  foreach tok $tokens {
    append html "\n    " [cmd_synopsis_token_html $cmd $tok]
  }
  return $html
}

proc cmd_synopsis_token_html { cmd token } {
  variable cmd_args

  set html ""
  set i 0
  set n [string length $token]
  while { $i < $n } {
    set rest [string range $token $i end]
    if { [regexp {^(-[a-zA-Z][a-zA-Z0-9_]*)} $rest match] } {
      set desc [cmd_arg_help_text $cmd $match]
      if { $desc != "" } {
        set canon [cmd_arg_help_group_canonical $cmd_args($cmd) $match]
        set href [cmd_opt_anchor $cmd $canon]
        append html "<a href=\"#$href\">[html_escape $match]</a>"
      } else {
        append html [html_escape $match]
      }
      incr i [string length $match]
    } else {
      append html [html_escape [string index $token $i]]
      incr i
    }
  }
  return $html
}

# Same token split as show_cmd_args in tcl/CmdUtil.tcl: a [bracketed]
# group or a word of letters, digits, _, |, -, and backslash.
proc cmd_synopsis_tokens { arglist } {
  set tokens {}
  while {1} {
    if {[regexp {(^[\n ]*)([a-zA-Z0-9_\\\|\-]+|\[[^\[]+\])(.*)} \
           $arglist ignore space arg rest]} {
      lappend tokens $arg
      set arglist $rest
    } else {
      set rest [string trim $arglist]
      if { $rest != "" } {
        lappend tokens $rest
      }
      break
    }
  }
  return $tokens
}

proc write_variables_md { path } {
  variable var_help

  set f [open $path w]
  puts $f "# Variables"
  puts $f ""
  puts $f "This page is generated from `define_var_help`."
  puts $f "Do not edit it by hand; rebuild `sta` to regenerate."
  puts $f ""
  puts $f "Use `help <variable>` in the Tcl interpreter for the same text."
  puts $f ""

  foreach var [lsort [array names var_help]] {
    puts $f "## $var"
    puts $f ""
    set values [var_help_values $var]
    if { $values != "" } {
      puts $f "```"
      puts $f "$var $values"
      puts $f "```"
      puts $f ""
    }
    set desc [var_help_text $var]
    if { $desc != "" } {
      puts $f $desc
      puts $f ""
    }
  }
  close $f
}

proc write_command_line_md { path } {
  set sta_bin [info nameofexecutable]
  set usage [exec $sta_bin -help]
  # Usage line includes the absolute argv[0]; normalize to "sta".
  set usage [regsub {Usage: [^ ]+} $usage {Usage: sta}]
  set f [open $path w]
  puts $f "# Command line arguments"
  puts $f ""
  puts $f "The command line arguments for `sta` are shown below."
  puts $f ""
  puts $f "```"
  puts -nonewline $f $usage
  if { ![string match "*\n" $usage] } {
    puts $f ""
  }
  puts $f "```"
  puts $f ""
  puts $f "When OpenSTA starts up, commands are first read from the user"
  puts $f "initialization file `~/.sta` if it exists. If a Tcl command file"
  puts $f "`cmd_file` is specified on the command line, commands are read from"
  puts $f "the file and executed before entering an interactive Tcl command"
  puts $f "interpreter. If `-exit` is specified the application exits after"
  puts $f "reading `cmd_file`. Use the Tcl `exit` command to exit the"
  puts $f "application. The `-threads` option specifies how many parallel"
  puts $f "threads to use. Use `-threads max` to use one thread per processor."
  puts $f ""
  close $f
}

write_cmd_docs

# namespace end
}
