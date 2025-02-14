#!/bin/sh
# The next line is executed by /bin/sh, but not Tcl \
exec tclsh $0 ${1+"$@"}

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

# Find warning/error message IDs and detect collisions.
# Usage: FindMessages.tcl > doc/messages.txt

set has_error 0

proc scan_file { file warn_regexp } {
  global msgs
  global has_error

  if { [file exists $file] } {
    set in_stream [open $file r]
    gets $in_stream line
    set file_line 1

    while { ![eof $in_stream] } {
      if { [regexp -- $warn_regexp $line ignore msg_id msg] } {
        lappend msgs "$msg_id $file $file_line $msg"
      }
      gets $in_stream line
      incr file_line
    }
    close $in_stream
  } else {
    puts stderr "Warning: Source file $file not found during message scanning"
    set has_error 1
  }
}

set subdirs {app dcalc graph liberty network parasitics \
                 power sdc sdf search spice util verilog tcl}
set files_c {}
foreach subdir $subdirs {
    set files [glob -nocomplain [file join $subdir "*.{cc,hh,yy,ll,i}"]]
    set files_c [concat $files_c $files]
}
set warn_regexp_c {(?:(?:criticalError|->warn|->fileWarn|->error|->fileError|libWarn|libError| warn)\(|tclArgError\(interp,\s*)([0-9]+),.*(".+")}

set files_tcl {}
foreach subdir $subdirs {
  set files_tcl [concat $files_tcl [glob -nocomplain [file join $subdir "*.tcl"]]]
}
set warn_regexp_tcl {(?:sta_warn|sta_error|sta_warn_error) ([0-9]+) (".+")}

proc scan_files {files warn_regexp } {
  foreach file $files {
    scan_file $file $warn_regexp
  }
}

proc check_msgs { } {
  global msgs
  global has_error

  set msgs [lsort -index 0 -integer $msgs]
  set prev_id -1
  foreach msg $msgs {
    set msg_id [lindex $msg 0]
    if { $msg_id == $prev_id } {
      puts stderr "Warning: Message id $msg_id duplicated"
      set has_error 1
    }
    set prev_id $msg_id
  }
}

proc report_msgs { } {
  global msgs

  foreach msg_info $msgs {
    lassign $msg_info msg_id file line msg1
    puts "[format %04d $msg_id] [format %-25s [file tail $file]:$line] $msg1"
  }
}

set msgs {}
scan_files $files_c $warn_regexp_c
scan_files $files_tcl $warn_regexp_tcl
check_msgs
report_msgs

if {$has_error} {
  exit 1
}
