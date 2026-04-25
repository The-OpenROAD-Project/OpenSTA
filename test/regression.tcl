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
# 
# The origin of this software must not be misrepresented; you must not
# claim that you wrote the original software.
# 
# Altered source versions must be plainly marked as such, and must not be
# misrepresented as being the original software.
# 
# This notice may not be removed or altered from any source distribution.

# Usage: regression -help | [-j jobs] [-threads threads] [-valgrind] [-report_stats]
#                   test1 [test2...]

proc regression_main {} {
  setup
  parse_args
  run_tests
  show_summary
  exit [found_errors]
}

proc setup {} {
  global result_dir diff_file failure_file errors failed_tests
  global use_valgrind valgrind_shared_lib_failure
  global report_stats max_jobs app_path

  set use_valgrind 0
  set report_stats 0
  set max_jobs 1

  if { !([file exists $result_dir] && [file isdirectory $result_dir]) } {
    file mkdir $result_dir
  }
  file delete $diff_file
  file delete $failure_file

  set errors(error) 0
  set errors(memory) 0
  set errors(leak) 0
  set errors(fail) 0
  set errors(no_cmd) 0
  set errors(no_ok) 0
  set failed_tests {}
  set valgrind_shared_lib_failure 0

  if { ![file exists $app_path] } {
    error "$app_path not found."
  } elseif { ![file executable $app_path] } {
    error "$app_path is not executable."
  }
}

proc parse_args {} {
  global argv app_options tests test_groups cmd_paths
  global use_valgrind
  global result_dir tests
  global report_stats max_jobs

  while { $argv != {} } {
    set arg [lindex $argv 0]
    if { $arg == "help" || $arg == "-help" } {
      puts {Usage: regression [-help] [-threads threads] [-j jobs] [-valgrind] [-report_stats] tests...}
      puts "  -j jobs - number of parallel test jobs (processes) to run"
      puts "  -threads max|integer - number of threads the STA uses"
      puts "  -valgrind - run valgrind (linux memory checker)"
      puts "  -report_stats - report run time and memory"
      puts "  Wildcarding for test names is supported (enclose in \"'s)"
      puts "  Tests are: all, fast, med, slow, or a test group or test name"
      puts ""
      puts "  If 'limit coredumpsize unlimited' corefiles are saved in $result_dir/test.core"
      exit
    } elseif { $arg == "-threads" } {
      set threads [lindex $argv 1]
      if { !([string is integer $threads] || $threads == "max") } {
	puts "Error: -threads arg $threads is not an integer or max."
	exit 0
      }
      lappend app_options "-threads"
      lappend app_options $threads
      set argv [lrange $argv 2 end]
    } elseif { $arg == "-j" } {
      set jobs [lindex $argv 1]
      if { ![string is integer $jobs] || $jobs < 1 } {
        puts "Error: -j arg $jobs must be a positive integer."
        exit 0
      }
      set max_jobs $jobs
      set argv [lrange $argv 2 end]
    } elseif { $arg == "-valgrind" } {
      if { ![find_valgrind] } {
        error "valgrind not found."
      }
      set use_valgrind 1
      set argv [lrange $argv 1 end]
    } elseif { $arg == "-report_stats" } {
      set report_stats 1
      set argv [lrange $argv 1 end]
    } else {
      break
    }
  }
  if { $argv == {} } {
    # Default is to run fast tests.
    set tests [group_tests fast]
  } else {
    set tests [expand_tests $argv]
  }
}

# Find valgrind in $PATH.
proc find_valgrind {} {
  global env

  foreach dir [regsub -all ":" $env(PATH) " "] {
    if { [file executable [file join $dir "valgrind"]] } {
      return 1
    }
  }
  return 0
}

proc expand_tests { argv } {
  global test_groups errors

  set tests {}
  foreach arg $argv {
    if { [info exists test_groups($arg)] } {
      set tests [concat $tests $test_groups($arg)]
    } elseif { [string first "*" $arg] != -1 \
                 || [string first "?" $arg] != -1 } {
      # Find wildcard matches.
      foreach test [group_tests "all"] {
        if [string match $arg $test] {
          lappend tests $test
        }
      }
    } else {
      lappend tests $arg
    }
  }
  return $tests
}

proc run_tests {} {
  global tests errors app_path max_jobs
  
  if { $max_jobs > 1 } {
    run_tests_parallel
  } else {
    foreach test $tests {
      run_test $test
    }
  }
}

proc run_test { test } {
  global result_dir diff_file errors diff_options failed_tests
  
  puts -nonewline $test
  flush stdout
  set exit_code 0
  if { [test_cmd_file_exists $test] } {
    set cmd [make_cmd_file $test]
    set log_file [test_log_file $test]
    if { [catch [concat "exec" "$cmd >& $log_file"] result result_options] } {
      set details [dict get $result_options -errorcode]
      set exit_signal [lindex $details 2]
      if { $exit_signal == "SIGSEGV" } {
        set exit_code 139
      } else {
        set exit_code 128
      }
    }
  }
  puts " [test_status $test $exit_code]"
}

