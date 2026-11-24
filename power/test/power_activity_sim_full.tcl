# simulate_power_activity -full: registers, latches and clock gates are
# simulated from loops through cells. Exact toggles per cycle: enable flop
# 0.045, toggle flop 0.05, hold-only flop 0, pipeline stage 2 0.045, clock
# gated flop 0.1. Clock period 6ns.
source ../../test/helpers.tcl
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib

set verilog [make_result_file power_activity_sim_full.v]
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

simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
foreach pin {en_r/Q tg_r/Q hq_r/Q p1_r/Q gf_r/Q lt/Q ar_r/Q} {
  show $pin
}
report_activity_annotation
report_power

puts "Rerun with tg_r/Q set by set_power_activity: it keeps its activity."
set_power_activity -pins [get_pins tg_r/Q] -activity 0.2 -duty 0.5
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
foreach pin {en_r/Q tg_r/Q tg_xor/A} {
  show $pin
}
report_activity_annotation

puts "An annotated clock gate output (as read_vcd does) is still simulated"
puts "from its clock and enable in full mode: gf_r/Q keeps 0.1."
set_power_activity -pins [get_pins icg/GCLK] -activity 0.3333 -duty 0.5
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
show icg/GCLK
show gf_r/Q
unset_power_activity -pins [get_pins icg/GCLK]

puts "Full mode, combinational mode, then formulas (the default)."
unset_power_activity -pins [get_pins tg_r/Q]
set_power_activity_mode full
show en_r/Q
set_power_activity_mode combinational
show en_r/Q
set_power_activity_mode vectorless
show en_r/Q

puts "Pin tied high in the netlist, and a latch + AND clock gate."
# sy_r is a reset synchronizer flop with D tied to 1'b1; its duty is near
# rb's. The latch + AND gated clock dg_and/X toggles 2 x 0.2 = 0.4 per cycle
# like icg/GCLK, not the 0.45 of the AND formula.
set verilog2 [make_result_file power_activity_sim_full2.v]
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
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
foreach pin {sy_r/Q dg_and/X icg/GCLK dg_r/Q gf_r/Q} {
  show $pin
}

# Every pin except sequential internal pins (IQ, IQ_N) has an activity.
proc unknown_pins {} {
  set count 0
  foreach pin [get_pins -hierarchical * -filter "direction != internal"] {
    lassign [get_property $pin activity] density duty origin
    if { $origin == "unknown" } { incr count }
  }
  puts "pins with unknown activity: $count"
}
unknown_pins

puts "Input-less divider, 60 stage pipeline, case analysis and annotation on"
puts "a load pin, pin swap."
# div_r toggles every cycle (1.0), so the clock gate it enables gives 1.0.
# Every pipeline stage toggles like x (0.5). With mx/S held at 0 by case
# analysis, or annotated at duty 0, mx_r holds (0). After swapping the
# inputs of sw, sw/A follows its new net (0.02).
set verilog3 [make_result_file power_activity_sim_full3.v]
set stream [open $verilog3 w]
puts $stream "module top3 (clk, x, y, sel, dq, gq, s, mq, wq);"
puts $stream {  input clk, x, y, sel;
  output dq, gq, s, mq, wq;
  wire dq_n, gclk, md, ya, xb, wd;
  wire [60:0] p;
  sky130_fd_sc_hd__inv_1 div_inv (.A(dq), .Y(dq_n));
  sky130_fd_sc_hd__dfxtp_1 div_r (.CLK(clk), .D(dq_n), .Q(dq));
  sky130_fd_sc_hd__dlclkp_1 dicg (.CLK(clk), .GATE(dq), .GCLK(gclk));
  sky130_fd_sc_hd__dfxtp_1 dg_r (.CLK(gclk), .D(x), .Q(gq));
  sky130_fd_sc_hd__mux2_1 mx (.A0(mq), .A1(x), .S(sel), .X(md));
  sky130_fd_sc_hd__dfxtp_1 mx_r (.CLK(clk), .D(md), .Q(mq));
  sky130_fd_sc_hd__dfxtp_1 xa_r (.CLK(clk), .D(x), .Q(xb));
  sky130_fd_sc_hd__dfxtp_1 ya_r (.CLK(clk), .D(y), .Q(ya));
  sky130_fd_sc_hd__and2_1 sw (.A(xb), .B(ya), .X(wd));
  sky130_fd_sc_hd__dfxtp_1 sw_r (.CLK(clk), .D(wd), .Q(wq));
  assign p[0] = x;
  assign s = p[60];}
