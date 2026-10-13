# set_wire_load_model before link_design (no top instance) must not crash.
# Regression for upstream 47f91f28, fixed in 6d8f13f2.
read_liberty asap7_small.lib.gz
read_liberty gf180mcu_sram.lib.gz
set_wire_load_model -name Estimate -library [get_name [lindex [get_libs gf180*] 0]]
puts "no crash"
