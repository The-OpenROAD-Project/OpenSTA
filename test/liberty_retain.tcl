# Liberty retaining_rise/fall and retain_rise_slew/retain_fall_slew.
# Retain tables are the min (contamination) delay and slew.

read_liberty liberty_retain.lib
read_verilog liberty_retain.v
link_design top

create_clock -name clk -period 10
set_input_delay -clock clk 0 {in1 in2}
set_output_delay -clock clk 0 {out1 out2}
set_input_transition 0 {in1 in2}
set_load 0 {out1 out2}

report_checks -path_delay max -to out1 -digits 4 -fields {slew} \
  -group_path_count 1
report_checks -path_delay min -to out1 -digits 4 -fields {slew} \
  -group_path_count 1

puts "max dcalc retain slew"
report_dcalc -from u1/A -to u1/Y -max -digits 4
puts "min dcalc retain slew"
report_dcalc -from u1/A -to u1/Y -min -digits 4

puts "max dcalc parent slew fallback"
report_dcalc -from u2/A -to u2/Y -max -digits 4
puts "min dcalc parent slew fallback"
report_dcalc -from u2/A -to u2/Y -min -digits 4
