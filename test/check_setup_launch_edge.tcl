# The clock network is found without regard to transitions. Only the
# falling edge of clk reaches r/CLK, through b, and r launches on the
# rising edge, so no path through r is timed: check_setup must report
# r/CLK as an unclocked register pin. f (a falling-edge flop behind
# the same cell) and c (clocked by clk directly) are clocked and must
# not be reported.
read_liberty asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty check_setup_launch_edge.lib
read_verilog check_setup_launch_edge.v
link_design top
create_clock -name clk -period 1000 [get_ports clk]

# no_input_delay no_output_delay multiple_clock no_clock
# unconstrained_endpoints loops generated_clocks
set unclocked {}
foreach error [sta::check_timing_cmd 0 0 0 1 0 0 0] {
  if { [string match "*unclocked register*" [lindex $error 0]] } {
    set unclocked [lrange $error 1 end]
  }
}
set failed 0
if { [lsearch -exact $unclocked r/CLK] < 0 } {
  puts "FAIL: r/CLK, which no clock reaches on its launch edge, is not reported"
  set failed 1
}
foreach pin {f/CLK c/CLK} {
  if { [lsearch -exact $unclocked $pin] >= 0 } {
    puts "FAIL: $pin is clocked but reported unclocked"
    set failed 1
  }
}
exit $failed
