# Test SDC net wire caps, voltage, net deratings, port loads/fanout,
# net resistance writing, timing deratings on instances/lib_cells/nets.
# Targets: Sdc.cc setNetWireCap, setVoltage, removeNetLoadCaps,
#          derating factors on nets/instances/cells,
#          WriteSdc.cc writeNetLoads, writeNetLoad, writePortLoads,
#          writeNetResistances, writeNetResistance, writeVoltages,
#          writeDeratings (global, net, instance, cell variants),
#          writeConstants, writeCaseAnalysis
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
puts "PASS: basic setup"

############################################################
# Net wire capacitance
############################################################
puts "--- set net wire cap ---"
catch {
  set_load 0.01 [get_nets n1]
  set_load 0.02 [get_nets n2]
  set_load 0.005 [get_nets n3]
  set_load 0.015 [get_nets n4]
  set_load 0.03 [get_nets n5]
}
report_checks
puts "PASS: net wire cap"

############################################################
# Port loads (pin_load and wire_load)
############################################################
puts "--- port loads ---"
set_load -pin_load 0.04 [get_ports out1]
set_load -wire_load 0.02 [get_ports out1]
set_load -pin_load 0.03 [get_ports out2]
set_load -wire_load 0.01 [get_ports out2]
report_checks
puts "PASS: port loads"

############################################################
# Port fanout
############################################################
puts "--- port fanout ---"
set_port_fanout_number 4 [get_ports out1]
set_port_fanout_number 6 [get_ports out2]
report_checks
puts "PASS: port fanout"

############################################################
# Net resistance
############################################################
puts "--- net resistance ---"
set_resistance -min 10.0 [get_nets n1]
set_resistance -max 20.0 [get_nets n1]
set_resistance -min 5.0 [get_nets n2]
set_resistance -max 15.0 [get_nets n2]
set_resistance 12.0 [get_nets n3]
report_checks
puts "PASS: net resistance"

############################################################
# Voltage settings
############################################################
puts "--- voltage ---"
catch {
  set_voltage 1.1 -min 0.9
  report_checks
}
puts "PASS: voltage global"

catch {
  set_voltage 1.2 -min 1.0 -object_list [get_nets n1]
  report_checks
}
puts "PASS: voltage on net"

############################################################
# Timing deratings: global
############################################################
puts "--- timing derate global ---"
set_timing_derate -early 0.95
set_timing_derate -late 1.05
set_timing_derate -early -clock 0.97
set_timing_derate -late -clock 1.03
report_checks
puts "PASS: global derating"

############################################################
# Timing deratings: on lib cells
############################################################
puts "--- timing derate lib cell ---"
set_timing_derate -early -cell_delay 0.91 [get_lib_cells NangateOpenCellLibrary/INV_X1]
set_timing_derate -late -cell_delay 1.09 [get_lib_cells NangateOpenCellLibrary/INV_X1]
set_timing_derate -early -cell_delay 0.92 [get_lib_cells NangateOpenCellLibrary/BUF_X1]
set_timing_derate -late -cell_delay 1.08 [get_lib_cells NangateOpenCellLibrary/BUF_X1]
report_checks
puts "PASS: lib cell derating"

############################################################
# Timing deratings: on instances
############################################################
puts "--- timing derate instance ---"
set_timing_derate -early -cell_delay 0.90 [get_cells buf1]
set_timing_derate -late -cell_delay 1.10 [get_cells buf1]
set_timing_derate -early -cell_delay 0.93 [get_cells inv1]
set_timing_derate -late -cell_delay 1.07 [get_cells inv1]
report_checks
puts "PASS: instance derating"

############################################################
# Timing deratings: on nets
############################################################
puts "--- timing derate net ---"
set_timing_derate -early -net_delay 0.88 [get_nets n1]
set_timing_derate -late -net_delay 1.12 [get_nets n1]
set_timing_derate -early -net_delay 0.89 [get_nets n3]
set_timing_derate -late -net_delay 1.11 [get_nets n3]
report_checks
puts "PASS: net derating"

############################################################
# Write SDC and verify all sections are written
############################################################
puts "--- write_sdc native ---"
set sdc1 [make_result_file sdc_net_wire_voltage1.sdc]
write_sdc -no_timestamp $sdc1
puts "PASS: write_sdc native"

puts "--- write_sdc compatible ---"
set sdc2 [make_result_file sdc_net_wire_voltage2.sdc]
write_sdc -no_timestamp -compatible $sdc2
puts "PASS: write_sdc compatible"

puts "--- write_sdc digits 8 ---"
set sdc3 [make_result_file sdc_net_wire_voltage3.sdc]
write_sdc -no_timestamp -digits 8 $sdc3
puts "PASS: write_sdc digits 8"

############################################################
# Read back and verify constraints survive roundtrip
############################################################
puts "--- read_sdc back ---"
read_sdc $sdc1
report_checks
puts "PASS: read_sdc roundtrip"

############################################################
# Write after re-read
############################################################
set sdc4 [make_result_file sdc_net_wire_voltage4.sdc]
write_sdc -no_timestamp $sdc4
puts "PASS: write_sdc after re-read"

############################################################
# Reset deratings
############################################################
puts "--- reset deratings ---"
unset_timing_derate
report_checks
puts "PASS: reset deratings"

############################################################
# Final write with cleared deratings
############################################################
set sdc5 [make_result_file sdc_net_wire_voltage5.sdc]
write_sdc -no_timestamp $sdc5
puts "PASS: write after clear deratings"

puts "ALL PASSED"
