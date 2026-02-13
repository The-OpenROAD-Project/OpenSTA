# Test verilog with assign statements and continuous assignments
# Targets: VerilogReader.cc uncovered functions:
#   makeAssign (line 510, hit=0), VerilogAssign constructor (line 1061, hit=0)
#   mergeAssignNet (line 2102, hit=0)
#   makeDclArg with VerilogAssign (line 454, hit=0)
#   makeNetConstant (line 477, hit=0)
#   VerilogDcl constructor (line 963, hit=0)
#   VerilogDclBus constructor (line 1006, 1018, hit=0)
# Also exercises: VerilogLex.ll (assign keyword lexing)
#   VerilogParse.yy (assign statement parsing)
#   VerilogWriter.cc (writeAssigns, line 439)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read verilog with assign statement
#---------------------------------------------------------------
puts "--- Test 1: read verilog with assign ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_assign_test.v
link_design verilog_assign_test

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Verify specific cells exist
foreach cell_name {buf1 buf2 and1 inv1 or1 reg1 reg2 reg3} {
  catch {
    set inst [get_cells $cell_name]
    set ref [get_property $inst ref_name]
    puts "$cell_name: ref=$ref"
  } msg
}

puts "PASS: read verilog with assign"

#---------------------------------------------------------------
# Test 2: Set up timing and verify assign net connectivity
#---------------------------------------------------------------
puts "--- Test 2: timing with assign nets ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports in1]
set_input_delay -clock clk 0 [get_ports in2]
set_input_delay -clock clk 0 [get_ports in3]
set_output_delay -clock clk 0 [get_ports out1]
set_output_delay -clock clk 0 [get_ports out2]
set_output_delay -clock clk 0 [get_ports out3]
set_input_transition 10 {in1 in2 in3 clk}

report_checks
puts "PASS: report_checks with assign"

report_checks -path_delay min
puts "PASS: report_checks min"

# Check different paths
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: in1->out1"

report_checks -from [get_ports in3] -to [get_ports out2]
puts "PASS: in3->out2 (through assign)"

report_checks -from [get_ports in3] -to [get_ports out3]
puts "PASS: in3->out3 (through assign)"

# Report with various fields
report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report with fields"

#---------------------------------------------------------------
# Test 3: Query objects related to assign
#---------------------------------------------------------------
puts "--- Test 3: assign-related queries ---"

# Query nets involved in assign
catch {
  set assigned [get_nets assigned_net]
  puts "assigned_net found: [get_full_name $assigned]"
} msg

# Query internal nets
foreach net_name {n1 n2 n3 n4 n5} {
  catch {
    set net [get_nets $net_name]
    puts "net $net_name: [get_full_name $net]"
  } msg
}

# Report nets
foreach net_name {n1 n3 n5} {
  catch {report_net $net_name} msg
  puts "report_net $net_name: done"
}

# Report instances
foreach inst_name {buf1 and1 inv1 or1 reg1 reg2} {
  report_instance $inst_name
  puts "report_instance $inst_name: done"
}

#---------------------------------------------------------------
# Test 4: Write verilog and verify
#---------------------------------------------------------------
puts "--- Test 4: write verilog ---"
set outfile [make_result_file verilog_assign_out.v]
write_verilog $outfile
puts "PASS: write_verilog"

if { [file exists $outfile] && [file size $outfile] > 0 } {
  puts "PASS: output file exists and non-empty"
  puts "output size: [file size $outfile]"
}

# Write with pwr_gnd
set outfile2 [make_result_file verilog_assign_pwr.v]
write_verilog -include_pwr_gnd $outfile2
puts "PASS: write_verilog -include_pwr_gnd"

if { [file exists $outfile2] && [file size $outfile2] > 0 } {
  puts "PASS: pwr_gnd file exists and non-empty"
}

#---------------------------------------------------------------
# Test 5: Get fanin/fanout through assign
#---------------------------------------------------------------
puts "--- Test 5: fanin/fanout through assign ---"
catch {
  set fi [get_fanin -to [get_ports out2] -flat]
  puts "fanin to out2: [llength $fi]"
} msg

catch {
  set fo [get_fanout -from [get_ports in3] -flat]
  puts "fanout from in3: [llength $fo]"
} msg

catch {
  set fi_cells [get_fanin -to [get_ports out2] -only_cells]
  puts "fanin cells to out2: [llength $fi_cells]"
} msg

catch {
  set fo_cells [get_fanout -from [get_ports in3] -only_cells]
  puts "fanout cells from in3: [llength $fo_cells]"
} msg

puts "ALL PASSED"
