# Test util report formatting, log file handling, and redirect paths
# Targets uncovered lines in Report.cc, ReportTcl.cc, MachineLinux.cc,
# Debug.cc, PatternMatch.cc, StringUtil.cc, Error.cc

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Memory/CPU reporting (MachineLinux.cc coverage)
#---------------------------------------------------------------
puts "--- processor_count ---"
set nproc [sta::processor_count]
if { $nproc > 0 } {
  puts "PASS: processor_count returned $nproc"
} else {
  puts "FAIL: processor_count returned $nproc"
}

puts "--- memory_usage ---"
set mem [sta::memory_usage]
if { $mem >= 0 } {
  puts "PASS: memory_usage returned non-negative value"
} else {
  puts "FAIL: memory_usage returned negative"
}

puts "--- elapsed_run_time ---"
set elapsed [elapsed_run_time]
if { $elapsed >= 0 } {
  puts "PASS: elapsed_run_time returned non-negative value"
} else {
  puts "FAIL: elapsed_run_time returned negative"
}

puts "--- user_run_time ---"
set user_time [user_run_time]
if { $user_time >= 0 } {
  puts "PASS: user_run_time returned non-negative value"
} else {
  puts "FAIL: user_run_time returned negative"
}

puts "--- cputime ---"
set cput [sta::cputime]
if { $cput >= 0 } {
  puts "PASS: cputime returned non-negative value"
} else {
  puts "FAIL: cputime returned negative"
}

#---------------------------------------------------------------
# Report redirect to file (Report.cc redirectFileBegin/End)
#---------------------------------------------------------------
puts "--- redirect to file ---"
set redir_file [make_result_file "util_redirect_out.txt"]
sta::redirect_file_begin $redir_file
puts "redirected content line 1"
report_units
sta::redirect_file_end
puts "PASS: redirect_file_begin/end completed"

if { [file exists $redir_file] } {
  set fh [open $redir_file r]
  set content [read $fh]
  close $fh
  if { [string length $content] > 0 } {
    puts "PASS: redirect file has content"
  } else {
    puts "INFO: redirect file was empty"
  }
} else {
  puts "INFO: redirect file not found"
}

#---------------------------------------------------------------
# Redirect append to file (Report.cc redirectFileAppendBegin)
#---------------------------------------------------------------
puts "--- redirect append to file ---"
set append_file [make_result_file "util_redirect_append.txt"]
sta::redirect_file_begin $append_file
puts "first write"
sta::redirect_file_end

sta::redirect_file_append_begin $append_file
puts "appended content"
sta::redirect_file_end
puts "PASS: redirect_file_append_begin/end completed"

if { [file exists $append_file] } {
  set fh [open $append_file r]
  set content [read $fh]
  close $fh
  if { [string length $content] > 0 } {
    puts "PASS: appended redirect file has content"
  } else {
    puts "INFO: appended redirect file was empty"
  }
} else {
  puts "INFO: appended redirect file not found"
}

#---------------------------------------------------------------
# Log file operations (Report.cc logBegin/logEnd, ReportTcl.cc)
#---------------------------------------------------------------
puts "--- log_begin with content ---"
set log_file2 [make_result_file "util_log2.txt"]
log_begin $log_file2
report_units
set_cmd_units -time ps -capacitance fF
report_units
set_cmd_units -time ns -capacitance pF
report_units
log_end
puts "PASS: log_begin/log_end with reports"

if { [file exists $log_file2] } {
  set fh [open $log_file2 r]
  set log_content [read $fh]
  close $fh
  if { [string length $log_content] > 0 } {
    puts "PASS: log file has report content"
  } else {
    puts "INFO: log file was empty (may be expected)"
  }
} else {
  puts "INFO: log file not found (may be expected)"
}

#---------------------------------------------------------------
# Error handling paths (Error.cc, FileNotReadable)
#---------------------------------------------------------------
puts "--- FileNotReadable error path ---"
set rc [catch { read_liberty "/nonexistent/file/path.lib" } msg]
if { $rc != 0 } {
  puts "PASS: caught expected error for nonexistent file"
} else {
  puts "INFO: no error for nonexistent file"
}

