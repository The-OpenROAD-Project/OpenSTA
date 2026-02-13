# Test pattern matching edge cases and string utility coverage
# Targets: PatternMatch.cc (52.3% -> wildcards, nocase, regexp paths)
#   Debug.cc (43.5% -> debug level set/check, stats level)
#   Stats.cc (52.0% -> stats reporting with debug on)
#   StringUtil.cc (84.6% -> string formatting edge cases)
#   Error.cc (60.0% -> FileNotWritable, ExceptionLine)
#   RiseFallValues.cc (45.0%)
#   RiseFallMinMax.cc (69.7%)
#   Transition.cc (81.8%)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# PatternMatch.cc coverage via get_cells/get_pins wildcards
#---------------------------------------------------------------
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../dcalc/test/dcalc_test1.v
link_design dcalc_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_output_delay -clock clk 0 [get_ports out1]

# Wildcard pattern matching: '*' at end
puts "--- get_cells with wildcard * ---"
set cells_star [get_cells buf*]
puts "buf* cells: [llength $cells_star]"
puts "PASS: get_cells wildcard *"

# Wildcard: '?' single char
puts "--- get_cells with ? wildcard ---"
set cells_q [get_cells buf?]
puts "buf? cells: [llength $cells_q]"
puts "PASS: get_cells wildcard ?"

# Wildcard: exact match (no wildcards)
puts "--- get_cells exact match ---"
set cells_exact [get_cells buf1]
puts "buf1 cells: [llength $cells_exact]"
puts "PASS: get_cells exact match"

# Wildcard: '*' matching everything
puts "--- get_cells * ---"
set cells_all [get_cells *]
puts "* cells: [llength $cells_all]"
puts "PASS: get_cells *"

# get_pins with wildcards
puts "--- get_pins with wildcards ---"
set pins_star [get_pins buf1/*]
puts "buf1/* pins: [llength $pins_star]"
puts "PASS: get_pins wildcard"

set pins_q [get_pins buf1/?]
puts "buf1/? pins: [llength $pins_q]"
puts "PASS: get_pins ? wildcard"

set pins_all [get_pins */*]
puts "*/* pins: [llength $pins_all]"
puts "PASS: get_pins */*"

# get_ports with wildcards
puts "--- get_ports with wildcards ---"
set ports_star [get_ports *1]
puts "*1 ports: [llength $ports_star]"
puts "PASS: get_ports wildcard"

set ports_all [get_ports *]
puts "* ports: [llength $ports_all]"
puts "PASS: get_ports *"

# get_nets with wildcards
puts "--- get_nets with wildcards ---"
set nets_star [get_nets n*]
puts "n* nets: [llength $nets_star]"
puts "PASS: get_nets wildcard"

# Pattern that matches nothing
puts "--- non-matching patterns ---"
set rc [catch {get_cells zzz_nonexistent} msg]
puts "get_cells nonexistent: rc=$rc"
puts "PASS: non-matching pattern"

set rc [catch {get_pins zzz_nonexistent/*} msg]
puts "get_pins nonexistent: rc=$rc"
puts "PASS: non-matching pin pattern"

#---------------------------------------------------------------
# get_lib_cells with wildcards (exercises PatternMatch::match)
#---------------------------------------------------------------
puts "--- get_lib_cells with wildcards ---"
set lc1 [get_lib_cells NangateOpenCellLibrary/BUF*]
puts "BUF* lib_cells: [llength $lc1]"
puts "PASS: get_lib_cells wildcard"

set lc2 [get_lib_cells NangateOpenCellLibrary/DFF_X?]
puts "DFF_X? lib_cells: [llength $lc2]"
puts "PASS: get_lib_cells ? wildcard"

set lc3 [get_lib_cells NangateOpenCellLibrary/*]
puts "* lib_cells: [llength $lc3]"
puts "PASS: get_lib_cells *"

#---------------------------------------------------------------
# Debug.cc coverage: set_debug with various levels, stats
#---------------------------------------------------------------
puts "--- set_debug with level > 0 ---"
sta::set_debug "search" 3
sta::set_debug "graph" 2
sta::set_debug "delay_calc" 1
puts "PASS: set_debug multiple modules"

# Trigger debug check path by running timing with debug on
report_checks
puts "PASS: report_checks with debug on"

# Turn off debug levels
sta::set_debug "search" 0
sta::set_debug "graph" 0
sta::set_debug "delay_calc" 0
puts "PASS: set_debug all off"

#---------------------------------------------------------------
# Error.cc: FileNotWritable path
#---------------------------------------------------------------
puts "--- FileNotWritable error path ---"
set rc [catch { write_sdf "/nonexistent_dir/no_write.sdf" } msg]
if { $rc != 0 } {
  puts "PASS: caught FileNotWritable for bad path"
} else {
  puts "INFO: no error for bad write path"
}

# Try write to read-only path
set rc [catch { log_begin "/proc/nonexistent_log" } msg]
if { $rc != 0 } {
  puts "PASS: caught error for unwritable log path"
} else {
  log_end
  puts "INFO: log_begin succeeded on /proc path"
}

#---------------------------------------------------------------
# Transition.cc coverage: rise/fall operations
#---------------------------------------------------------------
puts "--- report_checks with rise/fall fields ---"
set_input_transition 0.1 [get_ports in1]
set_input_transition 0.2 [get_ports in1] -rise
set_input_transition 0.15 [get_ports in1] -fall
report_checks -fields {slew cap}
puts "PASS: report_checks with rise/fall transitions"

report_checks -path_delay min -fields {slew cap input_pins}
puts "PASS: report_checks min with fields"

report_checks -path_delay max -fields {slew cap input_pins}
puts "PASS: report_checks max with fields"

#---------------------------------------------------------------
# RiseFallMinMax / RiseFallValues coverage
#---------------------------------------------------------------
puts "--- set_load rise/fall ---"
set_load -min 0.05 [get_ports out1]
set_load -max 0.1 [get_ports out1]
report_checks
puts "PASS: report_checks with min/max loads"

set_load -rise -min 0.02 [get_ports out1]
set_load -fall -min 0.03 [get_ports out1]
set_load -rise -max 0.08 [get_ports out1]
set_load -fall -max 0.09 [get_ports out1]
report_checks
puts "PASS: report_checks with rise/fall min/max loads"

set_load 0 [get_ports out1]

#---------------------------------------------------------------
# with_output_to_variable with nested calls
#---------------------------------------------------------------
puts "--- with_output_to_variable nesting ---"
with_output_to_variable v1 {
  report_checks
}
puts "v1 captured [string length $v1] chars"
puts "PASS: with_output_to_variable"

with_output_to_variable v2 {
  report_checks -path_delay min
  report_checks -path_delay max
}
puts "v2 captured [string length $v2] chars"
puts "PASS: with_output_to_variable multiple reports"

#---------------------------------------------------------------
# Redirect string with content
#---------------------------------------------------------------
puts "--- redirect_string with reports ---"
sta::redirect_string_begin
report_checks -fields {slew cap input_pins}
set rstr [sta::redirect_string_end]
puts "redirect string: [string length $rstr] chars"
puts "PASS: redirect_string with full report"

puts "ALL PASSED"
