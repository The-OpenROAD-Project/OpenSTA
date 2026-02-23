# Test pattern matching edge cases and string utility coverage.
# Targets PatternMatch/Debug/Stats/StringUtil/Transition paths.

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

# Wildcard: '?' single char
puts "--- get_cells with ? wildcard ---"
set cells_q [get_cells buf?]
puts "buf? cells: [llength $cells_q]"

# Wildcard: exact match (no wildcards)
puts "--- get_cells exact match ---"
set cells_exact [get_cells buf1]
puts "buf1 cells: [llength $cells_exact]"

# Wildcard: '*' matching everything
puts "--- get_cells * ---"
set cells_all [get_cells *]
puts "* cells: [llength $cells_all]"

# get_pins with wildcards
puts "--- get_pins with wildcards ---"
set pins_star [get_pins buf1/*]
puts "buf1/* pins: [llength $pins_star]"

set pins_q [get_pins buf1/?]
puts "buf1/? pins: [llength $pins_q]"

set pins_all [get_pins */*]
puts "*/* pins: [llength $pins_all]"

# get_ports with wildcards
puts "--- get_ports with wildcards ---"
set ports_star [get_ports *1]
puts "*1 ports: [llength $ports_star]"

set ports_all [get_ports *]
puts "* ports: [llength $ports_all]"

# get_nets with wildcards
puts "--- get_nets with wildcards ---"
set nets_star [get_nets n*]
puts "n* nets: [llength $nets_star]"

# Pattern that matches nothing
puts "--- non-matching patterns ---"
set no_cells [get_cells -quiet zzz_nonexistent]
puts "get_cells nonexistent count: [llength $no_cells]"
if {[llength $no_cells] != 0} {
  error "expected no matches for nonexistent cell wildcard"
}

set no_pins [get_pins -quiet zzz_nonexistent/*]
puts "get_pins nonexistent count: [llength $no_pins]"
if {[llength $no_pins] != 0} {
  error "expected no matches for nonexistent pin wildcard"
}

#---------------------------------------------------------------
# get_lib_cells with wildcards (exercises PatternMatch::match)
#---------------------------------------------------------------
puts "--- get_lib_cells with wildcards ---"
set lc1 [get_lib_cells NangateOpenCellLibrary/BUF*]
puts "BUF* lib_cells: [llength $lc1]"

set lc2 [get_lib_cells NangateOpenCellLibrary/DFF_X?]
puts "DFF_X? lib_cells: [llength $lc2]"

set lc3 [get_lib_cells NangateOpenCellLibrary/*]
puts "* lib_cells: [llength $lc3]"

#---------------------------------------------------------------
# Debug.cc coverage: set_debug with various levels, stats
#---------------------------------------------------------------
puts "--- set_debug with level > 0 ---"
sta::set_debug "search" 3
sta::set_debug "graph" 2
sta::set_debug "delay_calc" 1

# Trigger debug check path by running timing with debug on
report_checks

# Turn off debug levels
sta::set_debug "search" 0
sta::set_debug "graph" 0
sta::set_debug "delay_calc" 0

#---------------------------------------------------------------
# File path handling sanity checks
#---------------------------------------------------------------
puts "--- file path sanity ---"
if {[file exists "/nonexistent_dir/no_write.sdf"]} {
  error "unexpected existing path /nonexistent_dir/no_write.sdf"
}
if {[file exists "/proc/nonexistent_log"]} {
  error "unexpected existing path /proc/nonexistent_log"
}

#---------------------------------------------------------------
# Transition.cc coverage: rise/fall operations
#---------------------------------------------------------------
puts "--- report_checks with rise/fall fields ---"
set_input_transition 0.1 [get_ports in1]
set_input_transition 0.2 [get_ports in1] -rise
set_input_transition 0.15 [get_ports in1] -fall
report_checks -fields {slew cap}

report_checks -path_delay min -fields {slew cap input_pins}

report_checks -path_delay max -fields {slew cap input_pins}

#---------------------------------------------------------------
# RiseFallMinMax / RiseFallValues coverage
#---------------------------------------------------------------
puts "--- set_load rise/fall ---"
set_load -min 0.05 [get_ports out1]
set_load -max 0.1 [get_ports out1]
report_checks

set_load -rise -min 0.02 [get_ports out1]
set_load -fall -min 0.03 [get_ports out1]
set_load -rise -max 0.08 [get_ports out1]
set_load -fall -max 0.09 [get_ports out1]
report_checks

set_load 0 [get_ports out1]

#---------------------------------------------------------------
# with_output_to_variable with nested calls
#---------------------------------------------------------------
puts "--- with_output_to_variable nesting ---"
with_output_to_variable v1 {
  report_checks
}
puts "v1 captured [string length $v1] chars"

with_output_to_variable v2 {
  report_checks -path_delay min
  report_checks -path_delay max
}
puts "v2 captured [string length $v2] chars"

#---------------------------------------------------------------
# Redirect string with content
#---------------------------------------------------------------
puts "--- redirect_string with reports ---"
sta::redirect_string_begin
report_checks -fields {slew cap input_pins}
set rstr [sta::redirect_string_end]
puts "redirect string: [string length $rstr] chars"
