# Leakage when conditions that name output pins, groups without when, and
# zero-leakage states. Inputs have duty 0.5. Expected leakage (nW):
# INV_OUT_WHEN 0.5*1 + 0.5*3 = 2.0, AND_PARTIAL 0.25*4 + 0.75*2 = 2.5,
# BUF_ZERO_STATE 0.5*0 + 0.5*2 = 1.0.
source ../../test/helpers.tcl
read_liberty power_leakage_when.lib
set verilog [make_result_file power_leakage_when.v]
set stream [open $verilog w]
puts $stream {module top (a, b, y1, y2, y3);
  input a, b;
  output y1, y2, y3;
  INV_OUT_WHEN inv (.A(a), .Y(y1));
  AND_PARTIAL and2 (.A(a), .B(b), .Y(y2));
  BUF_ZERO_STATE buf1 (.A(a), .Y(y3));
endmodule}
close $stream
read_verilog $verilog
link_design top
create_clock -name clk -period 10
set_power_activity -input_ports [get_ports {a b}] -activity 0.1 -duty 0.5
foreach inst {inv and2 buf1} {
  lassign [sta::instance_power [get_cells $inst] [sta::cmd_scene]] \
    internal switching leakage total
  puts [format "%-5s leakage %.2f nW" $inst [expr {$leakage * 1e9}]]
}
