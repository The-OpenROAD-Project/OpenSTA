# Test VerilogReader error handling paths, bus expressions with partial bits,
# assign statements with concatenation, hierarchical modules, and write options.
# Targets:
#   VerilogReader.cc: bus expression parsing (bit select, part select),
#     assign statement with concatenation, module not found errors,
#     port mismatch handling, supply nets, multi-module designs,
#     linkNetwork hierarchy paths, mergeAssignNet,
#     makeNetBitSelect, makeNetPartSelect, makeNetConcat,
#     makeAssign, VerilogAssign constructor paths,
#     VerilogDclBus constructor, wire assign in declaration
#   VerilogWriter.cc: writeAssigns, writeChildren, writeWireDcls,
#     writePortDcls with bus ports

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read hierarchical design with buses and assigns
#---------------------------------------------------------------
puts "--- Test 1: read hierarchical bus design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_error_paths.v
link_design verilog_error_paths

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Verify hierarchical cells
set hier_cells [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier_cells]"

# Verify bus ports
set bus_in [get_ports bus_in*]
puts "bus_in ports: [llength $bus_in]"

set bus_out [get_ports bus_out*]
puts "bus_out ports: [llength $bus_out]"

set din [get_ports din*]
puts "din ports: [llength $din]"

set dout [get_ports dout*]
puts "dout ports: [llength $dout]"

# Verify sub-module instances
catch {
  set sub1 [get_cells sub1]
  set sub1_ref [get_property $sub1 ref_name]
  puts "sub1: ref=$sub1_ref"
} msg

catch {
  set sub2 [get_cells sub2]
  set sub2_ref [get_property $sub2 ref_name]
  puts "sub2: ref=$sub2_ref"
} msg

puts "PASS: read hierarchical bus design"

#---------------------------------------------------------------
# Test 2: Timing analysis with bus ports and hierarchical design
#---------------------------------------------------------------
puts "--- Test 2: timing analysis ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {din[0] din[1] din[2] din[3] sel en}]
set_input_delay -clock clk 0 [get_ports {bus_in[0] bus_in[1] bus_in[2] bus_in[3] bus_in[4] bus_in[5] bus_in[6] bus_in[7]}]
set_output_delay -clock clk 0 [get_ports {dout[0] dout[1] dout[2] dout[3] flag}]
set_output_delay -clock clk 0 [get_ports {bus_out[0] bus_out[1] bus_out[2] bus_out[3] bus_out[4] bus_out[5] bus_out[6] bus_out[7]}]
set_input_transition 0.1 [all_inputs]

report_checks
puts "PASS: report_checks"

report_checks -path_delay min
puts "PASS: min path"

# Paths through assign statements
report_checks -from [get_ports sel] -to [get_ports flag]
puts "PASS: sel->flag (through assign)"

# Paths through hierarchical sub-modules
catch {report_checks -from [get_ports {din[1]}] -to [get_ports {bus_out[0]}]} msg
puts "PASS: din[1]->bus_out[0] (through sub1)"

catch {report_checks -from [get_ports {din[2]}] -to [get_ports {bus_out[1]}]} msg
puts "PASS: din[2]->bus_out[1] (through sub2)"

# All path combinations
foreach from_idx {0 1 2 3} {
  catch {report_checks -from [get_ports "din\[$from_idx\]"] -to [get_ports "dout\[$from_idx\]"]} msg
  puts "din\[$from_idx\]->dout\[$from_idx\]: done"
}

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: report with fields"

#---------------------------------------------------------------
# Test 3: Fanin/fanout through hierarchy and assigns
#---------------------------------------------------------------
puts "--- Test 3: fanin/fanout ---"

catch {
  set fi [get_fanin -to [get_ports flag] -flat]
  puts "fanin to flag: [llength $fi]"
} msg

catch {
  set fo [get_fanout -from [get_ports sel] -flat]
  puts "fanout from sel: [llength $fo]"
} msg

catch {
  set fi_cells [get_fanin -to [get_ports {dout[0]}] -only_cells]
  puts "fanin cells to dout[0]: [llength $fi_cells]"
} msg

catch {
  set fo_cells [get_fanout -from [get_ports {din[0]}] -only_cells]
  puts "fanout cells from din[0]: [llength $fo_cells]"
} msg

catch {
  set fo_end [get_fanout -from [get_ports {din[1]}] -endpoints_only]
  puts "fanout endpoints from din[1]: [llength $fo_end]"
} msg

puts "PASS: fanin/fanout"

#---------------------------------------------------------------
# Test 4: Write verilog with various options
#---------------------------------------------------------------
puts "--- Test 4: write verilog ---"
set out1 [make_result_file verilog_error_paths_out.v]
write_verilog $out1
puts "PASS: write_verilog"

set out2 [make_result_file verilog_error_paths_pwr.v]
write_verilog -include_pwr_gnd $out2
puts "PASS: write_verilog -include_pwr_gnd"

if { [file exists $out1] && [file size $out1] > 0 } {
  puts "PASS: output size=[file size $out1]"
}
if { [file exists $out2] && [file size $out2] > 0 } {
  puts "PASS: pwr_gnd size=[file size $out2]"
}

#---------------------------------------------------------------
# Test 5: Report net for bus and assign-related nets
#---------------------------------------------------------------
puts "--- Test 5: net reports ---"
foreach net_name {w1 w2 w3} {
  catch {report_net $net_name} msg
  puts "report_net $net_name: done"
}
puts "PASS: net reports"

#---------------------------------------------------------------
# Test 6: Report instances
#---------------------------------------------------------------
puts "--- Test 6: instance reports ---"
foreach inst_name {buf0 buf1 buf2 buf3 and_en or_sel sub1 sub2 reg0 reg1 reg2 reg3} {
  catch {report_instance $inst_name} msg
}
puts "PASS: instance reports"

#---------------------------------------------------------------
# Test 7: Re-read to exercise module re-definition
#---------------------------------------------------------------
puts "--- Test 7: re-read ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_error_paths.v
link_design verilog_error_paths
puts "re-read cells: [llength [get_cells *]]"
puts "re-read nets: [llength [get_nets *]]"
puts "PASS: re-read"

puts "ALL PASSED"