for {set i 0} {$i < 60} {incr i} {
  puts $stream "  sky130_fd_sc_hd__dfxtp_1 s$i (.CLK(clk), .D(p\[$i\]), .Q(p\[[expr {$i + 1}]\]));"
}
puts $stream "endmodule"
close $stream
read_verilog $verilog3
link_design top3
create_clock -name clk -period 6 [get_ports clk]
set_input x 0.5 0.5
set_input y 0.5 0.02
set_input sel 0.5 0.5
set_case_analysis 0 [get_pins mx/S]
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
foreach pin {div_r/Q dicg/GCLK dg_r/CLK s0/Q s49/Q s50/Q s59/Q mx/S mx_r/Q
  sw/A sw/B} {
  show $pin
}
unknown_pins
unset_case_analysis [get_pins mx/S]
set_power_activity -pins [get_pins mx/S] -activity 0 -duty 0
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
show mx_r/Q
disconnect_pin xb [get_pins sw/A]
disconnect_pin ya [get_pins sw/B]
connect_pin ya [get_pins sw/A]
connect_pin xb [get_pins sw/B]
show sw/A
show sw/B

puts "Clock gate enable fed by a register on the gated clock."
# a_r/Q follows xr_r/Q one cycle later (0.1 toggles per cycle), and the gate
# is enabled when they differ, so GCLK duty is 0.05 (no combinational loop).
set verilog4 [make_result_file power_activity_sim_full4.v]
set stream [open $verilog4 w]
puts $stream {module top4 (clk, x, q1);
  input clk, x;
  output q1;
  wire xr, en, g;
  sky130_fd_sc_hd__dfxtp_1 a_r (.CLK(g), .D(xr), .Q(q1));
  sky130_fd_sc_hd__xor2_1 ex (.A(q1), .B(xr), .X(en));
  sky130_fd_sc_hd__dfxtp_1 xr_r (.CLK(clk), .D(x), .Q(xr));
  sky130_fd_sc_hd__dlclkp_1 icg (.CLK(clk), .GATE(en), .GCLK(g));
endmodule}
close $stream
read_verilog $verilog4
link_design top4
create_clock -name clk -period 6 [get_ports clk]
set_input x 0.5 0.1
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
foreach pin {xr_r/Q a_r/Q icg/GCLK} {
  show $pin
}

