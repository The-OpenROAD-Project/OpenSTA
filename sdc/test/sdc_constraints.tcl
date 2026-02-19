# Test SDC constraint commands
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test1.v
link_design sdc_test1

# Create clock
create_clock -name clk -period 10 [get_ports clk]

# Set input delay
set_input_delay -clock clk 2.0 [get_ports in1]

# Set output delay
set_output_delay -clock clk 3.0 [get_ports out1]

# Report clock properties
report_clock_properties

# Report checks
report_checks
