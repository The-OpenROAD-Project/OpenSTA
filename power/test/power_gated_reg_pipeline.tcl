# Vectorless activity of register chains behind one clock gate or one
# enable. The data of each stage is launched by the previous stage at the
# enabled edges, so every change is seen and the stages keep the activity
# of the first (0.05 per cycle with the enable at 0.1). With an xor from a
# port between the stages the data mixes changes from the chain and from
# the port; it is taken as synchronous, which errs high (at most the enable
# rate 0.1). Clock period 1ns.
source ../../test/helpers.tcl
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set verilog [make_result_file power_gated_reg_pipeline.v]
set stream [open $verilog w]
puts $stream {module top (clk, x, en, s, q, g, h);
  input clk, x, en, s;
  output q, g, h;
  wire gclk;
  wire [6:0] p; wire [6:0] r; wire [6:0] c; wire [5:0] cx;
  assign p[0] = x; assign q = p[6];
  assign r[0] = x; assign g = r[6];
  assign c[0] = x; assign h = c[6];
  sky130_fd_sc_hd__edfxtp_1 e0 (.CLK(clk), .D(p[0]), .DE(en), .Q(p[1]));
  sky130_fd_sc_hd__edfxtp_1 e1 (.CLK(clk), .D(p[1]), .DE(en), .Q(p[2]));
  sky130_fd_sc_hd__edfxtp_1 e2 (.CLK(clk), .D(p[2]), .DE(en), .Q(p[3]));
  sky130_fd_sc_hd__edfxtp_1 e3 (.CLK(clk), .D(p[3]), .DE(en), .Q(p[4]));
  sky130_fd_sc_hd__edfxtp_1 e4 (.CLK(clk), .D(p[4]), .DE(en), .Q(p[5]));
  sky130_fd_sc_hd__edfxtp_1 e5 (.CLK(clk), .D(p[5]), .DE(en), .Q(p[6]));
  sky130_fd_sc_hd__dlclkp_1 icg (.CLK(clk), .GATE(en), .GCLK(gclk));
  sky130_fd_sc_hd__dfxtp_1 g0 (.CLK(gclk), .D(r[0]), .Q(r[1]));
  sky130_fd_sc_hd__dfxtp_1 g1 (.CLK(gclk), .D(r[1]), .Q(r[2]));
  sky130_fd_sc_hd__dfxtp_1 g2 (.CLK(gclk), .D(r[2]), .Q(r[3]));
  sky130_fd_sc_hd__dfxtp_1 g3 (.CLK(gclk), .D(r[3]), .Q(r[4]));
  sky130_fd_sc_hd__dfxtp_1 g4 (.CLK(gclk), .D(r[4]), .Q(r[5]));
  sky130_fd_sc_hd__dfxtp_1 g5 (.CLK(gclk), .D(r[5]), .Q(r[6]));
  sky130_fd_sc_hd__dfxtp_1 c0 (.CLK(gclk), .D(c[0]), .Q(c[1]));
  sky130_fd_sc_hd__xor2_1 x0 (.A(c[1]), .B(s), .X(cx[0]));
  sky130_fd_sc_hd__dfxtp_1 c1 (.CLK(gclk), .D(cx[0]), .Q(c[2]));
  sky130_fd_sc_hd__xor2_1 x1 (.A(c[2]), .B(s), .X(cx[1]));
  sky130_fd_sc_hd__dfxtp_1 c2 (.CLK(gclk), .D(cx[1]), .Q(c[3]));
  sky130_fd_sc_hd__xor2_1 x2 (.A(c[3]), .B(s), .X(cx[2]));
  sky130_fd_sc_hd__dfxtp_1 c3 (.CLK(gclk), .D(cx[2]), .Q(c[4]));
  sky130_fd_sc_hd__xor2_1 x3 (.A(c[4]), .B(s), .X(cx[3]));
  sky130_fd_sc_hd__dfxtp_1 c4 (.CLK(gclk), .D(cx[3]), .Q(c[5]));
  sky130_fd_sc_hd__xor2_1 x4 (.A(c[5]), .B(s), .X(cx[4]));
  sky130_fd_sc_hd__dfxtp_1 c5 (.CLK(gclk), .D(cx[4]), .Q(c[6]));
endmodule}
close $stream
read_verilog $verilog
link_design top

set_cmd_units -time ns
create_clock -name clk -period 1 [get_ports clk]

proc show { pin } {
  lassign [get_property [get_pins $pin] activity] density duty origin
  puts [format "%-6s toggles/cycle %.4f duty %.3f %s" \
          $pin [expr {$density * 1e-9}] $duty $origin]
}
proc set_input { port duty toggles } {
  set_power_activity -input_ports [get_ports $port] -density $toggles \
    -duty $duty
}
set_input x 0.5 0.5
set_input en 0.1 0.18
set_input s 0.5 0.02
set pins {e0/Q e1/Q e2/Q e5/Q g0/Q g1/Q g2/Q g5/Q c0/Q c1/Q c2/Q c5/Q}
puts "Formulas."
foreach pin $pins { show $pin }
puts "Full simulation."
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
foreach pin $pins { show $pin }
