# define_scene -liberty/-spef with -liberty_min/max/-spef_min/max
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
read_spef -name reg1 reg1_asap7.spef
catch { define_scene s1 -liberty asap7_small -liberty_max asap7_small } error
puts $error
catch { define_scene s1 -liberty asap7_small -spef reg1 -spef_min reg1 } error
puts $error
