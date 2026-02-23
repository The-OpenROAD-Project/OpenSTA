# Test SAIF reading for power analysis
# Targets uncovered SaifReader.cc, SaifLex.ll, SaifParse.yy

source ../../test/helpers.tcl

#---------------------------------------------------------------
# SAIF reading with the example design
#---------------------------------------------------------------
puts "--- SAIF reading test ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

puts "--- read_saif ---"
read_saif -scope gcd_tb/gcd1 ../../examples/gcd_sky130hd.saif.gz

puts "--- report_activity_annotation after SAIF ---"
report_activity_annotation

puts "--- report_activity_annotation -report_annotated ---"
report_activity_annotation -report_annotated

puts "--- report_activity_annotation -report_unannotated ---"
report_activity_annotation -report_unannotated

puts "--- report_power with SAIF ---"
report_power

puts "--- report_power -format json ---"
report_power -format json

puts "--- report_power -highest_power_instances ---"
# TODO: report_power -highest_power_instances broken (highest_power_instances SWIG fn removed)
sta::report_power_highest_insts 5 [sta::cmd_scene] $sta_report_default_digits
