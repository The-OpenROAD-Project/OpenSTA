# Test input/output delays with -source_latency_included, -network_latency_included,
# latch borrow limits on pin/instance/clock, min pulse width on all targets,
# group_path -default with from/through/to, setMaxArea, unset operations,
# and write_sdc roundtrip for all of these.
# Targets:
#   Sdc.cc: setInputDelay/setOutputDelay with source_latency_included,
#     network_latency_included, add_delay+clock_fall combos,
#     setLatchBorrowLimit (pin, instance, clock), latchBorrowLimit lookup,
#     setMinPulseWidth (global, pin, instance, clock),
#     makeGroupPath (named, default, with through),
#     setMaxArea, maxArea, removePropagatedClock (clock, pin),
#     removeInputDelay, removeOutputDelay,
#     clockGroupsAreSame (via set_clock_groups -logically_exclusive),
#     unsetTimingDerate, setMaxTimeBorrow
#   WriteSdc.cc: writePortDelay (all 4-way compression paths),
#     writeLatchBorowLimits (pin, inst, clk),
#     writeMinPulseWidths (high/low equal/different),
#     writeMaxArea, writeGroupPaths (default + named w/through),
#     writeExceptions (group_path default)
#   ExceptionPath.cc: GroupPath constructor, isDefault, overrides,
#     fromThruToPriority with through
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

############################################################
# Setup clocks
############################################################
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 -waveform {0 10} [get_ports clk2]
create_clock -name vclk -period 8

############################################################
# Input delays with -source_latency_included
############################################################
set_input_delay -clock clk1 -source_latency_included 2.0 [get_ports in1]

set_input_delay -clock clk1 -network_latency_included 1.8 [get_ports in2]

set_input_delay -clock clk2 -source_latency_included -network_latency_included 1.5 [get_ports in3]

# Add delay on top with clock_fall and source_latency_included
set_input_delay -clock clk1 -clock_fall -source_latency_included -add_delay 2.2 [get_ports in1]

# Rise/fall with source latency on a different port
set_input_delay -clock clk1 -rise -max -source_latency_included 3.0 [get_ports in2] -add_delay
set_input_delay -clock clk1 -fall -min -network_latency_included 0.5 [get_ports in2] -add_delay

############################################################
# Output delays with -source_latency_included
############################################################
set_output_delay -clock clk1 -source_latency_included 3.0 [get_ports out1]

set_output_delay -clock clk2 -network_latency_included 2.5 [get_ports out2]

set_output_delay -clock clk1 -clock_fall -source_latency_included -add_delay 3.2 [get_ports out1]

set_output_delay -clock clk2 -clock_fall -network_latency_included -add_delay 2.8 [get_ports out2]

# Rise/fall max/min output delays creating 4-way variant
set_output_delay -clock clk1 -rise -max 3.5 [get_ports out1] -add_delay
set_output_delay -clock clk1 -rise -min 1.5 [get_ports out1] -add_delay
set_output_delay -clock clk1 -fall -max 3.2 [get_ports out1] -add_delay
set_output_delay -clock clk1 -fall -min 1.2 [get_ports out1] -add_delay

############################################################
# Propagated clock + remove propagated clock
############################################################
set_propagated_clock [get_clocks clk1]

# Setting clock latency removes propagated clock
set_clock_latency 0.3 [get_clocks clk1]

# Set propagated on pin, then set clock latency on that pin to remove
set_propagated_clock [get_ports clk2]

set_clock_latency 0.2 [get_ports clk2]

############################################################
# Latch borrow limits on all three target types
############################################################
set_max_time_borrow 2.0 [get_clocks clk1]
set_max_time_borrow 1.5 [get_clocks clk2]

set_max_time_borrow 1.0 [get_pins reg1/D]
set_max_time_borrow 0.8 [get_pins reg2/D]

catch {
  set_max_time_borrow 1.2 [get_cells reg1]
}

catch {
  set_max_time_borrow 0.9 [get_cells reg3]
}

############################################################
# Min pulse width on all targets
############################################################
# Global
set_min_pulse_width 0.5

# Clock with different high/low
set_min_pulse_width -high 0.6 [get_clocks clk1]
set_min_pulse_width -low 0.4 [get_clocks clk1]

# Clock with same high/low (exercises equal path in writer)
set_min_pulse_width 0.55 [get_clocks clk2]

# Pin
catch {
  set_min_pulse_width 0.3 [get_pins reg1/CK]
}

catch {
  set_min_pulse_width -high 0.35 [get_pins reg2/CK]
  set_min_pulse_width -low 0.25 [get_pins reg2/CK]
}

# Instance
catch {
  set_min_pulse_width 0.45 [get_cells reg3]
}

############################################################
# set_max_area
############################################################
set_max_area 250.0

############################################################
# Group paths - default and named with through
############################################################
group_path -default -from [get_ports in1] -to [get_ports out1]

group_path -name grp_thru -from [get_ports in2] \
  -through [get_pins and1/ZN] -to [get_ports out1]

group_path -name grp_clk -from [get_clocks clk1] -to [get_clocks clk2]

# Duplicate group path (same name, same from/to - exercises hasKey path)
group_path -name grp_clk -from [get_clocks clk1] -to [get_clocks clk2]

############################################################
# Clock groups - logically_exclusive (exercises clockGroupsAreSame)
############################################################
set_clock_groups -logically_exclusive -group {clk1} -group {clk2}

############################################################
# False paths and multicycle with -setup/-hold for exceptions
############################################################
set_false_path -setup -from [get_clocks clk1] -to [get_clocks clk2]

set_false_path -hold -from [get_clocks clk2] -to [get_clocks clk1]

# Multicycle with -start
set_multicycle_path -setup -start 3 -from [get_ports in2] -to [get_ports out1]

# Multicycle with -end for hold
set_multicycle_path -hold -end 1 -from [get_ports in2] -to [get_ports out1]

############################################################
# Max/min delay with -ignore_clock_latency
############################################################
set_max_delay -from [get_ports in3] -to [get_ports out2] -ignore_clock_latency 7.0

set_min_delay -from [get_ports in3] -to [get_ports out2] 0.5

############################################################
# Min fanout limit (covers setMinFanout through set_min_fanout if available)
############################################################
catch {
  set_min_fanout 2 [current_design]
}

############################################################
# Write SDC
############################################################
set sdc1 [make_result_file sdc_delay_borrow_group1.sdc]
write_sdc -no_timestamp $sdc1

set sdc2 [make_result_file sdc_delay_borrow_group2.sdc]
write_sdc -no_timestamp -compatible $sdc2

set sdc3 [make_result_file sdc_delay_borrow_group3.sdc]
write_sdc -no_timestamp -digits 8 $sdc3

############################################################
# Remove some constraints and re-write
############################################################
unset_input_delay -clock clk1 [get_ports in1]

unset_output_delay -clock clk1 [get_ports out1]

# Unset path exceptions
unset_path_exceptions -setup -from [get_clocks clk1] -to [get_clocks clk2]
unset_path_exceptions -hold -from [get_clocks clk2] -to [get_clocks clk1]

############################################################
# Read back SDC and report
############################################################
read_sdc $sdc1
report_checks

############################################################
# Re-write after read
############################################################
set sdc4 [make_result_file sdc_delay_borrow_group4.sdc]
write_sdc -no_timestamp $sdc4
