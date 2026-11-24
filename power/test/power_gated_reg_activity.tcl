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

puts "EN constant 0"
set_case_analysis 0 [get_ports en]
show r_icg/Q

puts "EN constant 1, D 0.03/cycle"
set_case_analysis 1 [get_ports en]
set_power_activity -input_ports [get_ports d] -density 0.03 -duty 0.5
show r_icg/Q
show r_ungated/Q
