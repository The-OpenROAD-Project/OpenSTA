# get_scenes / get_modes return objects
read_liberty ../examples/asap7_small_ff.lib.gz
read_liberty ../examples/asap7_small_ss.lib.gz
read_verilog ../examples/reg1_asap7.v
link_design top

read_sdc -mode mode1 ../examples/mcmm2_mode1.sdc
read_sdc -mode mode2 ../examples/mcmm2_mode2.sdc

read_spef -name reg1_ff ../examples/reg1_asap7.spef
read_spef -name reg1_ss ../examples/reg1_asap7_ss.spef

define_scene scene1 -mode mode1 -liberty asap7_small_ff -spef reg1_ff
define_scene scene2 -mode mode2 -liberty asap7_small_ss -spef reg1_ss

puts {[get_scenes *]}
report_object_names [get_scenes *]

puts {[get_modes *]}
report_object_names [get_modes *]
