# mmcm reg1 parasitics
read_liberty asap7_small_ff.lib.gz
read_liberty asap7_small_ss.lib.gz
read_verilog reg1_asap7.v
link_design top

read_sdc -mode mode1 mcmm2_mode1.sdc
read_sdc -mode mode2 mcmm2_mode2.sdc

read_spef -name reg1_ff reg1_asap7.spef
read_spef -name reg1_ss reg1_asap7_ss.spef

define_scene scene1 -mode mode1 -liberty asap7_small_ff -spef reg1_ff
define_scene scene2 -mode mode2 -liberty asap7_small_ss -spef reg1_ss

report_checks -scenes scene1
report_checks -scenes scene2
report_checks -group_path_count 4
