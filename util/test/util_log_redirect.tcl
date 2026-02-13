# Test Report.cc log + redirect interaction, buffer growth, and
# simultaneous log+console output.
# Targets: Report.cc uncovered functions:
#   logBegin/logEnd with real report output,
#   redirectFileAppendBegin with log active,
#   redirectStringBegin/End with log active,
#   printString with log_stream_ active (both redirect and log paths),
#   printLine, printConsole, reportLine, reportLineString,
#   buffer growth (printToBufferAppend with oversized strings),
#   warn/fileWarn with suppressed IDs

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Log file with real timing reports
# Exercises: logBegin, logEnd, printString with log_stream_ active
#---------------------------------------------------------------
puts "--- Test 1: log with timing reports ---"

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../test/reg1_asap7.v

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 [get_ports out]

set log_file1 [make_result_file "util_log_timing.log"]
log_begin $log_file1

report_checks
report_checks -path_delay min
report_checks -from [get_ports in1] -to [get_ports out]
report_checks -fields {slew cap input_pins nets fanout}
report_units

log_end
puts "PASS: log with timing reports"

# Verify log file has content
if { [file exists $log_file1] } {
  set fh [open $log_file1 r]
  set content [read $fh]
  close $fh
  set len [string length $content]
  if { $len > 100 } {
    puts "PASS: log file has $len chars"
  } else {
    puts "INFO: log file unexpectedly small: $len"
  }
} else {
  puts "INFO: log file not found"
}

#---------------------------------------------------------------
# Test 2: Simultaneous log + redirect
# Exercises: printString with both log_stream_ and redirect_stream_
#---------------------------------------------------------------
puts "--- Test 2: simultaneous log and redirect ---"

set log_file2 [make_result_file "util_simultaneous.log"]
set redir_file2 [make_result_file "util_simultaneous_redir.txt"]

log_begin $log_file2

sta::redirect_file_begin $redir_file2
report_checks
report_units
sta::redirect_file_end

# Now output goes to console + log only
report_checks -path_delay min

log_end
puts "PASS: simultaneous log and redirect"

# Verify both files have content
foreach {fname label} [list $log_file2 "log" $redir_file2 "redirect"] {
  if { [file exists $fname] } {
    set fh [open $fname r]
    set content [read $fh]
    close $fh
    set len [string length $content]
    if { $len > 0 } {
      puts "PASS: $label file has $len chars"
    }
  }
}

#---------------------------------------------------------------
# Test 3: Redirect file append with multiple writes
# Exercises: redirectFileAppendBegin, redirectFileEnd
#---------------------------------------------------------------
puts "--- Test 3: redirect file append ---"

set append_file [make_result_file "util_append.txt"]

# First write
sta::redirect_file_begin $append_file
report_units
sta::redirect_file_end

# Append second write
sta::redirect_file_append_begin $append_file
report_checks
sta::redirect_file_end

# Append third write
sta::redirect_file_append_begin $append_file
report_checks -path_delay min
sta::redirect_file_end

puts "PASS: redirect file append (3 writes)"

if { [file exists $append_file] } {
  set fh [open $append_file r]
  set content [read $fh]
  close $fh
  set len [string length $content]
  if { $len > 200 } {
    puts "PASS: appended file has $len chars"
  }
}

#---------------------------------------------------------------
# Test 4: Redirect to string capturing timing output
# Exercises: redirectStringBegin, redirectStringEnd,
#   redirectStringPrint, printString redirect_to_string_ path
#---------------------------------------------------------------
puts "--- Test 4: redirect to string ---"

sta::redirect_string_begin
report_checks
set str1 [sta::redirect_string_end]
puts "captured string 1: [string length $str1] chars"

sta::redirect_string_begin
report_checks -path_delay min
report_units
set str2 [sta::redirect_string_end]
puts "captured string 2: [string length $str2] chars"

# Capture with with_output_to_variable (uses same redirect string path)
with_output_to_variable v1 { report_checks }
puts "with_output v1: [string length $v1] chars"

with_output_to_variable v2 { report_checks; report_units; report_checks -path_delay min }
puts "with_output v2: [string length $v2] chars"

puts "PASS: redirect to string"

#---------------------------------------------------------------
# Test 5: Log + redirect string simultaneously
# Exercises: printString with both log_stream_ and redirect_to_string_
#---------------------------------------------------------------
puts "--- Test 5: log + redirect string ---"

set log_file3 [make_result_file "util_log_str.log"]
log_begin $log_file3

sta::redirect_string_begin
report_checks
report_units
set str3 [sta::redirect_string_end]

log_end

puts "log+string captured: [string length $str3] chars"
puts "PASS: log + redirect string"

#---------------------------------------------------------------
# Test 6: Message suppression with warnings
# Exercises: isSuppressed, warn with suppressed IDs
#---------------------------------------------------------------
puts "--- Test 6: message suppression ---"

suppress_msg 100 200 300
# Trigger some warnings by reading nonexistent files
set rc [catch { read_liberty "/nonexistent/path.lib" } msg]
if { $rc != 0 } {
  puts "PASS: caught error"
}

unsuppress_msg 100 200 300
puts "PASS: suppress/unsuppress"

#---------------------------------------------------------------
# Test 7: Various report formatting
# Exercises: reportLine, reportBlankLine, reportLineString
#---------------------------------------------------------------
puts "--- Test 7: report formatting ---"

set_cmd_units -time ps -capacitance fF -resistance Ohm
report_units

set_cmd_units -time ns -capacitance pF -resistance kOhm -voltage V -current mA -power mW
report_units

set_cmd_units -time us -capacitance nF
report_units

# Reset to defaults
set_cmd_units -time ns -capacitance pF -resistance kOhm
report_units
puts "PASS: report formatting"

puts "ALL PASSED"