################################################################

# Parallel runs use one pipeline per test; close() yields the real exit status.
# (Non-blocking channels must be switched to blocking before close - see Tcl manual.)
proc regression_parallel_close_pipe { fh } {
  fconfigure $fh -blocking 1
  if { [catch {close $fh} err opts] } {
    set ec [dict get $opts -errorcode]
    if { [lindex $ec 0] == "CHILDSTATUS" } {
      return [lindex $ec 2]
    }
    return 128
  }
  return 0
}

proc regression_pipe_readable { fh test } {
  global reg_parallel_active reg_parallel_job_done

  read $fh
  if { [eof $fh] } {
    fileevent $fh readable {}
    set exit_code [regression_parallel_close_pipe $fh]
    puts "$test [test_status $test $exit_code]"
    incr reg_parallel_active -1
    incr reg_parallel_job_done
  }
}

proc open_test_pipeline { test } {
  set cmd [make_cmd_file $test]
  set log [test_log_file $test]
  set inner [format {%s > %s 2>&1} $cmd [file nativename $log]]
  set fh [open [format {|/bin/sh -c %s} [list $inner]] r]
  fconfigure $fh -blocking 0
  return $fh
}

proc run_tests_parallel {} {
  global tests max_jobs reg_parallel_active reg_parallel_job_done

  set reg_parallel_active 0
  set reg_parallel_job_done 0
  set test_idx 0
  set test_count [llength $tests]

  while { $test_idx < $test_count || $reg_parallel_active > 0 } {
    while { $reg_parallel_active < $max_jobs && $test_idx < $test_count } {
      set test [lindex $tests $test_idx]
      incr test_idx
      if { ![test_cmd_file_exists $test] } {
        puts -nonewline $test
        flush stdout
        puts " [test_status $test 0]"
        continue
      }
      set fh [open_test_pipeline $test]
      fileevent $fh readable [list regression_pipe_readable $fh $test]
      incr reg_parallel_active
    }
    if { $reg_parallel_active > 0 } {
      set before $reg_parallel_job_done
      while { $reg_parallel_job_done == $before } {
        vwait reg_parallel_job_done
      }
    }
  }
}

proc make_cmd_file { test } {
  global app_path app_options result_dir use_valgrind report_stats

  foreach file [glob -nocomplain [file join $result_dir $test.*]] {
    file delete -force $file
  }

  set cmd_file [test_cmd_file $test]
  set ok_file [test_ok_file $test]
  set log_file [test_log_file $test]

  set run_file [test_run_file $test]
  set run_stream [open $run_file "w"]
  puts $run_stream "cd [file dirname $cmd_file]"
  puts $run_stream "include [file tail $cmd_file]"
  if { $use_valgrind } {
    puts $run_stream "sta::delete_all_memory"
  }
  if { $report_stats } {
    puts $run_stream "sta::write_stats [test_stats_file $test]"
  }
  close $run_stream

  if { $use_valgrind } {
    global valgrind_options
    set cmd "valgrind $valgrind_options $app_path $app_options $run_file"
  } else {
    set cmd "$app_path $app_options $run_file"
  }
  return $cmd
}

proc test_cmd_file_exists { test } {
  set cmd_file [test_cmd_file $test]
  return [file exists $cmd_file]
}

proc test_status { test exit_code } {
  global result_dir diff_options errors
  global use_valgrind report_stats test_status

  set test_status {}
  if { ![test_cmd_file_exists $test] } {
    test_failed $test "no_cmd"
  } else {
    set log_file [test_log_file $test]

    if { [file exists $log_file] } {
      # Check log file for error patterns
      set log_ch [open $log_file "r"]
      set log_content [read $log_ch]
      close $log_ch

      # Check if exit code indicates a segfault or signal termination
      # Exit codes >= 128 typically indicate termination by a signal
      # 139 = 128 + 11 (SIGSEGV), 134 = 128 + 6 (SIGABRT), etc.
      if { $exit_code >= 128
           || [string match "*Segmentation fault*" $log_content] \
             || [string match "*DEADLYSIGNAL*" $log_content] \
             || [string match "*Abort*" $log_content] \
             || [string match "*Fatal*" $log_content] } {
        test_failed $test "error"
      } elseif { [string match "*heap-use-after-free*" $log_content] } {
        # ASAN error
        test_failed $test "memory"
      }
      set ok_file [test_ok_file $test]
      if { [file exists $ok_file] } {
        if { $use_valgrind } {
          cleanse_valgrind_logfile $test
        }
        if { [catch [concat exec diff $diff_options $ok_file $log_file]] } {
          if { $test_status == "" } {
            test_failed $test "fail"
          }
        }
      } else {
        if { $test_status == "" } {
          test_failed $test "no_ok"
        }
      }
    } else {
      # Log file doesn't exist, likely an error
      test_failed $test "error" "*ERROR* no log file"
    }
  }
  if { $test_status == {} } {
    append test_status "pass"
  }
  if { $report_stats } {
    append test_status " [test_stats_summary $test]"
  }
  return $test_status
}

