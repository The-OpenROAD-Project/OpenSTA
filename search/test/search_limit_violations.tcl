# Test slew, capacitance, fanout limit checks with actual violations.
# Targets: Sta.cc checkSlewLimits, checkCapacitanceLimits, checkFanoutLimits,
#          reportSlewLimitShort/Verbose, reportCapacitanceLimitShort/Verbose,
#          reportFanoutLimitShort/Verbose, checkSlew, maxSlewCheck,
#          checkFanout, maxFanoutCheck, checkCapacitance, maxCapacitanceCheck,
#          CheckSlewLimits.cc, CheckCapacitanceLimits.cc, CheckFanoutLimits.cc,
#          CheckMinPulseWidths.cc, CheckMinPeriods.cc, CheckMaxSkews.cc
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_limit_violations.v
link_design search_limit_violations

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 1.0 [get_ports in3]
set_input_delay -clock clk 1.0 [get_ports in4]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

report_checks -path_delay max > /dev/null

puts "=== SLEW LIMIT CHECKS ==="

puts "--- set_max_transition tight limit ---"
set_max_transition 0.001 [current_design]
report_check_types -max_slew -verbose

puts "--- report_check_types -max_slew only ---"
report_check_types -max_slew

puts "--- report_check_types -max_slew -violators ---"
report_check_types -max_slew -violators

puts "--- set_max_transition on clock ---"
set_max_transition 0.002 -clock_path [get_clocks clk]
report_check_types -max_slew -verbose

puts "--- set_max_transition on port ---"
set_max_transition 0.003 [get_ports out1]
report_check_types -max_slew -verbose

puts "=== CAPACITANCE LIMIT CHECKS ==="

puts "--- set_max_capacitance tight limit ---"
set_max_capacitance 0.0001 [current_design]
report_check_types -max_capacitance -verbose

puts "--- report_check_types -max_capacitance only ---"
report_check_types -max_capacitance

puts "--- report_check_types -max_capacitance -violators ---"
report_check_types -max_capacitance -violators

puts "--- set_max_capacitance on port ---"
set_max_capacitance 0.0002 [get_ports out1]
report_check_types -max_capacitance -verbose

puts "=== FANOUT LIMIT CHECKS ==="

puts "--- set_max_fanout tight limit ---"
set_max_fanout 1 [current_design]
report_check_types -max_fanout -verbose

puts "--- report_check_types -max_fanout only ---"
report_check_types -max_fanout

puts "--- report_check_types -max_fanout -violators ---"
report_check_types -max_fanout -violators

puts "--- set_max_fanout on port ---"
set_max_fanout 2 [get_ports in1]
report_check_types -max_fanout -verbose

puts "=== PULSE WIDTH CHECKS ==="

puts "--- report_min_pulse_width_checks ---"
report_check_types -min_pulse_width

puts "--- report_min_pulse_width_checks -verbose ---"
report_check_types -min_pulse_width -verbose

puts "--- report_min_pulse_width_checks on specific pin ---"
# TODO: report_min_pulse_width_checks with pin arg removed (no Tcl wrapper)
# report_min_pulse_width_checks [get_pins reg1/CK]
# report_min_pulse_width_checks [get_pins reg2/CK]
puts "report_min_pulse_width_checks pin: skipped (API removed)"

puts "--- set_min_pulse_width ---"
set_min_pulse_width 5.0 [get_clocks clk]
report_check_types -min_pulse_width -verbose

set_min_pulse_width -high 4.0 [get_pins reg1/CK]
# TODO: report_min_pulse_width_checks with pin arg removed (no Tcl wrapper)
# report_min_pulse_width_checks [get_pins reg1/CK]
puts "report_min_pulse_width_checks pin: skipped (API removed)"

set_min_pulse_width -low 3.0 [get_cells reg1]
report_check_types -min_pulse_width -verbose

puts "--- report_check_types -min_pulse_width ---"
report_check_types -min_pulse_width
report_check_types -min_pulse_width -verbose

puts "=== MIN PERIOD CHECKS ==="

puts "--- report_clock_min_period ---"
report_clock_min_period

puts "--- report_clock_min_period -include_port_paths ---"
report_clock_min_period -include_port_paths

puts "--- report_clock_min_period -clocks ---"
report_clock_min_period -clocks clk

puts "--- Tight clock period for min_period violations ---"
create_clock -name clk -period 0.01 [get_ports clk]
report_check_types -min_period -verbose
report_check_types -min_period
report_check_types -min_period -violators

puts "--- report_clock_min_period with violation ---"
report_clock_min_period

puts "=== MAX SKEW CHECKS ==="

puts "--- report_check_types -max_skew ---"
report_check_types -max_skew
report_check_types -max_skew -verbose

puts "=== COMBINED CHECKS ==="

puts "--- report_check_types -violators (all) ---"
report_check_types -violators

puts "--- report_check_types verbose (all) ---"
report_check_types -verbose
