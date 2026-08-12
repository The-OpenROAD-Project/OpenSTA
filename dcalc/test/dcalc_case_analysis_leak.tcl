# Delay calculation must not merge slew from set_case_analysis constants.
#
# Regression guard for "dcalc no slew merge for constant disabled arcs".
#
# GraphDelayCalc uses DcalcPred (dcalc/GraphDelayCalc.cc).  Its searchFrom
# used to stop only at power/ground nets:
#
#   return !(sdc->isDisabledConstraint(from_pin)
#            || (net && (network->isPower(net) || network->isGround(net))));
#
# SearchPred0::searchFrom (search/SearchPred.cc) stops at
# sim->isConstant(from_vertex); DcalcPred did not, so in
# findDriverEdgeDelays an arc whose source pin is an SDC constant still
# passed searchFrom/searchThru and had its delay and slew merged into the
# driver vertex -- including when that driver is a live, non-constant pin.
#
# The fix adds sim->isConstant(from_vertex) to DcalcPred::searchFrom, and
# makes Sta::delayCalcPreamble propagate constants before delay calculation
# so the predicate sees them.
#
# Circuit (dcalc_case_analysis_leak.v):
#
#   c --BUF u0--> cn ---A2\
#                          AND2 u1 --> n1 --BUF u2--> z
#   a -------------------A1/
#
# set_case_analysis 1 c makes net cn constant.  1 is non-controlling for an
# AND, so n1 and z stay live and the A2->ZN arc is dead.  The reported path is
# a -> u1 -> u2 -> z, which never traverses cn.
#
# Sweeping the wire load on cn must not change anything on that path.  Before
# the fix it did: cn's slew rode the dead A2->ZN arc into n1, and n1's slew
# set u2's delay, so the arrival spread was 46.38 ps.  The arrival column
# below must now be flat.

read_liberty ../../examples/nangate45_slow.lib.gz
read_verilog dcalc_case_analysis_leak.v
link_design top

create_clock -name clk -period 10
set_input_delay 0 -clock clk [get_ports {a c}]
set_output_delay 0 -clock clk [get_ports z]
set_input_transition 0.010 [get_ports {a c}]

set_case_analysis 1 [get_ports c]

puts ""
puts "load on constant net cn (ff)    arrival at z (ps)"
set arrivals {}
foreach load {0 50 200 500 1000} {
  set_load $load [get_nets cn]
  set path [lindex [find_timing_paths -path_delay max] 0]
  set arrival [format %.2f [expr [$path data_arrival_time] * 1e12]]
  lappend arrivals $arrival
  puts [format "%-32s%s" $load $arrival]
}

set delta [expr [lindex $arrivals end] - [lindex $arrivals 0]]
puts ""
puts "arrival spread across the sweep: [format %.2f $delta] ps (expected 0.00)"
puts ""
