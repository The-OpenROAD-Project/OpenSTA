# set_max_delay with large -to pin and instance sets, two that differ
# only in their -to, -to internal pins that paths go on past, and an
# exception removed and redefined: every
# endpoint's slack must stay what the -to sets say.
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog ../examples/gcd_sky130hd.v.gz
link_design gcd
read_sdc ../examples/gcd_sky130hd.sdc

set ins [all_inputs -no_clocks]
set outs [all_outputs]
set half [expr { [llength $outs] / 2 }]

# -to instances
set_max_delay 0.5 -from $ins -to [all_registers]
# the same value, -from and type, two -to sets: a path delay with a -to
# is not merged (PathDelay::mergeable), so these stay two exceptions
set_max_delay 0.6 -from [all_registers] -to [lrange $outs 0 [expr { $half - 1 }]]
set_max_delay 0.6 -from [all_registers] -to [lrange $outs $half end]
# -to pins
set_max_delay 0.3 -from $ins -to $outs
# -to internal pins: the path goes on past them, and the exception's
# state must end there (Sdc::isCompleteTo), not reach the endpoints beyond
set internal [lsort [lmap p [get_pins */Y] { get_full_name $p }]]
set_max_delay 0.4 -from [all_registers] -to [get_pins [lrange $internal 0 99]]

report_checks -group_path_count 1000 -endpoint_path_count 1 -format end
report_checks -to [get_pins [lrange $internal 0 9]] -format end
report_tns
report_wns
# a path delay's state ends at its -to: past it, no tags for it
puts "tags [sta::tag_count] tag groups [sta::tag_group_count]"

# removed and redefined, tighter
unset_path_exceptions -from [all_registers] -to $outs
set_max_delay 0.2 -from [all_registers] -to $outs
report_checks -group_path_count 1000 -endpoint_path_count 1 -format end
report_tns
report_wns

# constraints moved from one pin to another (Sdc::moveConstraints): the
# -to follows them, and the path delay ends at the new pin
set d_pins [lsort [lmap p [all_registers -data_pins] { get_full_name $p }]]
set moved_from [get_pins [lindex $d_pins 0]]
set moved_to [get_pins [lindex $d_pins 1]]
unset_path_exceptions -from $ins
unset_path_exceptions -from [all_registers]
set_max_delay 0.05 -from $ins -to $moved_from
report_checks -to $moved_from -format end
report_checks -to $moved_to -format end
sta::move_pin_constraints_cmd $moved_from $moved_to
report_checks -to $moved_from -format end
report_checks -to $moved_to -format end