puts "Clock from a macro (PLL), output without a function, clock on a load"
puts "pin, default input activity with a virtual clock."
# pll/CLKOUT carries a generated clock, so pf_r toggles like x (0.5).
# mo/Z has no function and toggles at the default input activity, like the
# port z: 0.1 per 2ns virtual clock period, 0.3 per cycle. The clock on lb/A
# leaves its driver dg/X a data signal (0.25 duty).
set lib5 [make_result_file power_activity_sim_full5.lib]
set stream [open $lib5 w]
puts $stream {library (power_activity_sim_full5) {
  delay_model : table_lookup;
  time_unit : "1ns";
  voltage_unit : "1V";
  current_unit : "1mA";
  leakage_power_unit : "1nW";
  capacitive_load_unit (1,ff);
  pulling_resistance_unit : "1kohm";
  nom_process : 1.0;
  nom_temperature : 25.0;
  nom_voltage : 1.8;
  cell (PLL) {
    pin (REF) { direction : input; capacitance : 1.0; }
    pin (CLKOUT) { direction : output; }
  }
  cell (MIXOUT) {
    pin (A) { direction : input; capacitance : 1.0; }
    pin (Y) { direction : output; function : "A"; }
    pin (Z) { direction : output; }
  }
}}
close $stream
read_liberty $lib5
set verilog5 [make_result_file power_activity_sim_full5.v]
set stream [open $verilog5 w]
puts $stream {module top5 (clk, x, y, z, pq, mq, zq, dq, lq);
  input clk, x, y, z;
  output pq, mq, zq, dq, lq;
  wire pclk, my, mz, d, lclk;
  PLL pll (.REF(clk), .CLKOUT(pclk));
  sky130_fd_sc_hd__dfxtp_1 pf_r (.CLK(pclk), .D(x), .Q(pq));
  MIXOUT mo (.A(x), .Y(my), .Z(mz));
  sky130_fd_sc_hd__dfxtp_1 mo_r (.CLK(clk), .D(mz), .Q(mq));
  sky130_fd_sc_hd__dfxtp_1 z_r (.CLK(clk), .D(z), .Q(zq));
  sky130_fd_sc_hd__and2_1 dg (.A(x), .B(y), .X(d));
  sky130_fd_sc_hd__dfxtp_1 d_r (.CLK(clk), .D(d), .Q(dq));
  sky130_fd_sc_hd__buf_1 lb (.A(d), .X(lclk));
  sky130_fd_sc_hd__dfxtp_1 lf_r (.CLK(lclk), .D(x), .Q(lq));
endmodule}
close $stream
read_verilog $verilog5
link_design top5
create_clock -name clk -period 6 [get_ports clk]
create_generated_clock -name pclk -source [get_ports clk] -multiply_by 1 \
  [get_pins pll/CLKOUT]
create_clock -name lclk -period 6 [get_pins lb/A]
create_clock -name vclk -period 2
set_input x 0.5 0.5
set_input y 0.5 0.5
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
foreach pin {pll/CLKOUT pf_r/Q mo/Z mo_r/Q z z_r/Q dg/X d_r/Q lb/A lf_r/Q} {
  if { [get_pins -quiet $pin] == "" } {
    lassign [get_property [get_ports $pin] activity] density duty origin
    puts [format "%-6s toggles/cycle %.3f duty %.3f %s" \
            $pin [expr {$density * 6e-9}] $duty $origin]
  } else {
    show $pin
  }
}
unknown_pins
# The clock on lb/A held at 0 by case analysis does not toggle.
set_case_analysis 0 [get_pins lb/A]
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
show lf_r/Q
unset_case_analysis [get_pins lb/A]

puts "Clock gate test enables: driven, constant and unconnected."
# The gate is enabled when E or a driven or constant SE is 1. E has duty
# 0.25, so GCK toggles 2 x (1 - 0.75 x 0.7) = 0.95 per cycle with SE at duty
# 0.3, 2 x (1 - 0.75 x 0) = 0.5 with SE unconnected, and 2.0 with SE held at
# 1 by case analysis, as the formulas give.
read_liberty ../../test/nangate45/Nangate45_typ.lib
set verilog6 [make_result_file power_activity_sim_full6.v]
set stream [open $verilog6 w]
puts $stream {module top6 (clk, en, te, d, q, q2, q3);
  input clk, en, te, d;
  output q, q2, q3;
  wire gclk, gclk2, gclk3, en2;
  AND2_X1 a1 (.A1(en), .A2(d), .ZN(en2));
  CLKGATETST_X1 icg (.CK(clk), .E(en2), .SE(te), .GCK(gclk));
  CLKGATETST_X1 icg2 (.CK(clk), .E(en2), .GCK(gclk2));
  CLKGATETST_X1 icg3 (.CK(clk), .E(en2), .GCK(gclk3));
  DFF_X1 r (.D(d), .CK(gclk), .Q(q));
  DFF_X1 r2 (.D(d), .CK(gclk2), .Q(q2));
  DFF_X1 r3 (.D(d), .CK(gclk3), .Q(q3));
endmodule}
close $stream
read_verilog $verilog6
link_design top6
create_clock -name clk -period 6 [get_ports clk]
set_input en 0.5 0.2
set_input d 0.5 0.2
set_input te 0.3 0.1
set_case_analysis 1 [get_pins icg3/SE]
foreach pin {icg/GCK icg2/GCK icg3/GCK} {
  show $pin
}
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
foreach pin {icg/GCK icg2/GCK icg3/GCK} {
  show $pin
}

