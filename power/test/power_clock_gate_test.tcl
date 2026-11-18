# Clock gate test enable (clock_gate_test_pin) enables the gated clock
# with the enable: GCLK duty = clk duty x P(GATE | SCE). Clock period 1ns,
# so 2e9 is two transitions per cycle.
source ../../test/helpers.tcl
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set verilog [make_result_file power_clock_gate_test.v]
set stream [open $verilog w]
puts $stream {module top (clk, g, te, x, q0, q1);
  input clk, g, te, x;
  output q0, q1;
  wire gclk0, gclk1;
  sky130_fd_sc_hd__sdlclkp_1 tied (.CLK(clk), .GATE(g), .SCE(1'b0), .GCLK(gclk0));
  sky130_fd_sc_hd__dfxtp_1 r0 (.CLK(gclk0), .D(x), .Q(q0));
  sky130_fd_sc_hd__sdlclkp_1 free (.CLK(clk), .GATE(g), .SCE(te), .GCLK(gclk1));
  sky130_fd_sc_hd__dfxtp_1 r1 (.CLK(gclk1), .D(x), .Q(q1));
endmodule}
close $stream
read_verilog $verilog
link_design top

create_clock -name clk -period 1 [get_ports clk]
set_input_delay 0 -clock clk [get_ports {g te x}]
set_power_activity -input_ports [get_ports {g te x}] -activity 0.1 -duty 0.5

proc show { pin } {
  lassign [get_property [get_pins $pin] activity] density duty origin
  puts [format "%-10s transitions/cycle %.3f duty %.3f %s" \
          $pin [expr {$density * 1e-9}] $duty $origin]
}
puts "Test enable tied low, and on a port (P(GATE | SCE) = 0.75)"
show tied/GCLK
show free/GCLK
puts "Test enable held low by case analysis"
set_case_analysis 0 [get_ports te]
show free/GCLK
puts "Enable held low, test enable on a port: the clock still reaches r1"
unset_case_analysis [get_ports te]
set_case_analysis 0 [get_ports g]
show free/GCLK
show tied/GCLK
