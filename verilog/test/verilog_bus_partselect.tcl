# Test VerilogReader and VerilogWriter with bus part-select, bit-select,
# concatenation expressions, hierarchical sub-modules with bus ports,
# and write_verilog roundtrip of bus designs.

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: Read verilog with bus expressions
#---------------------------------------------------------------
puts "--- Test 1: read bus partselect verilog ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog verilog_bus_partselect.v
link_design verilog_bus_partselect

set cells [get_cells *]
puts "cells: [llength $cells]"

set nets [get_nets *]
puts "nets: [llength $nets]"

set ports [get_ports *]
puts "ports: [llength $ports]"

# Verify hierarchical instances
set hier_cells [get_cells -hierarchical *]
puts "hierarchical cells: [llength $hier_cells]"

#---------------------------------------------------------------
# Test 2: Timing analysis with bus design
#---------------------------------------------------------------
puts "--- Test 2: timing ---"
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports data_in*]
set_input_delay -clock clk 0 [get_ports sel]
set_output_delay -clock clk 0 [get_ports data_out*]
set_output_delay -clock clk 0 [get_ports valid]
set_input_transition 0.1 [all_inputs]

report_checks

report_checks -path_delay min

report_checks -from [get_ports {data_in[0]}] -to [get_ports {data_out[0]}]

report_checks -from [get_ports {data_in[4]}] -to [get_ports {data_out[4]}]

report_checks -from [get_ports sel] -to [get_ports valid]

report_checks -fields {slew cap input_pins fanout}

#---------------------------------------------------------------
# Test 3: Write verilog with bus nets (exercises bus wire dcls)
#---------------------------------------------------------------
puts "--- Test 3: write verilog ---"

set out1 [make_result_file verilog_bus_ps_basic.v]
write_verilog $out1
diff_files verilog_bus_ps_basic.vok $out1

# With power/ground
set out2 [make_result_file verilog_bus_ps_pwr.v]
write_verilog -include_pwr_gnd $out2
diff_files verilog_bus_ps_pwr.vok $out2

# With remove_cells (empty)
set out3 [make_result_file verilog_bus_ps_remove.v]
write_verilog -remove_cells {} $out3
diff_files verilog_bus_ps_remove.vok $out3

#---------------------------------------------------------------
# Test 4: Read back written verilog (roundtrip)
# Exercises: VerilogReader re-parsing bus declarations from writer output
#---------------------------------------------------------------
puts "--- Test 4: roundtrip ---"
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $out1
link_design verilog_bus_partselect

set cells2 [get_cells *]
puts "roundtrip cells: [llength $cells2]"

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {data_in* sel}]
set_output_delay -clock clk 0 [all_outputs]
set_input_transition 0.1 [get_ports {data_in* sel}]

report_checks

# Write again to see if sizes match
set out4 [make_result_file verilog_bus_ps_roundtrip2.v]
write_verilog $out4
diff_files verilog_bus_ps_roundtrip2.vok $out4

#---------------------------------------------------------------
# Test 5: Instance and net reports for bus design
#---------------------------------------------------------------
puts "--- Test 5: reports ---"

# Report bus nets
foreach net_name {buf_out[0] buf_out[1] buf_out[7] inv_out[0] inv_out[7] mux_out[0] mux_out[7]} {
  report_net $net_name
  puts "report_net $net_name: done"
}

# Report instances in hierarchy
foreach inst_name {buf0 buf7 inv0 inv3 reg0 reg7 or01 mux_lo0} {
  report_instance $inst_name
  puts "report_instance $inst_name: done"
}

#---------------------------------------------------------------
# Test 6: Hierarchical queries
#---------------------------------------------------------------
puts "--- Test 6: hierarchical queries ---"

# Query through hierarchical path
report_checks -through [get_pins buf0/Z]
puts "through buf0/Z: done"

report_checks -through [get_pins inv0/ZN]
puts "through inv0/ZN: done"

report_checks -through [get_pins mux_lo0/ZN]
puts "through mux_lo0/ZN: done"

#---------------------------------------------------------------
# Test 7: Network modification in bus design
#---------------------------------------------------------------
puts "--- Test 7: modify bus design ---"

# Add an extra buffer on a bus bit
set nn [make_net "extra_bus_wire"]
set ni [make_instance "extra_buf_bus" NangateOpenCellLibrary/BUF_X2]
connect_pin extra_bus_wire extra_buf_bus/A

set out5 [make_result_file verilog_bus_ps_modified.v]
write_verilog $out5
diff_files verilog_bus_ps_modified.vok $out5

disconnect_pin extra_bus_wire extra_buf_bus/A
delete_instance extra_buf_bus
delete_net extra_bus_wire