puts "Clocks with different periods: the automatic run uses the formulas"
puts "(once warned), the command runs both at the fastest period."
set verilog7 [make_result_file power_activity_sim_full7.v]
set stream [open $verilog7 w]
puts $stream {module top7 (clk1, clk2, x, q1, q2);
  input clk1, clk2, x;
  output q1, q2;
  sky130_fd_sc_hd__dfxtp_1 r1 (.CLK(clk1), .D(x), .Q(q1));
  sky130_fd_sc_hd__dfxtp_1 r2 (.CLK(clk2), .D(x), .Q(q2));
endmodule}
close $stream
read_verilog $verilog7
link_design top7
create_clock -name clk1 -period 6 [get_ports clk1]
create_clock -name clk2 -period 4 [get_ports clk2]
set_input x 0.5 0.5
set_power_activity_mode full
show r1/Q
show r2/Q
set_power_activity_mode vectorless
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
show r1/Q
show r2/Q

puts "Asynchronous clear and preset both asserted: clear wins on DFF_CP"
puts "(clear_preset_var1 L, duty 0.375), preset on dfbbp (H, duty 0.625)."
read_liberty power_activity.lib
set verilog8 [make_result_file power_activity_sim_full8.v]
set stream [open $verilog8 w]
puts $stream {module top8 (clk, x, c, p, cq, pq);
  input clk, x, c, p;
  output cq, pq;
  wire c_n, p_n;
  DFF_CP cw_r (.CK(clk), .D(x), .CLR(c), .PRE(p), .Q(cq));
  sky130_fd_sc_hd__inv_1 ci (.A(c), .Y(c_n));
  sky130_fd_sc_hd__inv_1 pi (.A(p), .Y(p_n));
  sky130_fd_sc_hd__dfbbp_1 pw_r (.CLK(clk), .D(x), .RESET_B(c_n), .SET_B(p_n),
    .Q(pq));
endmodule}
close $stream
read_verilog $verilog8
link_design top8
create_clock -name clk -period 6 [get_ports clk]
set_input x 0.5 0.5
set_input c 0.5 0.5
set_input p 0.5 0.5
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
show cw_r/Q
show pw_r/Q

puts "Two latches in one cell, the second reading the first: while"
puts "transparent Q2 follows D, so xor(Q2, D) is constant 0."
set verilog9 [make_result_file power_activity_sim_full9.v]
set stream [open $verilog9 w]
puts $stream {module top9 (clk, x, q1, q2, xo);
  input clk, x;
  output q1, q2, xo;
  wire xb;
  LAT2 l (.D(xb), .G(clk), .Q1(q1), .Q2(q2));
  sky130_fd_sc_hd__buf_1 b (.A(x), .X(xb));
  sky130_fd_sc_hd__xor2_1 x2 (.A(q2), .B(xb), .X(xo));
endmodule}
close $stream
read_verilog $verilog9
link_design top9
create_clock -name clk -period 6 [get_ports clk]
set_input x 0.5 0.5
simulate_power_activity -full -cycles 4000 -settle_cycles 500 -lanes 256 -seed 1
show l/Q1
show l/Q2
show x2/X
