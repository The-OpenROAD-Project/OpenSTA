# Helper functions common to multiple regressions.

set test_dir [file dirname [file normalize [info script]]]
set result_dir [file join [pwd] "results"]

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
  global result_dir
  if { ![file exists $result_dir] } {
    file mkdir $result_dir
  }
  return [file join $result_dir $filename]
}

proc sort_objects { objects } {
  return [sta::sort_by_full_name $objects]
}

proc diff_files_sorted { file1 file2 } {
  set stream1 [open $file1 r]
  set stream2 [open $file2 r]
  set lines1 [lsort [split [read $stream1] "\n"]]
  set lines2 [lsort [split [read $stream2] "\n"]]
  close $stream1
  close $stream2
  if { $lines1 eq $lines2 } {
    puts "No differences found."
    return 0
  } else {
    for {set i 0} {$i < [llength $lines1] && $i < [llength $lines2]} {incr i} {
      if { [lindex $lines1 $i] ne [lindex $lines2 $i] } {
        error "diff_files_sorted: $file1 vs $file2 differ at sorted line $i\n< [lindex $lines1 $i]\n> [lindex $lines2 $i]"
      }
    }
    error "diff_files_sorted: $file1 vs $file2 differ: file lengths differ"
  }
}

proc assert_file_nonempty { path } {
  if { ![file exists $path] || [file size $path] <= 0 } {
    error "expected non-empty file: $path"
  }
}

proc assert_file_contains { path token } {
  set in [open $path r]
  set text [read $in]
  close $in
  if { [string first $token $text] < 0 } {
    error "expected '$token' in $path"
  }
}

proc diff_files { file1 file2 { ignore "" } } {
  set stream1 [open $file1 r]
  set stream2 [open $file2 r]

  set skip false
  set line 1
  set found_diff 0
  set line1_length [gets $stream1 line1]
  set line2_length [gets $stream2 line2]
  while { $line1_length >= 0 && $line2_length >= 0 } {
    if { $ignore ne "" } {
      set skip [expr {[regexp $ignore $line1] || [regexp $ignore $line2]}]
    }
    if { !$skip && $line1 != $line2 } {
      set found_diff 1
      break
    }
    incr line
    set line1_length [gets $stream1 line1]
    set line2_length [gets $stream2 line2]
  }
  close $stream1
  close $stream2
  if { $found_diff || $line1_length != $line2_length } {
    error "diff_files: $file1 vs $file2 differ at line $line\n< $line1\n> $line2"
  } else {
    puts "No differences found."
    return 0
  }
}
