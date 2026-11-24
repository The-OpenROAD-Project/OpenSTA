# Vectorless activity of registers driven by integrated clock gates (ICGs).
# Sampling a two-state Markov input D at gated clock edges (p_en = P(EN)) gives
# out_density = active_edges * c / (p_en + (1 - p_en) * u), matching enable-mux
# flops for i.i.d. random inputs (u = 1) and attenuating missed toggles when
# 0 < p_en < 1 while reducing identically to ungated behavior at p_en = 1.
source ../../test/helpers.tcl
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set verilog [make_result_file power_gated_reg_activity.v]
set stream [open $verilog w]
puts $stream {module top (clk, d, en, q_en, q_icg, qb_icg, qn_icg, q_ungated);
  input clk, d, en;
  output q_en, q_icg, qb_icg, qn_icg, q_ungated;
  wire gclk;
  sky130_fd_sc_hd__edfxtp_1 r_en (.CLK(clk), .D(d), .DE(en), .Q(q_en));
  sky130_fd_sc_hd__sdlclkp_1 icg (.CLK(clk), .GATE(en), .SCE(1'b0), .GCLK(gclk));
  sky130_fd_sc_hd__dfxtp_1 r_icg (.CLK(gclk), .D(d), .Q(q_icg));
  sky130_fd_sc_hd__dfxbp_1 rb_icg (.CLK(gclk), .D(d), .Q(qb_icg), .Q_N(qn_icg));
  sky130_fd_sc_hd__dfxtp_1 r_ungated (.CLK(clk), .D(d), .Q(q_ungated));
endmodule}
close $stream
read_verilog $verilog
link_design top

set_cmd_units -time ns
create_clock -name clk -period 1 [get_ports clk]

proc show { pin } {
  lassign [get_property [get_pins $pin] activity] density duty origin
  puts [format "%-11s toggles/cycle %.4f duty %.3f %s" \
          $pin [expr {$density * 1e-9}] $duty $origin]
}

puts "Random i.i.d. D (u = 1): ICG-gated flop matches enable-mux flop"
foreach {d_duty d_density} {0.5 0.50 0.2 0.32 0.8 0.32} {
  puts "P(D) $d_duty P(EN) 0.1"
  set_power_activity -input_ports [get_ports d] -density $d_density -duty $d_duty
  set_power_activity -input_ports [get_ports en] -density 0.02 -duty 0.1
  show r_en/Q
  show r_icg/Q
  show rb_icg/Q_N
}

puts "Moderate D activity (0.03/cycle, duty 0.5) with P(EN) = 0.05 vs ungated"
set_power_activity -input_ports [get_ports d] -density 0.03 -duty 0.5
set_power_activity -input_ports [get_ports en] -density 0.01 -duty 0.05
show r_en/Q
show r_icg/Q
show r_ungated/Q

puts "Slow D activity (0.001/cycle, duty 0.5) with P(EN) = 0.5"
set_power_activity -input_ports [get_ports d] -density 0.001 -duty 0.5
set_power_activity -input_ports [get_ports en] -density 0.1 -duty 0.5
show r_en/Q
show r_icg/Q
show r_ungated/Q

puts "Enable on every other cycle, then single cycle pulses: two loads never"
puts "follow each other, so the data is taken as random (0.25, 0.05)."
set_power_activity -input_ports [get_ports d] -density 0.5 -duty 0.5
set_power_activity -input_ports [get_ports en] -density 1.0 -duty 0.5
show r_en/Q
show r_icg/Q
set_power_activity -input_ports [get_ports en] -density 0.2 -duty 0.1
show r_en/Q
show r_icg/Q

puts "EN constant 0"
set_case_analysis 0 [get_ports en]
show r_icg/Q

puts "EN constant 1, D 0.03/cycle"
set_case_analysis 1 [get_ports en]
set_power_activity -input_ports [get_ports d] -density 0.03 -duty 0.5
show r_icg/Q
show r_ungated/Q

puts "Toggles and loads: JK flop (J 0.2, K 0.8: 0.32) and a toggle flop with"
puts "a synchronous reset (T 0.5, RST 0.5: 0.375), random inputs."
read_liberty power_activity.lib
set verilog2 [make_result_file power_gated_reg_activity2.v]
set stream [open $verilog2 w]
puts $stream {module top2 (clk, j, k, t, rst, jq, tq);
  input clk, j, k, t, rst;
  output jq, tq;
  JK_FF jk (.CK(clk), .J(j), .K(k), .Q(jq));
  T_FF_SR tsr (.CK(clk), .T(t), .RST(rst), .Q(tq));
endmodule}
close $stream
read_verilog $verilog2
link_design top2
create_clock -name clk -period 1 [get_ports clk]
set_power_activity -input_ports [get_ports j] -density 0.32 -duty 0.2
set_power_activity -input_ports [get_ports k] -density 0.32 -duty 0.8
set_power_activity -input_ports [get_ports {t rst}] -density 0.5 -duty 0.5
show jk/Q
show tsr/Q

puts "Random data at the fast clock into registers on a divided clock: the"
puts "data is sampled at its own period, so a random input differs between"
puts "two samples with probability 0.5 (0.25 plain, 0.125 with enable 0.5,"
puts "0.025 with enable 0.1, toggles per fast cycle)."
set verilog3 [make_result_file power_gated_reg_activity3.v]
set stream [open $verilog3 w]
puts $stream {module top3 (clk, x, en, cq, pq, fq, sq);
  input clk, x, en;
  output cq, pq, fq, sq;
  wire dq_n, cq_w, xr, gclk;
  sky130_fd_sc_hd__inv_1 div_inv (.A(cq_w), .Y(dq_n));
  sky130_fd_sc_hd__dfxtp_1 div_r (.CLK(clk), .D(dq_n), .Q(cq_w));
  sky130_fd_sc_hd__buf_1 cb (.A(cq_w), .X(cq));
  sky130_fd_sc_hd__dfxtp_1 xr_r (.CLK(clk), .D(x), .Q(xr));
  sky130_fd_sc_hd__dfxtp_1 fs_r (.CLK(cq_w), .D(xr), .Q(pq));
  sky130_fd_sc_hd__edfxtp_1 fs_e (.CLK(cq_w), .D(xr), .DE(en), .Q(fq));
  sky130_fd_sc_hd__dlclkp_1 icg (.CLK(cq_w), .GATE(en), .GCLK(gclk));
  sky130_fd_sc_hd__dfxtp_1 fs_g (.CLK(gclk), .D(xr), .Q(sq));
endmodule}
close $stream
read_verilog $verilog3
link_design top3
create_clock -name clk -period 1 [get_ports clk]
create_generated_clock -name dclk -source [get_ports clk] -divide_by 2 \
  [get_pins div_r/Q]
set_power_activity -input_ports [get_ports x] -density 0.5 -duty 0.5
foreach {en_duty en_density} {0.5 0.5 0.5 0.02 0.1 0.18 0.1 0.02} {
  puts "P(EN) $en_duty, $en_density toggles per cycle"
  set_power_activity -input_ports [get_ports en] -density $en_density \
    -duty $en_duty
  show fs_r/Q
  show fs_e/Q
  show fs_g/Q
}
