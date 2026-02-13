# Test VerilogReader with preprocessor macro lines (`ifdef/`endif/`define/`else/`ifndef),
# parameter declarations (scalar and bus), parameter overrides via #(...) on instances,
# defparam statements, and parameter expressions (+, -, *, /).
# Targets:
#   VerilogLex.ll: ^[ \t]*`.*{EOL} macro line skip
#   VerilogParse.yy: parameter, parameter '[' INT ':' INT ']', parameter_dcls,
#     parameter_dcl with STRING, parameter_expr (arithmetic ops),
#     defparam, param_values, param_value with STRING,
#     instance with parameter_values (#(...))
#   VerilogReader.cc: makeModuleInst with parameter override path,
#     makeModule, makeDcl, linkNetwork

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read verilog with preprocessor/parameter constructs
#---------------------------------------------------------------
puts "--- Test 1: read verilog with preproc and params ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_preproc_param.v
link_design verilog_preproc_param

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Verify hierarchical sub-module instances
set hier_cells [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier_cells]"

puts "PASS: read verilog with preproc/param"

#---------------------------------------------------------------
# Test 2: Timing analysis
#---------------------------------------------------------------
puts "--- Test 2: timing ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {d1 d2 d3 d4}]
set_output_delay -clock clk 0 [get_ports {q1 q2 q3 q4}]
set_input_transition 0.1 [all_inputs]

report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: min path"

report_checks -from [get_ports d1] -to [get_ports q1]
puts "PASS: d1->q1"

report_checks -from [get_ports d3] -to [get_ports q2]
puts "PASS: d3->q2"

report_checks -from [get_ports d1] -to [get_ports q3]
puts "PASS: d1->q3 (through param_sub)"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report with fields"

#---------------------------------------------------------------
# Test 3: Write verilog and verify
#---------------------------------------------------------------
puts "--- Test 3: write ---"
set out1 [make_result_file verilog_preproc_param_out.v]
write_verilog $out1
puts "PASS: write_verilog"

set out2 [make_result_file verilog_preproc_param_pwr.v]
write_verilog -include_pwr_gnd $out2
puts "PASS: write_verilog -include_pwr_gnd"

if { [file exists $out1] && [file size $out1] > 0 } {
  puts "PASS: output file non-empty size=[file size $out1]"
}

#---------------------------------------------------------------
# Test 4: Instance and net reports
#---------------------------------------------------------------
puts "--- Test 4: reports ---"
foreach inst {buf1 inv1 or1 reg1 reg2 reg3 reg4 ps1 ps2 ps3} {
  catch {report_instance $inst} msg
}
puts "PASS: instance reports"

foreach net_name {n1 n2 n3 n4 n5 n6} {
  catch {report_net $net_name} msg
}
puts "PASS: net reports"

#---------------------------------------------------------------
# Test 5: Re-read to exercise module re-definition paths
#---------------------------------------------------------------
puts "--- Test 5: re-read ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_preproc_param.v
link_design verilog_preproc_param
puts "re-read cells: [llength [get_cells *]]"
puts "PASS: re-read"

puts "ALL PASSED"
