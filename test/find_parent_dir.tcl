#! /bin/sh
# The next line is executed by /bin/sh, but not Tcl \
exec tclsh $0 ${1+"$@"}

# Parallax Static Timing Analyzer
# Copyright (c) 2019, Parallax Software, Inc.
# All rights reserved.
# 
# No part of this document may be copied, transmitted or
# disclosed in any form or fashion without the express
# written consent of Parallax Software, Inc.

proc find_parent_dir { dir } {
  if { $dir == "." } {
    return ".."
  } else {
    set path [file split $dir]
    set path_len [llength $path]
    if { $path_len == 1 } {
      return "."
    } else {
      set path_len2 [expr $path_len - 2]
      return [eval file join [lrange $path 0 $path_len2]]
    }
  }
}
