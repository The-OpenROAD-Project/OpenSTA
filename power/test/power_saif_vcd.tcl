# Test power VCD/SAIF reading and highest_power_instances
# Targets uncovered Power.cc paths: highestPowerInstances, power with VCD,
# SAIF reading (SaifReader.cc, SaifLex.ll, SaifParse.yy),
# VCD reading (VcdReader.cc, VcdParse.cc)

source ../../test/helpers.tcl

#---------------------------------------------------------------
# Test 1: VCD reading with the example design
#---------------------------------------------------------------
puts "--- VCD reading test ---"
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__tt_025C_1v80.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc

puts "--- read_vcd ---"
read_vcd -scope gcd_tb/gcd1 ../../examples/gcd_sky130hd.vcd.gz

puts "--- report_activity_annotation after VCD ---"
report_activity_annotation

puts "--- report_activity_annotation -report_annotated ---"
report_activity_annotation -report_annotated

puts "--- report_activity_annotation -report_unannotated ---"
report_activity_annotation -report_unannotated

puts "--- report_power with VCD ---"
report_power

puts "--- report_power -digits 5 ---"
report_power -digits 5

puts "--- report_power -format json ---"
report_power -format json

#---------------------------------------------------------------
# Test 2: highest_power_instances
#---------------------------------------------------------------
puts "--- highest_power_instances ---"
report_power -highest_power_instances 5

report_power -highest_power_instances 3 -format json

report_power -highest_power_instances 10 -digits 4

#---------------------------------------------------------------
# Test 3: instance power with VCD
#---------------------------------------------------------------
puts "--- instance power with VCD ---"
set some_cells [get_cells _*_]
report_power -instances $some_cells
