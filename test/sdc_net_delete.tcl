# Constraints on deleted nets are removed and moved nets keep them.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog sdc_net_delete.v
link_design sdc_net_delete
create_clock -name clk -period 100 [get_ports clk]
set_output_delay 1 -clock clk [get_ports out2]
group_path -name thru_n2 -through [get_nets n2]
set_false_path -through [get_nets {n2 n4}]
set_resistance 10 [get_nets n2]
set_timing_derate -net_delay -late 1.2 [get_nets n2]
# Exception whose second -through net survives the first.
set_false_path -through [get_nets n2] -through [get_nets n3]
set sdc_file [make_result_file sdc_net_delete.sdc]
write_sdc -no_timestamp $sdc_file
report_file $sdc_file

# Exceptions left without a -through object are deleted.
delete_net [get_nets n2]
# Find exceptions through n3 after the n2 exception is deleted.
set_max_delay 50 -through [get_nets n3]
# Reuse the deleted net's memory.
make_net spare
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
# The deleted net's -through no longer applies once its pins are
# connected to another net.
connect_pin spare buf1/Y
connect_pin spare buf2/A
report_checks -through [get_pins buf2/A] -format end

# Remove buf3, moving the constraints on its output net to its input net.
set_resistance 5 [get_nets n4]
set_load 0.01 [get_nets n4]
sta::move_constraints_cmd {} {} {} {} [get_nets n4] [get_nets n1]
disconnect_pin n4 buf4/A
delete_instance buf3
delete_net [get_nets n4]
connect_pin n1 buf4/A
write_sdc -no_timestamp $sdc_file
report_file $sdc_file
report_checks -through [get_pins buf4/A] -format end
