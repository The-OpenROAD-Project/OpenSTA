# Leakage when conditions that name output pins, groups without when, and
# zero-leakage states. Inputs have duty 0.5. Expected leakage (nW):
# INV_OUT_WHEN 0.5*1 + 0.5*3 = 2.0, AND_PARTIAL 0.25*4 + 0.75*2 = 2.5,
# BUF_ZERO_STATE 0.5*0 + 0.5*2 = 1.0, DFF_Q_WHEN with Q annotated at duty
# 0.8: 8.0, CHAIN_OUT_WHEN 0.5*1 = 0.5, CHAIN_ORDER_WHEN 0.5*4 = 2.0,
# BUF_CASE_WHEN with Y held at 0 by case analysis: 2.0.
# INV_INTERNAL_WHEN internal power 0.
source ../../test/helpers.tcl
read_liberty power_leakage_when.lib
set verilog [make_result_file power_leakage_when.v]
set stream [open $verilog w]
puts $stream {module top (a, b, clk, y1, y2, y3, q, c1, c2, y4, o1, o2, y5);
  input a, b, clk;
  output y1, y2, y3, q, c1, c2, y4, o1, o2, y5;
  DFF_Q_WHEN dff (.D(a), .CK(clk), .Q(q));
  CHAIN_OUT_WHEN chain (.A(a), .Y1(c1), .Y2(c2));
  INV_INTERNAL_WHEN inv2 (.A(a), .Y(y4));
  INV_OUT_WHEN inv (.A(a), .Y(y1));
  AND_PARTIAL and2 (.A(a), .B(b), .Y(y2));
  BUF_ZERO_STATE buf1 (.A(a), .Y(y3));
  CHAIN_ORDER_WHEN order (.A(a), .Y1(o1), .Y2(o2));
  BUF_CASE_WHEN buf2 (.A(a), .Y(y5));
endmodule}
close $stream
read_verilog $verilog
link_design top
create_clock -name clk -period 10 [get_ports clk]
set_power_activity -input_ports [get_ports {a b}] -activity 0.1 -duty 0.5
set_power_activity -pins [get_pins dff/Q] -activity 0.1 -duty 0.8
set_case_analysis 0 [get_pins buf2/Y]
foreach inst {inv and2 buf1 dff chain order buf2} {
  lassign [sta::instance_power [get_cells $inst] [sta::cmd_scene]] \
    internal switching leakage total
  puts [format "%-5s leakage %.2f nW" $inst [expr {$leakage * 1e9}]]
}
lassign [sta::instance_power [get_cells inv2] [sta::cmd_scene]] internal
puts [format "inv2  internal %.2f" [expr {$internal * 1e9}]]
