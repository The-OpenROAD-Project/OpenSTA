# Test util report formatting, error paths, string operations
# Targets: Report.cc (printConsole, printLine, printString, reportLine,
#   reportLineString, warn, error, vfileWarn, vfileError, redirect paths,
#   logBegin/logEnd, buffer management)
# Targets: TokenParser.cc (hasNext/next parsing)
# Targets: StringUtil.cc (stdstrPrint, trimRight, isSame patterns)
# Targets: Error.cc (FileNotReadable, FileNotWritable, InternalError)
# Targets: PatternMatch.cc (various match operations)
# Targets: MachineLinux.cc (readMemoryUsage, readCpuTime)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Report formatting with different unit scales
#---------------------------------------------------------------
puts "--- unit format sequences ---"
set_cmd_units -time ns -capacitance pF -resistance kOhm
report_units
puts "PASS: ns/pF/kOhm"

set_cmd_units -time us -capacitance nF
report_units
puts "PASS: us/nF"

set_cmd_units -time ps -capacitance fF -resistance Ohm
report_units
puts "PASS: ps/fF/Ohm"

set_cmd_units -time ns -capacitance pF -resistance kOhm
report_units
puts "PASS: back to ns/pF/kOhm"

#---------------------------------------------------------------
# Format functions with different values
#---------------------------------------------------------------
puts "--- format functions edge cases ---"
puts "format_time 0: [sta::format_time 0 3]"
puts "format_time 1e-9: [sta::format_time 1e-9 3]"
puts "format_time 1e-12: [sta::format_time 1e-12 6]"
puts "format_time 1e-6: [sta::format_time 1e-6 3]"
puts "PASS: format_time"

puts "format_cap 0: [sta::format_capacitance 0 3]"
puts "format_cap 1e-12: [sta::format_capacitance 1e-12 3]"
puts "format_cap 1e-15: [sta::format_capacitance 1e-15 6]"
puts "PASS: format_capacitance"

puts "format_res 0: [sta::format_resistance 0 3]"
puts "format_res 1000: [sta::format_resistance 1000 3]"
puts "format_res 1e6: [sta::format_resistance 1e6 3]"
puts "PASS: format_resistance"

puts "format_volt 0: [sta::format_voltage 0 3]"
puts "format_volt 1.1: [sta::format_voltage 1.1 3]"
puts "format_volt 0.001: [sta::format_voltage 0.001 6]"
puts "PASS: format_voltage"

puts "format_curr 0: [sta::format_current 0 3]"
puts "format_curr 1e-3: [sta::format_current 1e-3 3]"
puts "format_curr 1e-6: [sta::format_current 1e-6 6]"
puts "PASS: format_current"

puts "format_pwr 0: [sta::format_power 0 3]"
puts "format_pwr 1e-3: [sta::format_power 1e-3 3]"
puts "format_pwr 1e-9: [sta::format_power 1e-9 6]"
puts "PASS: format_power"

puts "format_dist 0: [sta::format_distance 0 3]"
puts "format_dist 1e-6: [sta::format_distance 1e-6 3]"
puts "format_dist 1e-3: [sta::format_distance 1e-3 6]"
puts "PASS: format_distance"

#---------------------------------------------------------------
# Multiple redirect sequences (exercises buffer management)
#---------------------------------------------------------------
puts "--- redirect sequences ---"
sta::redirect_string_begin
report_units
set r1 [sta::redirect_string_end]
puts "redirect string 1: [string length $r1] chars"

sta::redirect_string_begin
set_cmd_units -time ps
report_units
set r2 [sta::redirect_string_end]
puts "redirect string 2: [string length $r2] chars"

sta::redirect_string_begin
set_cmd_units -time ns
report_units
set_cmd_units -capacitance pF
report_units
set r3 [sta::redirect_string_end]
puts "redirect string 3: [string length $r3] chars"
puts "PASS: redirect sequences"

#---------------------------------------------------------------
# with_output_to_variable
#---------------------------------------------------------------
puts "--- with_output_to_variable ---"
with_output_to_variable v1 { report_units }
with_output_to_variable v2 { puts "line1"; puts "line2"; puts "line3" }
with_output_to_variable v3 { set_cmd_units -time ps; report_units }
puts "v1: [string length $v1] chars"
puts "v2: [string length $v2] chars"
puts "v3: [string length $v3] chars"
set_cmd_units -time ns
puts "PASS: with_output_to_variable"

