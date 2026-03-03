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
suppress_msg 1140

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
set sub1 [get_cells sub1]
set sub1_ref [get_property $sub1 ref_name]
puts "sub1: ref=$sub1_ref"

set sub2 [get_cells sub2]
set sub2_ref [get_property $sub2 ref_name]
puts "sub2: ref=$sub2_ref"

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

report_checks -path_delay min

# Paths through assign statements
report_checks -from [get_ports sel] -to [get_ports flag]

# Paths through hierarchical sub-modules
report_checks -from [get_ports {din[1]}] -to [get_ports {bus_out[0]}]

report_checks -from [get_ports {din[2]}] -to [get_ports {bus_out[1]}]

# All path combinations
foreach from_idx {0 1 2 3} {
  report_checks -from [get_ports "din\[$from_idx\]"] -to [get_ports "dout\[$from_idx\]"]
  puts "din\[$from_idx\]->dout\[$from_idx\]: done"
}

report_checks -fields {slew cap input_pins nets fanout}

#---------------------------------------------------------------
# Test 3: Fanin/fanout through hierarchy and assigns
#---------------------------------------------------------------
puts "--- Test 3: fanin/fanout ---"

set fi [get_fanin -to [get_ports flag] -flat]
puts "fanin to flag: [llength $fi]"

set fo [get_fanout -from [get_ports sel] -flat]
puts "fanout from sel: [llength $fo]"

set fi_cells [get_fanin -to [get_ports {dout[0]}] -only_cells]
puts "fanin cells to dout[0]: [llength $fi_cells]"

set fo_cells [get_fanout -from [get_ports {din[0]}] -only_cells]
puts "fanout cells from din[0]: [llength $fo_cells]"

set fo_end [get_fanout -from [get_ports {din[1]}] -endpoints_only]
puts "fanout endpoints from din[1]: [llength $fo_end]"

#---------------------------------------------------------------
# Test 4: Write verilog with various options
#---------------------------------------------------------------
puts "--- Test 4: write verilog ---"
set out1 [make_result_file verilog_error_paths_out.v]
write_verilog $out1

set out2 [make_result_file verilog_error_paths_pwr.v]
write_verilog -include_pwr_gnd $out2

#---------------------------------------------------------------
# Test 5: Report net for bus and assign-related nets
#---------------------------------------------------------------
puts "--- Test 5: net reports ---"
foreach net_name {w1 w2 w3} {
  report_net $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Test 6: Report instances
#---------------------------------------------------------------
puts "--- Test 6: instance reports ---"
foreach inst_name {buf0 buf1 buf2 buf3 and_en or_sel sub1 sub2 reg0 reg1 reg2 reg3} {
  report_instance $inst_name
}

#---------------------------------------------------------------
# Test 7: Re-read to exercise module re-definition
#---------------------------------------------------------------
puts "--- Test 7: re-read ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_error_paths.v
link_design verilog_error_paths
puts "re-read cells: [llength [get_cells *]]"
puts "re-read nets: [llength [get_nets *]]"
