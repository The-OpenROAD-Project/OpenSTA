# Test SdcNetwork namespace switching and VerilogNamespace escape handling.
# Targets:
#   SdcNetwork.cc: findPort, findPin, findNet with SdcNetwork name adapter,
#     escapeDividers, escapeBrackets, portDirection, findInstancesMatching,
#     findPinsHierMatching, findNetsMatching, findNetsHierMatching,
#     NetworkNameAdapter port/pin/net/instance delegation methods
#   VerilogNamespace.cc: staToVerilog, staToVerilog2, verilogToSta,
#     cellVerilogName, instanceVerilogName, netVerilogName, portVerilogName,
#     moduleVerilogToSta, instanceVerilogToSta, netVerilogToSta,
#     portVerilogToSta
#   Network.cc: setPathDivider, setPathEscape, pathDivider, pathEscape,
#     findPin(path_name), findNet(path_name), findInstance(path_name)
#   ParseBus.cc: parseBusName, isBusName, escapeChars

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib

#---------------------------------------------------------------
# Test 1: SDC namespace operations with flat design
#---------------------------------------------------------------
puts "--- Test 1: SDC namespace with flat design ---"
read_verilog ../../verilog/test/verilog_bus_test.v
link_design verilog_bus_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_in[*]}]
set_output_delay -clock clk 0 [get_ports {data_out[*]}]
set_input_delay -clock clk 0 [get_ports {sel enable}]
set_input_transition 0.1 [all_inputs]

# Switch to SDC namespace
sta::set_cmd_namespace sdc

# Query in SDC namespace - exercises SdcNetwork name adapter path
set sdc_ports [get_ports *]
puts "sdc ports: [llength $sdc_ports]"

set sdc_cells [get_cells *]
puts "sdc cells: [llength $sdc_cells]"

set sdc_nets [get_nets *]
puts "sdc nets: [llength $sdc_nets]"

# Bus port queries in SDC namespace
set sdc_bus_in [get_ports {data_in[*]}]
puts "sdc data_in[*]: [llength $sdc_bus_in]"

set sdc_bus_out [get_ports {data_out[*]}]
puts "sdc data_out[*]: [llength $sdc_bus_out]"

# Individual bit queries in SDC namespace
foreach i {0 1 2 3} {
  set p [get_ports "data_in\[$i\]"]
  set dir [get_property $p direction]
  puts "sdc data_in\[$i\]: dir=$dir"
}

# Pin queries in SDC namespace
set sdc_pins [get_pins */*]
puts "sdc flat pins: [llength $sdc_pins]"

set sdc_hier_pins [get_pins -hierarchical *]
puts "sdc hier pins: [llength $sdc_hier_pins]"

# Net queries in SDC namespace
set sdc_n_nets [get_nets n*]
puts "sdc n* nets: [llength $sdc_n_nets]"

set sdc_hier_nets [get_nets -hierarchical *]
puts "sdc hier nets: [llength $sdc_hier_nets]"

# Instance queries in SDC namespace
set sdc_buf_cells [get_cells buf*]
puts "sdc buf* cells: [llength $sdc_buf_cells]"

set sdc_and_cells [get_cells and*]
puts "sdc and* cells: [llength $sdc_and_cells]"

set sdc_reg_cells [get_cells reg*]
puts "sdc reg* cells: [llength $sdc_reg_cells]"

# Hierarchical cell queries in SDC namespace
set sdc_hier_cells [get_cells -hierarchical *]
puts "sdc hier cells: [llength $sdc_hier_cells]"

# report_checks in SDC namespace
report_checks

report_checks -path_delay min

# Switch back to STA namespace
sta::set_cmd_namespace sta

# Verify queries still work after switching back
set sta_ports [get_ports *]
puts "sta ports: [llength $sta_ports]"

set sta_cells [get_cells *]
puts "sta cells: [llength $sta_cells]"

set sta_nets [get_nets *]
puts "sta nets: [llength $sta_nets]"

report_checks

#---------------------------------------------------------------
# Test 2: Namespace with hierarchical design
#---------------------------------------------------------------
puts "--- Test 2: SDC namespace with hierarchical design ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_hier_test.v
link_design network_hier_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]

# Switch to SDC namespace and query hierarchical objects
sta::set_cmd_namespace sdc

# Hierarchical queries in SDC namespace
set sdc_h_cells [get_cells -hierarchical *]
puts "sdc hier cells: [llength $sdc_h_cells]"

set sdc_h_pins [get_pins -hierarchical *]
puts "sdc hier pins: [llength $sdc_h_pins]"

set sdc_h_nets [get_nets -hierarchical *]
puts "sdc hier nets: [llength $sdc_h_nets]"

# Specific patterns in SDC namespace
set sdc_sub_cells [get_cells sub*]
puts "sdc sub* cells: [llength $sdc_sub_cells]"

set sdc_h_sub [get_cells -hierarchical sub*]
puts "sdc hier sub*: [llength $sdc_h_sub]"

# Port queries in SDC namespace with hierarchy
foreach pname {clk in1 in2 in3 out1 out2} {
  set p [get_ports $pname]
  set dir [get_property $p direction]
  puts "sdc port $pname: dir=$dir"
}

# Timing reports in SDC namespace
report_checks

report_checks -from [get_ports in1] -to [get_ports out1]

# Switch back to STA namespace
sta::set_cmd_namespace sta

# Verify after switching
report_checks

#---------------------------------------------------------------
# Test 3: Path divider operations
#---------------------------------------------------------------
puts "--- Test 3: path divider ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_hier_test.v
link_design network_hier_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]

# Query with default divider
set pins1 [get_pins sub1/*]
puts "sub1/* pins (default divider): [llength $pins1]"

# Try hierarchical pattern
set hier_pins1 [get_pins -hierarchical sub1/*]
puts "hier sub1/* pins: [llength $hier_pins1]"

# Query instances
set inst_sub1 [get_cells sub1]
puts "sub1 cell ref: [get_property $inst_sub1 ref_name]"

# Query nets
set all_nets [get_nets *]
puts "all nets: [llength $all_nets]"

set hier_all_nets [get_nets -hierarchical *]
puts "hier all nets: [llength $hier_all_nets]"

# Timing through hierarchy
report_checks -from [get_ports in1] -to [get_ports out1]

report_checks -from [get_ports in2] -to [get_ports out2]

report_checks -from [get_ports in3] -to [get_ports out2]

# Fanin/fanout queries exercise SdcNetwork delegation
set fi [get_fanin -to [get_ports out1] -flat]
puts "fanin to out1 flat: [llength $fi]"

set fo [get_fanout -from [get_ports in1] -flat]
puts "fanout from in1 flat: [llength $fo]"

set fi_cells [get_fanin -to [get_ports out2] -only_cells]
puts "fanin to out2 cells: [llength $fi_cells]"

set fo_ends [get_fanout -from [get_ports in3] -endpoints_only]
puts "fanout from in3 endpoints: [llength $fo_ends]"

#---------------------------------------------------------------
# Test 4: register queries through SdcNetwork
#---------------------------------------------------------------
puts "--- Test 4: register queries ---"
set regs [all_registers]
puts "all_registers: [llength $regs]"

set reg_data [all_registers -data_pins]
puts "register data_pins: [llength $reg_data]"

set reg_clk [all_registers -clock_pins]
puts "register clock_pins: [llength $reg_clk]"

set reg_out [all_registers -output_pins]
puts "register output_pins: [llength $reg_out]"

set reg_async [all_registers -async_pins]
puts "register async_pins: [llength $reg_async]"
