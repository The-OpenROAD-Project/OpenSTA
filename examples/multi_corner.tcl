# 3 liberty corners with +/- 10% derating example
read_liberty nangate45_slow.lib.gz
read_liberty nangate45_typ.lib.gz
read_liberty nangate45_fast.lib.gz
read_verilog example1.v
link_design top
set_timing_derate -early 0.9
set_timing_derate -late  1.1
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

define_scene ss -liberty NangateOpenCellLibrary_slow
define_scene tt -liberty NangateOpenCellLibrary
define_scene ff -liberty NangateOpenCellLibrary_fast

# report all scenes
report_checks -path_delay min_max
# report typical scene
report_checks -scene tt

