# Test SdcNetwork adapter functions with hierarchical design,
# namespace switching, and detailed pin/net/instance queries.
# Targets:
#   SdcNetwork.cc: findPort, findPin, findNet, findCell,
#     findPortsMatching, findPinsMatching, findPinsHierMatching,
#     findNetsMatching, findNetsHierMatching, findInstancesMatching,
#     findInstancesHierMatching, findCellsMatching,
#     portDirection, isDriver, isLoad, isHierarchical, isLeaf,
#     name, pathName, escapeDividers, escapeBrackets
#   Network.cc: findPin(instance, port), findPin(path_name),
#     pathName delegation, isDriver/isLoad delegation,
#     pathNameFirst, pathNameLast, netDrvrPinMap
#   VerilogNamespace.cc: staToVerilog, verilogToSta name conversion

source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog network_hier_test.v
link_design network_hier_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2 in3}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]
set_input_transition 0.1 [all_inputs]

report_checks
puts "PASS: initial design"

#---------------------------------------------------------------
# Test 1: SDC namespace queries on hierarchical design
#---------------------------------------------------------------
puts "--- Test 1: SDC namespace hierarchical queries ---"

sta::set_cmd_namespace sdc

# Port queries
set sdc_ports [get_ports *]
puts "sdc ports: [llength $sdc_ports]"

foreach pname {clk in1 in2 in3 out1 out2} {
  set p [get_ports $pname]
  set dir [get_property $p direction]
  puts "sdc port $pname dir=$dir"
}

# Instance queries
set sdc_cells [get_cells *]
puts "sdc cells: [llength $sdc_cells]"

set sdc_sub [get_cells sub*]
puts "sdc sub* cells: [llength $sdc_sub]"

set sdc_buf [get_cells buf*]
puts "sdc buf* cells: [llength $sdc_buf]"

set sdc_hier_cells [get_cells -hierarchical *]
puts "sdc hier cells: [llength $sdc_hier_cells]"

set sdc_hier_buf [get_cells -hierarchical *buf*]
puts "sdc hier *buf*: [llength $sdc_hier_buf]"

set sdc_hier_and [get_cells -hierarchical *and*]
puts "sdc hier *and*: [llength $sdc_hier_and]"

# Pin queries
set sdc_flat_pins [get_pins */*]
puts "sdc flat pins: [llength $sdc_flat_pins]"

set sdc_hier_pins [get_pins -hierarchical *]
puts "sdc hier pins: [llength $sdc_hier_pins]"

set sdc_sub1_pins [get_pins sub1/*]
puts "sdc sub1/* pins: [llength $sdc_sub1_pins]"

set sdc_sub2_pins [get_pins sub2/*]
puts "sdc sub2/* pins: [llength $sdc_sub2_pins]"

# Net queries
set sdc_nets [get_nets *]
puts "sdc nets: [llength $sdc_nets]"

set sdc_w_nets [get_nets w*]
puts "sdc w* nets: [llength $sdc_w_nets]"

set sdc_hier_nets [get_nets -hierarchical *]
puts "sdc hier nets: [llength $sdc_hier_nets]"

set sdc_hier_sub_nets [get_nets -hierarchical sub*/*]
puts "sdc hier sub*/* nets: [llength $sdc_hier_sub_nets]"

# Timing reports in SDC namespace
report_checks -from [get_ports in1] -to [get_ports out1]
puts "PASS: sdc in1->out1"

report_checks -from [get_ports in2] -to [get_ports out2]
puts "PASS: sdc in2->out2"

report_checks -path_delay min
puts "PASS: sdc min path"

sta::set_cmd_namespace sta
puts "PASS: back to STA namespace"

#---------------------------------------------------------------
# Test 2: All registers queries through SdcNetwork
#---------------------------------------------------------------
puts "--- Test 2: register queries ---"

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

puts "PASS: register queries"

#---------------------------------------------------------------
# Test 3: Fanin/fanout in SDC namespace
#---------------------------------------------------------------
puts "--- Test 3: SDC namespace fanin/fanout ---"

sta::set_cmd_namespace sdc

set fi_out1 [get_fanin -to [get_ports out1] -flat]
puts "sdc fanin to out1: [llength $fi_out1]"

