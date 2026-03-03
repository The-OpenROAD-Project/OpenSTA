# Shared test helpers for module Tcl tests.
# Modeled after OpenROAD/test/helpers.tcl.
# CWD is set to CMAKE_CURRENT_SOURCE_DIR by ctest.
set result_dir [file join [pwd] "results"]

proc make_result_file { filename } {
  global result_dir
  if { ![file exists $result_dir] } {
    file mkdir $result_dir
  }
  return [file join $result_dir $filename]
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
        puts "Differences found (sorted)."
        puts "[lindex $lines1 $i]"
        puts "[lindex $lines2 $i]"
        return 1
      }
    }
    puts "Differences found (sorted): file lengths differ."
    return 1
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
    puts "Differences found at line $line."
    puts "$line1"
    puts "$line2"
    return 1
  } else {
    puts "No differences found."
    return 0
  }
}