proc test_exit_code { test } {
  # Read exit code
  set test_error ""
  set exit_code_file [test_exit_code_file $test]
  if { [file exists $exit_code_file] } {
    set exit_code_ch [open $exit_code_file "r"]
    set exit_code [string trim [read $exit_code_ch]]
    close $exit_code_ch

    if { [string is integer $exit_code] } {
      return $exit_code
    }
  }
  return 0
}

proc test_stats_summary { test } {
  if { ![catch {open [test_stats_file $test] r} stream] } {
    gets $stream stats
    close $stream

    set elapsed_time [lindex $stats 0]
    set user_time [lindex $stats 1]
    set memory [lindex $stats 2]
    if { [string is double $elapsed_time] } {
      set elapsed [format "%.1fe" $elapsed_time]
    } else {
      set elapsed "?"
    }
    if { [string is double $user_time] } {
      set user [format "%.1fu" $user_time]
    } else {
      set user "?"
    }
    if { [string is double $memory] } {
      set mem [format "%.0fmb" [expr $memory * 1e-6]]
    } else {
      set mem "?"
    }
    return "$elapsed $user $mem"
  } else {
    return ""
  }
}

proc test_failed { test reason } {
  global errors test_status failed_tests
  
  if { $reason == "error" } {
    set test_status "*ERROR*"
  } elseif { $reason == "no_cmd" } {
    set test_status "*NO CMD FILE*"
  } elseif { $reason == "memory" } {
    set test_status "*MEMORY*"
  } elseif { $reason == "leak" } {
    set test_status "*LEAK*"
  } elseif { $reason == "fail" } {
    set test_status "*FAIL*"
  } elseif { $reason == "no_ok" } {
    set test_status "*NO OK FILE*"
  } else {
    error "unknown test failure reason $reason"
  }
  lappend failed_tests $test
  incr errors($reason)
  append_diff_file $test
}

proc append_diff_file { test } {
  global failure_file
  global diff_file diff_options

  set fail_ch [open $failure_file "a"]
  puts $fail_ch $test
  close $fail_ch
    
  # Append diff to results/diffs
  set log_file [test_log_file $test]
  set ok_file [test_ok_file $test]
  catch [concat exec diff $diff_options $ok_file $log_file >> $diff_file]
}

# Error messages can be found in "valgrind/memcheck/mc_errcontext.c".
#
# "Conditional jump or move depends on uninitialised value(s)"
# "%s contains unaddressable byte(s)"
# "%s contains uninitialised or unaddressable byte(s)"
# "Use of uninitialised value of size %d"
# "Invalid read of size %d"
# "Syscall param %s contains uninitialised or unaddressable byte(s)"
# "Unaddressable byte(s) found during client check request"
# "Uninitialised or unaddressable byte(s) found during client check request"
# "Invalid free() / delete / delete[]"
# "Mismatched free() / delete / delete []"
set valgrind_mem_regexp "(depends on uninitialised value)|(contains unaddressable)|(contains uninitialised)|(Use of uninitialised value)|(Invalid read)|(Unaddressable byte)|(Uninitialised or unaddressable)|(Invalid free)|(Mismatched free)"

# "%d bytes in %d blocks are definitely lost in loss record %d of %d"
# "%d bytes in %d blocks are possibly lost in loss record %d of %d"
#set valgrind_leak_regexp "blocks are (possibly|definitely) lost"
set valgrind_leak_regexp "blocks are definitely lost"

# Valgrind fails on executables using shared libraries.
set valgrind_shared_lib_failure_regexp "No malloc'd blocks -- no leaks are possible"

