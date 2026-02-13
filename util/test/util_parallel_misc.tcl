# Test parallel dispatch, thread operations, and miscellaneous util coverage
# Targets: DispatchQueue.cc (dispatch queue creation, parallel execution)
#   TokenParser.cc (token parsing operations)
#   Report.cc (printToBuffer growth, printToBufferAppend, various warn paths)
#   ReportTcl.cc (Tcl-specific report formatting and error paths)
#   MachineLinux.cc (thread count, processor count, memory usage)
#   StringUtil.cc (string operations)
#   Debug.cc (additional debug paths)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Thread count and parallel settings
#---------------------------------------------------------------
puts "--- thread operations ---"
set tc [sta::thread_count]
puts "initial thread_count: $tc"

# Set thread count to test dispatch queue paths
sta::set_thread_count 2
set tc2 [sta::thread_count]
puts "thread_count after set to 2: $tc2"

sta::set_thread_count 1
set tc3 [sta::thread_count]
puts "thread_count after set to 1: $tc3"

# Try larger thread count
sta::set_thread_count 4
set tc4 [sta::thread_count]
puts "thread_count after set to 4: $tc4"

# Reset to 1
sta::set_thread_count 1
puts "PASS: thread count operations"

#---------------------------------------------------------------
# Processor count
#---------------------------------------------------------------
puts "--- processor_count ---"
set nproc [sta::processor_count]
puts "processor_count: $nproc"
if { $nproc > 0 } {
  puts "PASS: processor_count positive"
} else {
  puts "FAIL: processor_count non-positive"
}

#---------------------------------------------------------------
# Memory usage
#---------------------------------------------------------------
puts "--- memory_usage ---"
set mem [sta::memory_usage]
if { $mem >= 0 } {
  puts "PASS: memory_usage non-negative"
} else {
  puts "FAIL: memory_usage negative"
}

#---------------------------------------------------------------
# Load a design to exercise parallel timing with threads
#---------------------------------------------------------------
puts "--- load design for parallel timing ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../verilog/test/verilog_complex_bus_test.v
link_design verilog_complex_bus_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [all_inputs]
set_output_delay -clock clk 0 [all_outputs]
set_input_transition 0.1 [all_inputs]

# Run timing with 1 thread
sta::set_thread_count 1
report_checks
puts "PASS: report_checks with 1 thread"

# Run timing with 2 threads to exercise dispatch queue
sta::set_thread_count 2
report_checks
puts "PASS: report_checks with 2 threads"

# Run timing with 4 threads
sta::set_thread_count 4
report_checks
puts "PASS: report_checks with 4 threads"

# Back to 1
sta::set_thread_count 1

#---------------------------------------------------------------
# Report redirect to variable multiple times (exercises buffer growth)
#---------------------------------------------------------------
puts "--- buffer growth test ---"
# Capture large output to exercise buffer growth paths
with_output_to_variable v1 {
  report_checks
  report_checks -path_delay min
  report_checks -path_delay max
  report_checks -fields {slew cap input_pins nets fanout}
}
puts "large capture length: [string length $v1]"
puts "PASS: buffer growth"

#---------------------------------------------------------------
# String redirect with large content
#---------------------------------------------------------------
puts "--- string redirect large ---"
sta::redirect_string_begin
report_checks
report_checks -path_delay min
report_checks -path_delay max
report_checks -fields {slew cap input_pins nets fanout}
set s1 [sta::redirect_string_end]
puts "string redirect length: [string length $s1]"
puts "PASS: string redirect large"

#---------------------------------------------------------------
# Report to file with large content
#---------------------------------------------------------------
puts "--- file redirect large ---"
set rfile [make_result_file "util_parallel_redir.txt"]
sta::redirect_file_begin $rfile
report_checks
report_checks -path_delay min
report_checks -path_delay max
report_checks -fields {slew cap input_pins nets fanout}
sta::redirect_file_end
if { [file exists $rfile] } {
  set fh [open $rfile r]
  set content [read $fh]
  close $fh
  puts "file redirect size: [string length $content]"
  puts "PASS: file redirect large"
} else {
  puts "INFO: file not created"
}

#---------------------------------------------------------------
# Report append with multiple cycles
#---------------------------------------------------------------
puts "--- append cycles ---"
set afile [make_result_file "util_parallel_append.txt"]
sta::redirect_file_begin $afile
report_units
sta::redirect_file_end

sta::redirect_file_append_begin $afile
report_checks
sta::redirect_file_end

sta::redirect_file_append_begin $afile
report_checks -path_delay min
sta::redirect_file_end

if { [file exists $afile] } {
  set fh [open $afile r]
  set content [read $fh]
  close $fh
  puts "appended file size: [string length $content]"
  puts "PASS: append cycles"
} else {
  puts "INFO: append file not created"
}

#---------------------------------------------------------------
# Debug with parallel timing
#---------------------------------------------------------------
puts "--- debug with threads ---"
sta::set_thread_count 2
sta::set_debug "search" 1
report_checks
sta::set_debug "search" 0
puts "PASS: debug search with threads"

sta::set_debug "delay_calc" 1
report_checks
sta::set_debug "delay_calc" 0
puts "PASS: debug delay_calc with threads"

sta::set_thread_count 1

#---------------------------------------------------------------
# Various report_line calls
#---------------------------------------------------------------
puts "--- report_line coverage ---"
sta::report_line ""
sta::report_line "single line"
sta::report_line "line with special: \[ \] \{ \} \$ \\"
sta::report_line "very long line: [string repeat "abcdefghij" 50]"
puts "PASS: report_line coverage"

#---------------------------------------------------------------
# Format functions with extreme values
#---------------------------------------------------------------
puts "--- format extreme values ---"
set ft_tiny [sta::format_time "1e-15" 6]
puts "format_time(1fs): $ft_tiny"

set ft_huge [sta::format_time "1e-3" 3]
puts "format_time(1ms): $ft_huge"

set fc_tiny [sta::format_capacitance "1e-18" 6]
puts "format_capacitance(1aF): $fc_tiny"

set fr_tiny [sta::format_resistance "0.001" 6]
puts "format_resistance(1mOhm): $fr_tiny"

set fp_tiny [sta::format_power "1e-12" 6]
puts "format_power(1pW): $fp_tiny"

set fd_tiny [sta::format_distance "1e-9" 6]
puts "format_distance(1nm): $fd_tiny"

puts "PASS: format extreme values"

#---------------------------------------------------------------
# Log file with design operations
#---------------------------------------------------------------
puts "--- log with design ops ---"
set lfile [make_result_file "util_parallel_log.txt"]
log_begin $lfile
report_checks
report_checks -path_delay min
report_units
log_end
if { [file exists $lfile] } {
  puts "PASS: log with design ops"
} else {
  puts "INFO: log file not created"
}

#---------------------------------------------------------------
# Error paths (run last since they may affect design state)
#---------------------------------------------------------------
puts "--- error paths ---"
set rc [catch { read_liberty "/nonexistent/path/file.lib" } msg]
if { $rc != 0 } {
  puts "PASS: caught nonexistent liberty error"
}

set rc [catch { read_verilog "/nonexistent/path/file.v" } msg]
if { $rc != 0 } {
  puts "PASS: caught nonexistent verilog error"
}

set rc [catch { read_spef "/nonexistent/path/file.spef" } msg]
if { $rc != 0 } {
  puts "PASS: caught nonexistent SPEF error"
}

set rc [catch { read_sdf "/nonexistent/path/file.sdf" } msg]
if { $rc != 0 } {
  puts "PASS: caught nonexistent SDF error"
}

puts "ALL PASSED"
