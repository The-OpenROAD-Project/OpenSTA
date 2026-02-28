# Test limit violation reporting in verbose mode, check_timing deeper,
# and various check types with specific flags.
# Targets: Sta.cc checkSlewLimits, reportSlewLimitVerbose/Short,
#          checkFanoutLimits, reportFanoutLimitVerbose/Short,
#          checkCapacitanceLimits, reportCapacitanceLimitVerbose/Short,
#          maxSlewCheck, maxFanoutCheck, maxCapacitanceCheck,
#          checkSlew, checkFanout, checkCapacitance,
#          CheckSlewLimits.cc, CheckFanoutLimits.cc, CheckCapacitanceLimits.cc,
#          CheckTiming.cc checkTiming deeper (individual flags),
#          ReportPath.cc reportSlewLimitShortHeader, reportFanoutLimitShortHeader,
#          reportCapacitanceLimitShortHeader
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

report_checks > /dev/null

############################################################
# Slew limit checks
############################################################
puts "--- set_max_transition ---"
set_max_transition 0.01 [current_design]

puts "--- report_check_types -max_slew ---"
report_check_types -max_slew

puts "--- report_check_types -max_slew -verbose ---"
report_check_types -max_slew -verbose

puts "--- report_check_types -max_slew -violators ---"
report_check_types -max_slew -violators

puts "--- report_check_types -max_slew -violators -verbose ---"
report_check_types -max_slew -violators -verbose

puts "--- max_slew_violation_count ---"
set svc [sta::max_slew_violation_count]
puts "max slew violations: $svc"

puts "--- max_slew_check_slack ---"
set ss [sta::max_slew_check_slack]
set sl [sta::max_slew_check_limit]
puts "max slew slack: $ss limit: $sl"

############################################################
# Fanout limit checks
############################################################
puts "--- set_max_fanout ---"
set_max_fanout 2 [current_design]

puts "--- report_check_types -max_fanout ---"
report_check_types -max_fanout

puts "--- report_check_types -max_fanout -verbose ---"
report_check_types -max_fanout -verbose

puts "--- report_check_types -max_fanout -violators ---"
report_check_types -max_fanout -violators

puts "--- report_check_types -max_fanout -violators -verbose ---"
report_check_types -max_fanout -violators -verbose

puts "--- max_fanout_violation_count ---"
set fvc [sta::max_fanout_violation_count]
puts "max fanout violations: $fvc"

puts "--- max_fanout_check_slack ---"
set fs [sta::max_fanout_check_slack]
set fl [sta::max_fanout_check_limit]
puts "max fanout slack: $fs limit: $fl"

############################################################
# Capacitance limit checks
############################################################
puts "--- set_max_capacitance ---"
set_max_capacitance 0.001 [current_design]

puts "--- report_check_types -max_capacitance ---"
report_check_types -max_capacitance

puts "--- report_check_types -max_capacitance -verbose ---"
report_check_types -max_capacitance -verbose

puts "--- report_check_types -max_capacitance -violators ---"
report_check_types -max_capacitance -violators

puts "--- report_check_types -max_capacitance -violators -verbose ---"
report_check_types -max_capacitance -violators -verbose

puts "--- max_capacitance_violation_count ---"
set cvc [sta::max_capacitance_violation_count]
puts "max cap violations: $cvc"

puts "--- max_capacitance_check_slack ---"
set cs [sta::max_capacitance_check_slack]
set cl [sta::max_capacitance_check_limit]
puts "max cap slack: $cs limit: $cl"

############################################################
# All check types together
############################################################
puts "--- report_check_types (all) ---"
report_check_types

puts "--- report_check_types -verbose ---"
report_check_types -verbose

puts "--- report_check_types -violators ---"
report_check_types -violators

puts "--- report_check_types -violators -verbose ---"
report_check_types -violators -verbose

############################################################
# Min slew checks
############################################################
puts "--- report_check_types -min_slew ---"
report_check_types -min_slew

puts "--- report_check_types -min_slew -verbose ---"
report_check_types -min_slew -verbose

############################################################
# Min fanout / min capacitance
############################################################
puts "--- report_check_types -min_fanout ---"
report_check_types -min_fanout

puts "--- report_check_types -min_capacitance ---"
report_check_types -min_capacitance

############################################################
# check_slew_limits for specific net
############################################################
puts "--- check_slew_limits specific net ---"
# TODO: sta::check_slew_limits removed from SWIG interface; use report_slew_checks
# set net [get_nets n7]
# set slew_pins [sta::check_slew_limits $net 0 "NULL" max]
# puts "slew limit pins for n7: [llength $slew_pins]"
# foreach p $slew_pins {
#   sta::report_slew_limit_short_header
#   sta::report_slew_limit_short $p "NULL" max
#   sta::report_slew_limit_verbose $p "NULL" max
# }
puts "check_slew_limits: skipped (API removed)"

############################################################
# check_fanout_limits for specific net
############################################################
puts "--- check_fanout_limits specific net ---"
# TODO: sta::check_fanout_limits removed from SWIG interface; use report_fanout_checks
# set fan_pins [sta::check_fanout_limits $net 0 max]
# puts "fanout limit pins for n7: [llength $fan_pins]"
# foreach p $fan_pins {
#   sta::report_fanout_limit_short_header
#   sta::report_fanout_limit_short $p max
#   sta::report_fanout_limit_verbose $p max
# }
puts "check_fanout_limits: skipped (API removed)"

############################################################
# check_capacitance_limits for specific net
############################################################
puts "--- check_capacitance_limits specific net ---"
# TODO: sta::check_capacitance_limits removed from SWIG interface; use report_capacitance_checks
# set cap_pins [sta::check_capacitance_limits $net 0 "NULL" max]
# puts "cap limit pins for n7: [llength $cap_pins]"
# foreach p $cap_pins {
#   sta::report_capacitance_limit_short_header
#   sta::report_capacitance_limit_short $p "NULL" max
#   sta::report_capacitance_limit_verbose $p "NULL" max
# }
puts "check_capacitance_limits: skipped (API removed)"

############################################################
# report_check_types with -format end
############################################################
puts "--- report_check_types -max_delay -format end ---"
report_check_types -max_delay -format end

puts "--- report_check_types -min_delay -format slack_only ---"
report_check_types -min_delay -format slack_only

############################################################
# check_setup individual flags
############################################################
puts "--- check_setup -no_input_delay ---"
check_setup -verbose -no_input_delay

puts "--- check_setup -no_output_delay ---"
check_setup -verbose -no_output_delay

puts "--- check_setup -no_clock ---"
check_setup -verbose -no_clock

puts "--- check_setup -multiple_clock ---"
check_setup -verbose -multiple_clock

puts "--- check_setup -unconstrained_endpoints ---"
check_setup -verbose -unconstrained_endpoints

puts "--- check_setup -loops ---"
check_setup -verbose -loops

############################################################
# Endpoint violation counts
############################################################
puts "--- endpoint_violation_count ---"
puts "max violations: [sta::endpoint_violation_count max]"
puts "min violations: [sta::endpoint_violation_count min]"
