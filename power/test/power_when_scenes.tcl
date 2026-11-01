# State dependent internal power with a different liberty per scene. The
# function (linked cell) and when (scene cell) ports share BDD variables by name.
# A only switches Z when B is 1, so the "!B" group of AND2_WHEN should get
# no weight in either scene.
source ../../test/helpers.tcl

read_liberty power_activity.lib
set stream [open power_activity.lib]
set lib_text [read $stream]
close $stream
regsub {library \(power_activity\)} $lib_text {library (power_activity_b)} lib_text
set lib_b [make_result_file power_activity_b.lib]
set stream [open $lib_b w]
puts $stream $lib_text
close $stream
read_liberty $lib_b

set verilog [make_result_file power_when_scenes.v]
set stream [open $verilog w]
puts $stream {module top (a, b, w);
  input a, b;
  output w;
  AND2_WHEN u_when (.A(a), .B(b), .Z(w));
endmodule}
close $stream
read_verilog $verilog
link_design top

create_clock -name clk -period 10
set_input_delay 0 -clock clk [all_inputs]
define_scene s1 -liberty power_activity
define_scene s2 -liberty power_activity_b
set_power_activity -input_ports {a b} -density 0.05 -duty 0.5

report_power -instances u_when -scene s1 -digits 4
report_power -instances u_when -scene s2 -digits 4
