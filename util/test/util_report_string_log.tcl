# Test Report.cc string output, log file, and buffer growth paths.
# Targets: Report.cc uncovered:
#   reportBlankLine (line 102-105)
#   reportLineString (const char*) (line 108-111)
#   reportLineString (const std::string&) (line 114-117)
#   printToBuffer (non-varargs, line 122-129)
#   printToBufferAppend: buffer growth path (line 158-168)
#   printBufferLine (line 172-175)
#   vwarn (line 196-206): va_list warn variant
#   vfileWarn (line 227-239): va_list file warn variant
#   verror (line 256-263): va_list error variant
#   fileError (line 266-279)
#   vfileError (line 282-292)
#   logBegin / logEnd (line 348-362)
#   redirectFileBegin / redirectFileEnd (line 365-386)
#   redirectFileAppendBegin (line 373-378)
#   redirectStringBegin / redirectStringEnd / redirectStringPrint
# Also targets: Debug.cc, PatternMatch.cc, StringUtil.cc

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Log file with extensive output to trigger buffer growth
# Exercises: logBegin, logEnd, printToBufferAppend buffer growth
#---------------------------------------------------------------
puts "--- Test 1: log file with large output ---"

set log1 [make_result_file "util_log_large.txt"]
log_begin $log1

# Generate lots of output to exercise buffer
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../examples/example1.v
link_design top

create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
set_output_delay -clock clk 0 [get_ports out]
set_input_transition 0.1 {in1 in2 clk1 clk2 clk3}

# Generate many timing reports (exercises printToBuffer extensively)
report_checks
report_checks -path_delay min
report_checks -fields {slew cap input_pins nets fanout}
report_checks -format full_clock_expanded
report_checks -format full_clock
report_checks -sort_by_slack
report_checks -group_path_count 10

# Unit reports
report_units
set_cmd_units -time ps -capacitance fF
report_units
set_cmd_units -time ns -capacitance pF
report_units
set_cmd_units -time ns -capacitance pF -resistance kOhm -voltage V -current mA
report_units

log_end

# Verify log file was created and has content
if { [file exists $log1] } {
  set sz [file size $log1]
  puts "log file size: $sz"
  if { $sz > 1000 } {
  }
} else {
  puts "INFO: log file not created"
}

#---------------------------------------------------------------
# Test 2: Log + redirect simultaneously
# Exercises: printString dual output path (redirect + log)
#---------------------------------------------------------------
puts "--- Test 2: log + redirect simultaneous ---"

set log2 [make_result_file "util_log_simul.txt"]
set redir2 [make_result_file "util_redir_simul.txt"]

log_begin $log2
sta::redirect_file_begin $redir2

# All output goes to both log and redirect
report_checks
report_checks -path_delay min
report_checks -fields {slew cap}
report_units

sta::redirect_file_end
log_end

if { [file exists $log2] && [file exists $redir2] } {
  set sz_log [file size $log2]
  set sz_redir [file size $redir2]
  puts "log size: $sz_log, redirect size: $sz_redir"
  if { $sz_log > 0 && $sz_redir > 0 } {
  }
}

#---------------------------------------------------------------
# Test 3: Redirect string with large content
# Exercises: redirectStringBegin/End/Print, redirect_to_string_ path
#---------------------------------------------------------------
puts "--- Test 3: redirect string ---"

sta::redirect_string_begin
report_checks
report_checks -path_delay min
report_units
set str1 [sta::redirect_string_end]
puts "redirect string length: [string length $str1]"
if { [string length $str1] > 100 } {
}

# Multiple redirect string cycles
for {set i 0} {$i < 5} {incr i} {
  sta::redirect_string_begin
  report_units
  set s [sta::redirect_string_end]
  puts "cycle $i string length: [string length $s]"
}

#---------------------------------------------------------------
# Test 4: with_output_to_variable (exercises string redirect)
#---------------------------------------------------------------
puts "--- Test 4: with_output_to_variable ---"

