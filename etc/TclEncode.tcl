#! /bin/sh
# The next line is executed by /bin/sh, but not Tcl \
exec tclsh $0 ${1+"$@"}

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

# Usage: TclEncode encoded_filename var_name tcl_init_dir tcl_filename...
# Encode the contents of tcl_filenames into a C character array
# named var_name in the file encoded_filename.
# Each TCL file is encoded as a separate string of three digit decimal numbers
# that is unencoded and evaled on startup of the application.  
# The init variable character array is terminated with a NULL pointer.

set encoded_filename [lindex $argv 0]
set init_var [lindex $argv 1]
set init_filenames [lrange $argv 2 end]

# Microcruft Visual C-- ridiculously short max string constant length.
set max_string_length 2000

set out_stream [open $encoded_filename w]
puts $out_stream "// TCL init file encoded by TclEncode.tcl"
puts $out_stream "namespace sta {"
puts $out_stream "const char *$init_var\[\] = {"
puts -nonewline $out_stream "\""
set encoded_length 0

binary scan "\n" c newline_enc

proc encode_line { line } {
  global encoded_length max_string_length
  global newline_enc out_stream

  set length [string length $line]
  set i 0
  while { $i < $length } {
    set ch [string index $line $i]
    binary scan $ch c enc
    puts -nonewline $out_stream [format "%03d" $enc]
    incr i
    incr encoded_length 3
    if { $encoded_length > $max_string_length } {
      puts $out_stream "\","
      puts -nonewline $out_stream "\""
      set encoded_length 0
    }
  }
  puts -nonewline $out_stream [format "%03d" $newline_enc]
  incr encoded_length 3
}

proc encode_file { filename } {
  set in_stream [open $filename r]
  while {![eof $in_stream]} {
    gets $in_stream line
    encode_line $line
  }
  close $in_stream
}

foreach filename $init_filenames {
  encode_file $filename
}

puts $out_stream "\","
# NULL string to terminate char* array.
puts $out_stream "0"
puts $out_stream "};"
puts $out_stream "} // namespace"
close $out_stream