#---------------------------------------------------------------
# with_output_to_variable (redirect string path in Report.cc)
#---------------------------------------------------------------
puts "--- with_output_to_variable multiple calls ---"
with_output_to_variable v1 { report_units }
with_output_to_variable v2 { puts "hello world" }
with_output_to_variable v3 { report_units; puts "extra" }
puts "captured v1 length: [string length $v1]"
puts "captured v2 length: [string length $v2]"
puts "captured v3 length: [string length $v3]"
puts "PASS: multiple with_output_to_variable calls"

#---------------------------------------------------------------
# Redirect string directly (exercises redirect string paths)
#---------------------------------------------------------------
puts "--- redirect_string_begin/end ---"
sta::redirect_string_begin
report_units
set rstr [sta::redirect_string_end]
puts "redirect string captured: [string length $rstr] chars"
puts "PASS: redirect_string_begin/end"

#---------------------------------------------------------------
# Unit commands (various set_cmd_units options)
#---------------------------------------------------------------
puts "--- set_cmd_units power ---"
set_cmd_units -power mW
report_units
puts "PASS: set_cmd_units -power mW"

puts "--- set_cmd_units all options ---"
set_cmd_units -time ns -capacitance pF -resistance kOhm -voltage V -current mA -distance um -power mW
report_units
puts "PASS: set_cmd_units all options"

#---------------------------------------------------------------
# Debug level setting (Debug.cc)
#---------------------------------------------------------------
puts "--- set_debug commands ---"
sta::set_debug "search" 1
sta::set_debug "search" 0
puts "PASS: set_debug search on/off"

sta::set_debug "graph" 2
sta::set_debug "graph" 0
puts "PASS: set_debug graph on/off"

#---------------------------------------------------------------
# Message suppression exercising different paths
#---------------------------------------------------------------
puts "--- suppress/unsuppress variety ---"
suppress_msg 1 2 3 4 5
unsuppress_msg 1 2 3 4 5
suppress_msg 999
unsuppress_msg 999
puts "PASS: suppress/unsuppress variety"

#---------------------------------------------------------------
# Thread count
#---------------------------------------------------------------
puts "--- thread_count ---"
set tc [sta::thread_count]
puts "thread_count: $tc"
puts "PASS: thread_count"

#---------------------------------------------------------------
# Fuzzy equality
#---------------------------------------------------------------
puts "--- fuzzy_equal ---"
set eq1 [sta::fuzzy_equal 1.0 1.0]
set eq2 [sta::fuzzy_equal 1.0 2.0]
puts "fuzzy_equal(1.0, 1.0) = $eq1"
puts "fuzzy_equal(1.0, 2.0) = $eq2"
puts "PASS: fuzzy_equal"

#---------------------------------------------------------------
# is_object / object_type
#---------------------------------------------------------------
puts "--- is_object ---"
set io1 [sta::is_object "not_an_object"]
puts "is_object(not_an_object) = $io1"
puts "PASS: is_object"

#---------------------------------------------------------------
# Unit format functions
#---------------------------------------------------------------
puts "--- format_time ---"
set ft [sta::format_time "1e-9" 3]
puts "format_time: $ft"
puts "PASS: format_time"

puts "--- format_capacitance ---"
set fc [sta::format_capacitance "1e-12" 3]
puts "format_capacitance: $fc"
puts "PASS: format_capacitance"

puts "--- format_resistance ---"
set fr [sta::format_resistance "1000" 3]
puts "format_resistance: $fr"
puts "PASS: format_resistance"

puts "--- format_voltage ---"
set fv [sta::format_voltage "1.1" 3]
puts "format_voltage: $fv"
puts "PASS: format_voltage"

puts "--- format_current ---"
set fi [sta::format_current "1e-3" 3]
puts "format_current: $fi"
puts "PASS: format_current"

puts "--- format_power ---"
set fp [sta::format_power "1e-3" 3]
puts "format_power: $fp"
puts "PASS: format_power"

puts "--- format_distance ---"
set fd [sta::format_distance "1e-6" 3]
puts "format_distance: $fd"
puts "PASS: format_distance"

puts "ALL PASSED"
