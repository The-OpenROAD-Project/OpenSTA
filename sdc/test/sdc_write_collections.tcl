# write_sdc writes all_registers, all_inputs [-no_clocks] and all_outputs
# where an exception names exactly their objects, and the objects
# otherwise: a subset of the registers stays a list.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 10 [get_ports clk2]
set_max_delay 5 -from [all_inputs -no_clocks] -to [all_registers]
set_max_delay 6 -from [all_registers] -to [all_outputs]
group_path -name io -from [all_inputs] -to [all_outputs]
set_false_path -from [get_cells {reg1 reg2}] -to [all_registers]

set sdc_file [make_result_file sdc_write_collections.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
