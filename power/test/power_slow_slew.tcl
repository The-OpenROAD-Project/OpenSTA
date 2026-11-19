# Activities on nets with slow slews are not limited by 1/slew.
# An unbuffered clock gate output (before clock tree synthesis) keeps
# clock density x enable duty, and annotations keep their values.
# Clock period 1ns, so 2e9 is two transitions per cycle.
source ../../test/helpers.tcl
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set verilog [make_result_file power_slow_slew.v]
set stream [open $verilog w]
puts $stream {module top (clk, g, x, q, y);
  input clk, g, x;
  output q, y;
  wire gclk, xb;
  sky130_fd_sc_hd__dlclkp_1 icg (.CLK(clk), .GATE(g), .GCLK(gclk));
  sky130_fd_sc_hd__dfxtp_1 r (.CLK(gclk), .D(x), .Q(q));
  sky130_fd_sc_hd__buf_1 b (.A(x), .X(xb));
  sky130_fd_sc_hd__buf_1 b2 (.A(xb), .X(y));
endmodule}
close $stream
read_verilog $verilog
link_design top

create_clock -name clk -period 1 [get_ports clk]
set_input_delay 0 -clock clk [get_ports {g x}]
set_input_transition 0.05 [all_inputs]
# Slow nets: large loads on the gated clock and on xb.
set_load 0.5 [get_nets {gclk xb}]
set_power_activity -input_ports [get_ports g] -activity 0.5 -duty 0.5
set_power_activity -input_ports [get_ports x] -activity 0.5 -duty 0.5
set_power_activity -pins [get_pins b/X] -activity 0.9 -duty 0.5

proc show { pin } {
  lassign [get_property [get_pins $pin] activity] density duty origin
  puts [format "%-8s transitions/cycle %.3f duty %.3f %s" \
          $pin [expr {$density * 1e-9}] $duty $origin]
}
foreach pin {icg/GCLK r/CLK b/X b2/A} {
  set slew [get_property [get_pins $pin] slew_max]
  puts [format "%-8s slew %.2fns" $pin $slew]
}
foreach pin {icg/GCLK r/CLK b/X b2/A} {
  show $pin
}
