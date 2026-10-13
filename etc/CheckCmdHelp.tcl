#!/bin/sh
# The next line is executed by /bin/sh, but not Tcl \
exec sta -no_splash -no_init -exit $0 ${1+"$@"}

# OpenSTA, Static Timing Analyzer
# Copyright (c) 2026, Parallax Software, Inc.
#
# Hidden commands may have a synopsis but no -help prose, so they are
# omitted from the generated command reference. Exit 1 if any other
# command is missing help.
# Usage: sta -no_splash -no_init -exit etc/CheckCmdHelp.tcl

namespace eval sta {

# Exported commands that are intentionally undocumented (internal or
# specialist). They are skipped by WriteCmdDocs.tcl.
variable hidden_cmds {
  report_constant
  report_path
  set_ideal_net
  show_copying
  show_splash
  show_warranty
  write_gate_gnuplot
  write_gate_spice
}

proc check_cmd_help {} {
  variable cmd_args
  variable cmd_help
  variable hidden_cmds

  set missing {}
  foreach cmd [lsort [array names cmd_args]] {
    if { ![info exists cmd_help($cmd)] || $cmd_help($cmd) == "" } {
      lappend missing $cmd
    }
  }

  set unexpected {}
  set hidden {}
  foreach cmd $missing {
    if { [lsearch -exact $hidden_cmds $cmd] >= 0 } {
      lappend hidden $cmd
    } else {
      lappend unexpected $cmd
    }
  }

  puts "Hidden commands: [llength $hidden]"
  foreach cmd $hidden {
    puts "  $cmd"
  }

  if { $unexpected != {} } {
    puts stderr "Commands missing help that are not hidden:"
    foreach cmd $unexpected {
      puts stderr "  $cmd"
    }
    exit 1
  }
}

proc check_cmd_arg_help {} {
  variable cmd_args
  variable cmd_help

  set missing_pairs {}
  foreach cmd [lsort [array names cmd_args]] {
    if { ![info exists cmd_help($cmd)] || $cmd_help($cmd) == "" } {
      continue
    }
    foreach opt [cmd_synopsis_options $cmd_args($cmd)] {
      if { [cmd_arg_help_text $cmd $opt] == "" } {
        lappend missing_pairs [list $cmd $opt]
      }
    }
  }

  if { $missing_pairs != {} } {
    puts stderr "Documented commands with undocumented options:"
    foreach pair $missing_pairs {
      lassign $pair cmd opt
      puts stderr "  $cmd $opt"
    }
    exit 1
  }
}

check_cmd_help
check_cmd_arg_help

# namespace end
}