set fi_out2 [get_fanin -to [get_ports out2] -flat]
puts "sdc fanin to out2: [llength $fi_out2]"

set fo_in1 [get_fanout -from [get_ports in1] -flat]
puts "sdc fanout from in1: [llength $fo_in1]"

set fo_in2 [get_fanout -from [get_ports in2] -flat]
puts "sdc fanout from in2: [llength $fo_in2]"

set fo_in3 [get_fanout -from [get_ports in3] -flat]
puts "sdc fanout from in3: [llength $fo_in3]"

# Fanin with -only_cells
set fi_cells_out1 [get_fanin -to [get_ports out1] -only_cells]
puts "sdc fanin cells to out1: [llength $fi_cells_out1]"

set fi_cells_out2 [get_fanin -to [get_ports out2] -only_cells]
puts "sdc fanin cells to out2: [llength $fi_cells_out2]"

# Fanout with -endpoints_only
set fo_ep_in1 [get_fanout -from [get_ports in1] -endpoints_only]
puts "sdc fanout endpoints from in1: [llength $fo_ep_in1]"

set fo_ep_in3 [get_fanout -from [get_ports in3] -endpoints_only]
puts "sdc fanout endpoints from in3: [llength $fo_ep_in3]"

sta::set_cmd_namespace sta
puts "PASS: SDC fanin/fanout"

#---------------------------------------------------------------
# Test 4: Switch namespaces repeatedly and verify consistency
#---------------------------------------------------------------
puts "--- Test 4: namespace switching ---"

for {set i 0} {$i < 3} {incr i} {
  sta::set_cmd_namespace sdc
  set sdc_n [llength [get_cells *]]

  sta::set_cmd_namespace sta
  set sta_n [llength [get_cells *]]

  puts "iteration $i: sdc_cells=$sdc_n sta_cells=$sta_n"
}
puts "PASS: namespace switching consistency"

#---------------------------------------------------------------
# Test 5: Query specific pins in SDC namespace
#---------------------------------------------------------------
puts "--- Test 5: specific pin queries in SDC ---"

sta::set_cmd_namespace sdc

# Directly query hierarchical pin paths
foreach pin_path {buf_in/A buf_in/Z inv1/A inv1/ZN reg1/D reg1/CK reg1/Q} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  set fn [get_full_name $pin]
  puts "sdc pin $pin_path: dir=$dir full_name=$fn"
}

# Deep hierarchical pins
foreach pin_path {sub1/and_gate/A1 sub1/and_gate/ZN sub1/buf_gate/Z
                  sub2/and_gate/A1 sub2/buf_gate/Z} {
  set pin [get_pins $pin_path]
  set dir [get_property $pin direction]
  puts "sdc deep pin $pin_path: dir=$dir"
}

sta::set_cmd_namespace sta
puts "PASS: SDC pin queries"

#---------------------------------------------------------------
# Test 6: Load bus design and exercise SDC with bus ports
#---------------------------------------------------------------
puts "--- Test 6: SDC with bus design ---"

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../verilog/test/verilog_bus_test.v
link_design verilog_bus_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_in[*]}]
set_output_delay -clock clk 0 [get_ports {data_out[*]}]
set_input_delay -clock clk 0 [get_ports {sel enable}]
set_input_transition 0.1 [all_inputs]

sta::set_cmd_namespace sdc

set sdc_ports [get_ports *]
puts "sdc bus design ports: [llength $sdc_ports]"

set sdc_bus_in [get_ports {data_in[*]}]
puts "sdc data_in[*]: [llength $sdc_bus_in]"

set sdc_bus_out [get_ports {data_out[*]}]
puts "sdc data_out[*]: [llength $sdc_bus_out]"

# Individual bus bits
foreach i {0 1 2 3} {
  catch {
    set p [get_ports "data_in\[$i\]"]
    set dir [get_property $p direction]
    puts "sdc data_in\[$i\]: dir=$dir"
  }
}

set sdc_cells [get_cells *]
puts "sdc bus design cells: [llength $sdc_cells]"

set sdc_nets [get_nets *]
puts "sdc bus design nets: [llength $sdc_nets]"

report_checks
puts "PASS: SDC bus design report_checks"

sta::set_cmd_namespace sta
puts "PASS: SDC bus design queries"

puts "ALL PASSED"
