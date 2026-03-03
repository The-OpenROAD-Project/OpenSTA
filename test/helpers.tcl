# Helper functions common to multiple regressions.

set test_dir [file dirname [file normalize [info script]]]
set result_dir [file join $test_dir "results"]

# puts [exec cat $file] without forking.
proc report_file { file } {
  set stream [open $file r]
  if { [file extension $file] == ".gz" } {
    zlib push gunzip $stream
  }
  gets $stream line
  while { ![eof $stream] } {
    puts $line
    gets $stream line
  }
  close $stream
}

proc report_file_filter { file filter } {
  set stream [open $file r]
  gets $stream line
  while { ![eof $stream] } {
    set index [string first $filter $line]
    if { $index != -1 } {
      set line [string replace $line $index [expr $index + [string length $filter] - 1]]
    }
    puts $line
    gets $stream line
  }
  close $stream
}

proc make_result_file { filename } {
  variable result_dir
  return [file join $result_dir $filename]
}

proc sort_objects { objects } {
  return [sta::sort_by_full_name $objects]
}