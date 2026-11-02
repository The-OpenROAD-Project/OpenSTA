# Vectorless power per scene in a design with two modes (1ns and 10ns).
# Clock activity and the sampling period must come from the reported
# scene's clocks, so each scene matches a run with only its own mode.
source ../../test/helpers.tcl
read_liberty ../../test/nangate45/Nangate45_typ.lib

set verilog [make_result_file power_scene_clk.v]
set stream [open $verilog w]
puts $stream {module top (clk, a, b, z);
  input clk, a, b;
  output z;
  wire qa, qb, x;
  DFF_X1 ra (.D(a), .CK(clk), .Q(qa));
  DFF_X1 rb (.D(b), .CK(clk), .Q(qb));
  XOR2_X1 ux (.A(qa), .B(qb), .Z(x));
  DFF_X1 rz (.D(x), .CK(clk), .Q(z));
endmodule}
close $stream
read_verilog $verilog
link_design top

foreach {mode period} {fast 1 slow 10} {
  set sdc [make_result_file power_scene_clk_$mode.sdc]
  set stream [open $sdc w]
  puts $stream "create_clock -name clk -period $period \[get_ports clk\]"
  puts $stream "set_input_delay 0 -clock clk \[get_ports {a b}\]"
  close $stream
  read_sdc -mode $mode $sdc
}
define_scene s_fast -mode fast -liberty NangateOpenCellLibrary
define_scene s_slow -mode slow -liberty NangateOpenCellLibrary
# Same absolute input rate in both scenes (0.05 transitions per ns).
set_power_activity -input -density 0.05 -duty 0.5

foreach scene {s_fast s_slow} {
  puts "scene $scene"
  report_power -scene $scene -digits 4
  report_power -scene $scene -instances [get_cells {ra ux}] -digits 4
}
