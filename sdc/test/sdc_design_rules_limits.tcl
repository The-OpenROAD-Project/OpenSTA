# Test design rules, limits, and WriteSdc.cc design rules paths.
# Targets:
#   Sdc.cc: setSlewLimit (port, cell, clock with clk_data/rf),
#     setCapacitanceLimit (port, pin, cell),
#     setFanoutLimit (port, cell), setMaxArea,
#     slewLimit, capacitanceLimit, fanoutLimit,
#     setMinPulseWidth (global, pin, clock, instance),
#     setLatchBorrowLimit (pin, instance, clock)
#   WriteSdc.cc: writeDesignRules, writeMinPulseWidths,
#     writeLatchBorowLimits, writeSlewLimits, writeClkSlewLimits,
#     writeClkSlewLimit, writeCapLimits (min/max with port/pin),
#     writeFanoutLimits (min/max with port), writeMaxArea,
#     writeMinPulseWidth (hi/low variants)
#   Clock.cc: slewLimit with PathClkOrData
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

############################################################
# Max/min transition limits on design, ports, and clocks
############################################################
set_max_transition 0.5 [current_design]

set_max_transition 0.3 [get_ports out1]
set_max_transition 0.35 [get_ports out2]

# Clock-specific slew limits (exercises writeClkSlewLimits)
set_max_transition -clock_path 0.2 [get_clocks clk1]
set_max_transition -data_path 0.4 [get_clocks clk1]

# Per-rise/fall on clock data path
set_max_transition -clock_path -rise 0.18 [get_clocks clk2]
set_max_transition -clock_path -fall 0.22 [get_clocks clk2]
set_max_transition -data_path -rise 0.38 [get_clocks clk2]
set_max_transition -data_path -fall 0.42 [get_clocks clk2]

############################################################
# Max/min capacitance limits
############################################################
set_max_capacitance 0.2 [current_design]

set_max_capacitance 0.1 [get_ports out1]
set_max_capacitance 0.15 [get_ports out2]

# Pin-level cap limits
set_max_capacitance 0.08 [get_pins reg1/Q]

# Min capacitance
set_min_capacitance 0.001 [current_design]

set_min_capacitance 0.0005 [get_ports out1]

############################################################
# Max/min fanout limits
############################################################
set_max_fanout 20 [current_design]

set_max_fanout 10 [get_ports in1]
set_max_fanout 15 [get_ports in2]

############################################################
# Max area
############################################################
set_max_area 500.0

############################################################
# Min pulse width on various targets
############################################################

# Global min pulse width
set_min_pulse_width 0.5

# Clock min pulse width with high/low
set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]

# Same value for high and low (exercises equal path)
set_min_pulse_width 0.7 [get_clocks clk2]

# Pin min pulse width
set_min_pulse_width 0.3 [get_pins reg1/CK]
set_min_pulse_width -high 0.35 [get_pins reg2/CK]
set_min_pulse_width -low 0.25 [get_pins reg2/CK]

# Instance min pulse width
set_min_pulse_width 0.45 [get_cells reg3]

############################################################
# Latch borrow limits
############################################################
set_max_time_borrow 2.0 [get_clocks clk1]
set_max_time_borrow 1.5 [get_clocks clk2]

set_max_time_borrow 1.0 [get_pins reg1/D]

set_max_time_borrow 1.2 [get_cells reg2]

############################################################
# Port slew limits
############################################################
set_max_transition 0.25 [get_ports in1]
set_max_transition 0.28 [get_ports in2]

############################################################
# Write SDC (exercises all design rule writing paths)
############################################################
set sdc1 [make_result_file sdc_design_rules1.sdc]
write_sdc -no_timestamp $sdc1

set sdc2 [make_result_file sdc_design_rules2.sdc]
write_sdc -no_timestamp -compatible $sdc2

set sdc3 [make_result_file sdc_design_rules3.sdc]
write_sdc -no_timestamp -digits 8 $sdc3

############################################################
# Read back and verify
############################################################
read_sdc $sdc1
report_checks

############################################################
# Check reporting
############################################################
report_check_types -max_slew -max_capacitance -max_fanout

report_check_types -min_pulse_width -min_period

############################################################
# Final write after read
############################################################
set sdc4 [make_result_file sdc_design_rules4.sdc]
write_sdc -no_timestamp $sdc4