# Scan the log file to separate valgrind notifications and check for
# valgrind errors.
proc cleanse_valgrind_logfile { test } {
  global valgrind_mem_regexp valgrind_leak_regexp
  global valgrind_shared_lib_failure_regexp
  global valgrind_shared_lib_failure error
  
  set log_file [test_log_file $test]
  set tmp_file [test_tmp_file $test]
  set valgrind_log_file [test_valgrind_file $test]
  file copy -force $log_file $tmp_file
  set tmp [open $tmp_file "r"]
  set log [open $log_file "w"]
  set valgrind [open $valgrind_log_file "w"]
  set leak 0
  set mem_errors 0
  gets $tmp line
  while { ![eof $tmp] } {
    if {[regexp "^==" $line]} {
      puts $valgrind $line
      if {[regexp $valgrind_leak_regexp $line]} {
        set leak 1
      }
      if {[regexp $valgrind_mem_regexp $line]} {
        set mem_errors 1
      }
      if {[regexp $valgrind_shared_lib_failure_regexp $line]} {
        set valgrind_shared_lib_failure 1
      }
    } elseif {[regexp {^--[0-9]+} $line]} {
      # Valgrind notification line.
    } else {
      puts $log $line
    }
    gets $tmp line
  }
  close $log
  close $tmp
  close $valgrind
  
  if { $mem_errors } {
    test_failed $test "memory"
  } elseif { $leak } {
    test_failed $test "leak"
  } 
}

################################################################

proc show_summary {} {
  global errors tests diff_file result_dir valgrind_shared_lib_failure
  global app_path app
  
  puts "------------------------------------------------------"
  if { $valgrind_shared_lib_failure } {
    puts "WARNING: valgrind failed because the executable is not statically linked."
  }
  puts "See $result_dir for log files"
  set test_count [llength $tests]
  if { [found_errors] } {
    if { $errors(error) != 0 } {
      puts "Errored $errors(error)/$test_count"
    }
    if { $errors(fail) != 0 } {
      puts "Failed $errors(fail)/$test_count"
    }
    if { $errors(leak) != 0 } {
      puts "Memory leaks in $errors(leak)/$test_count"
    }
    if { $errors(memory) != 0 } {
      puts "Memory corruption in $errors(memory)/$test_count"
    }
    if { $errors(no_ok) != 0 } {
      puts "No ok file for $errors(no_ok)/$test_count"
    }
    if { $errors(no_cmd) != 0 } {
      puts "No cmd tcl file for $errors(no_cmd)/$test_count"
    }
    if { $errors(fail) != 0 } {
      puts "See $diff_file for differences"
    }
  } else {
    puts "Passed $test_count"
  }
}

proc found_errors {} {
  global errors
  
  return [expr $errors(error) != 0 || $errors(fail) != 0 \
            || $errors(no_cmd) != 0 || $errors(no_ok) != 0 \
            || $errors(memory) != 0 || $errors(leak) != 0]
}

################################################################

proc save_ok_main {} {
  global argv
  if { $argv == "help" || $argv == "-help" } {
    puts {Usage: save_ok [failures] test1 [test2]...}
  } elseif { $argv == "failures" } {
    global failure_file
    if [file exists $failure_file] {
      set fail_ch [open $failure_file "r"]
      while { ! [eof $fail_ch] } {
        set test [gets $fail_ch]
        if { $test != "" } {
          save_ok $test
        }
      }
      close $fail_ch
    }
  } else {
    foreach test $argv {
      if { [lsearch [group_tests "all"] $test] == -1 } {
        puts "Error: test $test not found."
      } else {
        save_ok $test
      }
    }
  }
}

# hook for pvt/public sync.
proc save_ok { test } {
  save_ok_file $test
}

proc save_ok_file { test } {
  set ok_file [test_ok_file $test]
  set log_file [test_log_file $test]
  if { ! [file exists $log_file] } {
    puts "Error: log file $log_file not found."
  } else {
    file copy -force $log_file $ok_file
  }
}

################################################################

proc test_cmd_dir { test } {
  global cmd_dirs
  
  if {[info exists cmd_dirs($test)]} {
    return $cmd_dirs($test)
  } else {
    return ""
  }
}

proc test_cmd_file { test } {
  return [file join [test_cmd_dir $test] "$test.tcl"]
}

proc test_ok_file { test } {
  global test_dir
  return [file join $test_dir "$test.ok"]
}

proc test_log_file { test } {
  global result_dir
  return [file join $result_dir "$test.log"]
}

proc test_run_file { test } {
  global result_dir
  return [file join $result_dir $test.run]
}

proc test_tmp_file { test } {
  global result_dir
  return [file join $result_dir $test.tmp]
}

proc test_valgrind_file { test } {
  global result_dir
  return [file join $result_dir $test.valgrind]
}

proc test_stats_file { test } {
  global result_dir
  return [file join $result_dir "$test.stats"]
}

proc test_core_file { test } {
  global result_dir
  return [file join $result_dir $test.core]
}

proc test_sys_core_file { test pid } {
  global cmd_dirs
  
  # macos
  # return [file join "/cores" "core.$pid"]
  
  # Suse
  return [file join [test_cmd_dir $test] "core"]
}

proc test_exit_code_file { test } {
  global result_dir
  return [file join $result_dir "$test.exitcode"]
}

################################################################

# Local Variables:
# mode:tcl
# End:
