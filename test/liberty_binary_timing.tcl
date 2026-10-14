# Timing through a .blib library. The expected output is identical to the
# same script reading ../examples/nangate45_typ.lib.gz directly.
source helpers.tcl

set blib_file [make_result_file nangate45_typ.blib]
write_liberty_binary ../examples/nangate45_typ.lib.gz $blib_file
read_liberty $blib_file
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}
report_checks -path_delay min_max -fields {slew cap input_pins} -digits 3
report_power -digits 3