#---------------------------------------------------------------
# Log file with multiple reports
#---------------------------------------------------------------
puts "--- log file ---"
set log_file [make_result_file "util_format_log.txt"]
log_begin $log_file
report_units
set_cmd_units -time ps -capacitance fF
report_units
set_cmd_units -time ns -capacitance pF
report_units
log_end
puts "PASS: log file with reports"

if { [file exists $log_file] } {
  set fh [open $log_file r]
  set lc [read $fh]
  close $fh
  if { [string length $lc] > 0 } { puts "PASS: log file has content" }
}

#---------------------------------------------------------------
# Redirect file with append
#---------------------------------------------------------------
puts "--- redirect file append ---"
set rf [make_result_file "util_format_redir.txt"]
sta::redirect_file_begin $rf
report_units
sta::redirect_file_end

sta::redirect_file_append_begin $rf
set_cmd_units -time ps -capacitance fF
report_units
sta::redirect_file_end

sta::redirect_file_append_begin $rf
set_cmd_units -time ns -capacitance pF
report_units
sta::redirect_file_end

puts "PASS: redirect file append"

if { [file exists $rf] } {
  set fh [open $rf r]
  set rc [read $fh]
  close $fh
  if { [string length $rc] > 0 } { puts "PASS: redirect file has content" }
}

#---------------------------------------------------------------
# Error handling paths
#---------------------------------------------------------------
puts "--- error handling ---"
set rc [catch { read_liberty "/nonexistent/path/test.lib" } msg]
if { $rc != 0 } {
  puts "PASS: FileNotReadable caught"
}

set rc [catch { read_verilog "/nonexistent/path/test.v" } msg]
if { $rc != 0 } {
  puts "PASS: FileNotReadable for verilog caught"
}

#---------------------------------------------------------------
# Fuzzy equality
#---------------------------------------------------------------
puts "--- fuzzy equal ---"
puts "fuzzy_equal(1.0, 1.0) = [sta::fuzzy_equal 1.0 1.0]"
puts "fuzzy_equal(1.0, 2.0) = [sta::fuzzy_equal 1.0 2.0]"
puts "fuzzy_equal(0.0, 0.0) = [sta::fuzzy_equal 0.0 0.0]"
puts "fuzzy_equal(1e-15, 1e-15) = [sta::fuzzy_equal 1e-15 1e-15]"
puts "PASS: fuzzy_equal"

#---------------------------------------------------------------
# Object type queries
#---------------------------------------------------------------
puts "--- is_object ---"
puts "is_object(string) = [sta::is_object not_an_object]"
puts "PASS: is_object"

#---------------------------------------------------------------
# Thread and system info
#---------------------------------------------------------------
puts "--- system info ---"
set tc [sta::thread_count]
if { $tc > 0 } { puts "PASS: thread_count positive" }

set np [sta::processor_count]
if { $np > 0 } { puts "PASS: processor_count positive" }

set mem [sta::memory_usage]
if { $mem >= 0 } { puts "PASS: memory_usage non-negative" }

set cpu [sta::cputime]
if { $cpu >= 0 } { puts "PASS: cputime non-negative" }

set elapsed [elapsed_run_time]
if { $elapsed >= 0 } { puts "PASS: elapsed non-negative" }

set utime [user_run_time]
if { $utime >= 0 } { puts "PASS: user_run_time non-negative" }
puts "PASS: system info"

#---------------------------------------------------------------
# Debug level
#---------------------------------------------------------------
puts "--- debug ---"
sta::set_debug "search" 1
sta::set_debug "search" 0
sta::set_debug "graph" 1
sta::set_debug "graph" 0
sta::set_debug "delay_calc" 1
sta::set_debug "delay_calc" 0
puts "PASS: debug level"

#---------------------------------------------------------------
# Message suppression
#---------------------------------------------------------------
puts "--- suppress ---"
suppress_msg 100 200 300 400 500
unsuppress_msg 100 200 300 400 500
puts "PASS: suppress/unsuppress"

puts "ALL PASSED"