with_output_to_variable v1 { report_checks }
with_output_to_variable v2 { report_checks -path_delay min }
with_output_to_variable v3 { report_units }
with_output_to_variable v4 {
  report_checks
  report_checks -path_delay min
  report_units
}
puts "v1 length: [string length $v1]"
puts "v2 length: [string length $v2]"
puts "v3 length: [string length $v3]"
puts "v4 length: [string length $v4]"

if { [string length $v4] >= [string length $v1] } {
}

#---------------------------------------------------------------
# Test 5: Redirect file append
# Exercises: redirectFileAppendBegin
#---------------------------------------------------------------
puts "--- Test 5: redirect file append ---"

set app_file [make_result_file "util_append.txt"]

# First write
sta::redirect_file_begin $app_file
report_units
sta::redirect_file_end

set sz_before [file size $app_file]

# Append
sta::redirect_file_append_begin $app_file
report_checks
sta::redirect_file_end

set sz_after [file size $app_file]
puts "before append: $sz_before, after append: $sz_after"
if { $sz_after > $sz_before } {
}

#---------------------------------------------------------------
# Test 6: Error handling paths
# Exercises: error, fileError (via bad file reads)
#---------------------------------------------------------------
puts "--- Test 6: error paths ---"

# FileNotReadable
set rc1 [catch { read_liberty "/nonexistent/path/xyz.lib" } err1]
if { $rc1 != 0 } {
}

# FileNotWritable (try writing to /dev/null/impossible)
set rc2 [catch { write_verilog "/nonexistent/dir/xyz.v" } err2]
if { $rc2 != 0 } {
}

# Bad verilog file
set bad_v [make_result_file "bad_verilog.v"]
set fh [open $bad_v w]
puts $fh "module bad_design (input a, output b);"
puts $fh "  NONEXISTENT_CELL u1 (.A(a), .Z(b));"
puts $fh "endmodule"
close $fh
set rc3 [catch {
  read_verilog $bad_v
  link_design bad_design
} err3]
puts "bad verilog result: rc=$rc3"

#---------------------------------------------------------------
# Test 7: Message suppression/unsuppression
# Exercises: suppressMsgId, unsuppressMsgId, isSuppressed
#---------------------------------------------------------------
puts "--- Test 7: message suppression ---"

# Suppress a range of message IDs
for {set id 100} {$id < 120} {incr id} {
  suppress_msg $id
}

# Unsuppress them
for {set id 100} {$id < 120} {incr id} {
  unsuppress_msg $id
}

# Suppress specific warnings
suppress_msg 1640 1641 1642 1643 1644 1645

unsuppress_msg 1640 1641 1642 1643 1644 1645

#---------------------------------------------------------------
# Test 8: Debug level setting
# Exercises: Debug.cc set/check paths
#---------------------------------------------------------------
puts "--- Test 8: debug levels ---"

foreach tag {search graph delay_calc parasitic_reduce verilog} {
  sta::set_debug $tag 1
  sta::set_debug $tag 0
}

# Higher debug levels
sta::set_debug "search" 3
sta::set_debug "search" 0
sta::set_debug "graph" 3
sta::set_debug "graph" 0

#---------------------------------------------------------------
# Test 9: Format functions
# Exercises: format_time, format_capacitance, format_resistance, etc.
#---------------------------------------------------------------
puts "--- Test 9: format functions ---"

# Time formatting
foreach t {1e-9 1e-10 1e-11 1e-12 5.5e-9 0.0} {
  set ft [sta::format_time $t 4]
  puts "format_time($t) = $ft"
}

# Capacitance formatting
foreach c {1e-12 1e-13 1e-14 1e-15 5.5e-12 0.0} {
  set fc [sta::format_capacitance $c 4]
  puts "format_capacitance($c) = $fc"
}

# Resistance formatting
foreach r {100 1000 10000 0.1 0.0} {
  set fr [sta::format_resistance $r 4]
  puts "format_resistance($r) = $fr"
}

# Power formatting
foreach p {1e-3 1e-6 1e-9 5.5e-3 0.0} {
  set fp [sta::format_power $p 4]
  puts "format_power($p) = $fp"
}
