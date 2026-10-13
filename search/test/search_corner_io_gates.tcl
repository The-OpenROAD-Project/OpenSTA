# IO delays that exist only in a corner overlay must reach the engine's
# gates: a corner-only set_output_delay creates a path endpoint, a
# corner-only set_input_delay on an internal pin seeds a segment start,
# and a bundle redefine with data empties the corner's overlays.
source ../../test/helpers.tcl
read_liberty ../../test/nangate45/Nangate45_slow.lib
read_verilog search_crpr.v
link_design search_crpr

create_clock -name clk -period 10 [get_ports clk]
set_input_delay 1.0 -clock clk [get_ports in1]
set_input_delay 1.0 -clock clk [get_ports in2]
# The mode has no set_output_delay on out1 and no internal input delays.

set io_sdc [make_result_file corner_io_gates.sdc]
set stream [open $io_sdc "w"]
puts $stream {set_output_delay 2.0 -clock clk [get_ports out1]}
puts $stream {set_input_delay 1.5 -clock clk [get_pins buf1/Z]}
close $stream
define_analysis_corner ssc -liberty NangateOpenCellLibrary_slow -sdc [list $io_sdc]
define_scene ss -analysis_corner ssc

puts "=== corner-only output delay endpoint ==="
report_checks -to [get_ports out1] -format end
puts "=== corner-only internal input delay segment start ==="
report_checks -from [get_pins buf1/Z] -format end

# Redefine with data replaces the bundle and empties the overlay: the
# corner-only constraints are gone.
define_analysis_corner ssc -liberty NangateOpenCellLibrary_slow
puts "=== after redefine with data ==="
report_checks -to [get_ports out1] -format end
report_checks -from [get_pins buf1/Z] -format end

# Overlay references die with their objects: delete_clock purges the
# corner's constraints referencing the clock, so in2 falls back to the
# mode's clk delay.
# Deleting a pin or instance that an overlay constrains is not covered
# here. Deleting SDC constrained objects is not supported by OpenSTA at
# all (the caller is expected to check Sdc::isConstrained first), so
# there is no purge behavior to assert.
create_clock -name vclk -period 10
set_cmd_analysis_corner ssc
set_input_delay 2.0 -clock vclk [get_ports in2]
set_clock_latency -source 0.4 [get_clocks vclk]
unset_cmd_analysis_corner
puts "=== corner delay on in2 wrt vclk ==="
report_checks -from [get_ports in2] -format end
delete_clock vclk
puts "=== after delete_clock vclk: falls back to mode delay ==="
report_checks -from [get_ports in2] -format end

# A clock also dies when a new clock claims its last pin, which does not
# go through delete_clock: the corner's references must be purged there
# too, and the new clock must not inherit the dead clock's corner data.
create_clock -name clk2 -period 10 [get_ports clk]
set_cmd_analysis_corner ssc
set_clock_uncertainty 0.9 [get_clocks clk2]
set_input_delay 3.0 -clock clk2 [get_ports in1]
unset_cmd_analysis_corner
# clk3 claims clk2's only pin, so clk2 dies without delete_clock. The
# corner's uncertainty and overlay delay for clk2 must not carry over to
# clk3: only the mode's 1.0 input delay applies below.
create_clock -name clk3 -period 10 [get_ports clk]
set_input_delay 1.0 -clock clk3 [get_ports in1]
puts "=== after clk3 replaced clk2 on the same pin ==="
report_checks -format end
