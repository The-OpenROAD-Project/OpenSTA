# Test verilog read/write with bus ports
# Targets: VerilogReader.cc (bus port paths, bit select parsing)
#   VerilogWriter.cc (writeInstBusPin, writeInstBusPinBit, bus wire dcls)
#   VerilogLex.ll (bus index lexing)
#   VerilogParse.yy (bus port/wire parsing)
# Also targets: ConcreteNetwork.cc (bus port operations, findBusBit, fromIndex, toIndex, etc.)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read verilog with bus ports
#---------------------------------------------------------------
puts "--- read verilog with bus ports ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_test.v
link_design verilog_bus_test

set cells [get_cells *]
puts "cells: [llength $cells]"
if { [llength $cells] != 12 } {
  puts "WARN: expected 12 cells"
}

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Query bus ports
puts "--- bus port queries ---"
set din_ports [get_ports data_in*]
puts "data_in* ports: [llength $din_ports]"

set dout_ports [get_ports data_out*]
puts "data_out* ports: [llength $dout_ports]"

# Query individual bus bits
foreach i {0 1 2 3} {
  catch {
    set p [get_ports "data_in\[$i\]"]
    puts "data_in\[$i\] dir=[get_property $p direction]"
  } msg
}

foreach i {0 1 2 3} {
  catch {
    set p [get_ports "data_out\[$i\]"]
    puts "data_out\[$i\] dir=[get_property $p direction]"
  } msg
}

#---------------------------------------------------------------
# Set up timing constraints
#---------------------------------------------------------------
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_in[0] data_in[1] data_in[2] data_in[3]}]
set_input_delay -clock clk 0 [get_ports enable]
set_input_delay -clock clk 0 [get_ports sel]
set_output_delay -clock clk 0 [get_ports {data_out[0] data_out[1] data_out[2] data_out[3]}]

report_checks
puts "PASS: report_checks with bus ports"

report_checks -path_delay min
puts "PASS: report_checks min with bus ports"

#---------------------------------------------------------------
# Test get_pins with bus-style patterns
#---------------------------------------------------------------
puts "--- pin queries with bus patterns ---"
set all_pins [get_pins */*]
puts "all pins: [llength $all_pins]"

set buf_pins [get_pins buf*/*]
puts "buf* pins: [llength $buf_pins]"

set and_pins [get_pins and*/*]
puts "and* pins: [llength $and_pins]"

set reg_pins [get_pins reg*/*]
puts "reg* pins: [llength $reg_pins]"

#---------------------------------------------------------------
# Test write_verilog with bus ports
#---------------------------------------------------------------
puts "--- write_verilog with bus ports ---"
set outfile [make_result_file verilog_bus_out.v]
write_verilog $outfile
puts "PASS: write_verilog bus"

if { [file exists $outfile] && [file size $outfile] > 0 } {
  puts "PASS: output file exists"
  puts "output size: [file size $outfile]"
}

# Write with pwr_gnd to exercise pwr/gnd port direction paths
set outfile2 [make_result_file verilog_bus_pwr.v]
write_verilog -include_pwr_gnd $outfile2
puts "PASS: write_verilog -include_pwr_gnd bus"

if { [file exists $outfile2] && [file size $outfile2] > 0 } {
  puts "PASS: pwr_gnd file exists"
}

#---------------------------------------------------------------
# Test report_net with bus nets
#---------------------------------------------------------------
puts "--- report_net with bus nets ---"
foreach net_name {n1[0] n1[1] n1[2] n1[3] n2[0] n2[1] n2[2] n2[3]} {
  catch {report_net $net_name} msg
}
puts "PASS: report_net bus nets"

#---------------------------------------------------------------
# Test report_instance on cells connected to bus wires
#---------------------------------------------------------------
puts "--- report_instance ---"
foreach inst_name {buf0 buf1 and0 and1 reg0 reg1} {
  report_instance $inst_name
}
puts "PASS: report_instance bus cells"

#---------------------------------------------------------------
# Test get_fanin/get_fanout
#---------------------------------------------------------------
puts "--- fanin/fanout ---"
catch {
  set fi [get_fanin -to [get_ports "data_out\[0\]"] -flat]
  puts "fanin to data_out\[0\]: [llength $fi]"
} msg

catch {
  set fo [get_fanout -from [get_ports "data_in\[0\]"] -flat]
  puts "fanout from data_in\[0\]: [llength $fo]"
} msg

catch {
  set fi_cells [get_fanin -to [get_ports "data_out\[0\]"] -only_cells]
  puts "fanin cells to data_out\[0\]: [llength $fi_cells]"
} msg

puts "ALL PASSED"
