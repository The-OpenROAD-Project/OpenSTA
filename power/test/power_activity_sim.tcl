# Register activities from simulate_power_activity on loops through cells.
# Exact toggles per cycle: enable flop 0.045, toggle flop 0.05, hold-only
# flop 0, pipeline stage 2 0.045, clock gated flop 0.1. Clock period 6ns.
source ../../test/helpers.tcl
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set verilog [make_result_file power_activity_sim.v]
set stream [open $verilog w]
puts $stream {module top (clk, x, en, t, g, lg, rb, en_q, tg_q, hq_q, p1_q,
                          gf_q, lt_q, ar_q);
  input clk, x, en, t, g, lg, rb;
  output en_q, tg_q, hq_q, p1_q, gf_q, lt_q, ar_q;
  wire en_d, tg_d, p0_d, p0_q, p1_d, gclk;
  sky130_fd_sc_hd__mux2_1 en_mux (.A0(en_q), .A1(x), .S(en), .X(en_d));
  sky130_fd_sc_hd__dfxtp_1 en_r (.CLK(clk), .D(en_d), .Q(en_q));
  sky130_fd_sc_hd__xor2_1 tg_xor (.A(tg_q), .B(t), .X(tg_d));
  sky130_fd_sc_hd__dfxtp_1 tg_r (.CLK(clk), .D(tg_d), .Q(tg_q));
  sky130_fd_sc_hd__dfxtp_1 hq_r (.CLK(clk), .D(hq_q), .Q(hq_q));
  sky130_fd_sc_hd__mux2_1 p0_mux (.A0(p0_q), .A1(x), .S(en), .X(p0_d));
  sky130_fd_sc_hd__dfxtp_1 p0_r (.CLK(clk), .D(p0_d), .Q(p0_q));
  sky130_fd_sc_hd__mux2_1 p1_mux (.A0(p1_q), .A1(p0_q), .S(en), .X(p1_d));
  sky130_fd_sc_hd__dfxtp_1 p1_r (.CLK(clk), .D(p1_d), .Q(p1_q));
  sky130_fd_sc_hd__dlclkp_1 icg (.CLK(clk), .GATE(g), .GCLK(gclk));
  sky130_fd_sc_hd__dfxtp_1 gf_r (.CLK(gclk), .D(x), .Q(gf_q));
  sky130_fd_sc_hd__dlxtp_1 lt (.D(x), .GATE(lg), .Q(lt_q));
  sky130_fd_sc_hd__dfrtp_1 ar_r (.CLK(clk), .D(x), .RESET_B(rb), .Q(ar_q));
endmodule}
close $stream
read_verilog $verilog
link_design top

set_cmd_units -time ns
create_clock -name clk -period 6 [get_ports clk]
set_input_delay 0 -clock clk [delete_from_list [all_inputs] [get_ports clk]]

proc set_input { port duty toggles } {
  set_power_activity -input_ports [get_ports $port] \
    -density [expr {$toggles / 6.0}] -duty $duty
}
set_input x 0.5 0.5
set_input en 0.09 0.0225
set_input t 0.05 0.095
set_input g 0.2 0.1
set_input lg 0.5 0.2
set_input rb 0.9 0.05

proc show { pin } {
  lassign [get_property [get_pins $pin] activity] density duty origin
  puts [format "%-6s toggles/cycle %.3f duty %.3f %s" \
          $pin [expr {$density * 6e-9}] $duty $origin]
}

simulate_power_activity -cycles 4000 -warmup 500 -lanes 256 -seed 1
foreach pin {en_r/Q tg_r/Q hq_r/Q p1_r/Q gf_r/Q lt/Q ar_r/Q} {
  show $pin
}
report_activity_annotation
report_power

puts "Rerun with tg_r/Q set by set_power_activity: it keeps its activity."
set_power_activity -pins [get_pins tg_r/Q] -activity 0.2 -duty 0.5
simulate_power_activity -cycles 4000 -warmup 500 -lanes 256 -seed 1
foreach pin {en_r/Q tg_r/Q tg_xor/A} {
  show $pin
}
report_activity_annotation

puts "Simulation mode, then formulas (the default)."
unset_power_activity -pins [get_pins tg_r/Q]
set_power_activity_mode simulation
show en_r/Q
set_power_activity_mode vectorless
show en_r/Q

puts "Pin tied high in the netlist, and a latch + AND clock gate."
# sy_r is a reset synchronizer flop with D tied to 1'b1; its duty is near
# rb's. The latch + AND gated clock dg_and/X toggles 2 x 0.2 = 0.4 per cycle
# like icg/GCLK, not the 0.45 of the AND formula.
set verilog2 [make_result_file power_activity_sim2.v]
set stream [open $verilog2 w]
puts $stream {module top2 (clk, x, g, rb, sy_q, dg_q, gf_q);
  input clk, x, g, rb;
  output sy_q, dg_q, gf_q;
  wire g_l, dgclk, gclk;
  sky130_fd_sc_hd__dfrtp_1 sy_r (.CLK(clk), .D(1'b1), .RESET_B(rb), .Q(sy_q));
  sky130_fd_sc_hd__dlxtn_1 dg_lat (.GATE_N(clk), .D(g), .Q(g_l));
  sky130_fd_sc_hd__and2_1 dg_and (.A(clk), .B(g_l), .X(dgclk));
  sky130_fd_sc_hd__dfxtp_1 dg_r (.CLK(dgclk), .D(x), .Q(dg_q));
  sky130_fd_sc_hd__dlclkp_1 icg (.CLK(clk), .GATE(g), .GCLK(gclk));
  sky130_fd_sc_hd__dfxtp_1 gf_r (.CLK(gclk), .D(x), .Q(gf_q));
endmodule}
close $stream
read_verilog $verilog2
link_design top2
create_clock -name clk -period 6 [get_ports clk]
set_input x 0.5 0.5
set_input g 0.2 0.1
set_input rb 0.9 0.05
simulate_power_activity -cycles 4000 -warmup 500 -lanes 256 -seed 1
foreach pin {sy_r/Q dg_and/X icg/GCLK dg_r/Q gf_r/Q} {
  show $pin
}
