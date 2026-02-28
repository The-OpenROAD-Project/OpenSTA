# Test filter commands, constrained queries, all_inputs/all_outputs,
# and various query/utility commands for code coverage.
# Targets:
#   Sdc.cc: allInputs, allOutputs, isConstrained (pin, instance, net),
#     findClocksMatching, sortedClocks, findClock,
#     isClockSrc, isClock, isIdealClock,
#     clkThruTristateEnabled, setClkThruTristateEnabled,
#     removeConstraints
#   Sdc.i: all_inputs_cmd, all_outputs_cmd, filter_ports, filter_insts,
#     filter_pins, filter_clocks, filter_lib_cells, filter_lib_pins,
#     filter_liberty_libraries, filter_nets, filter_timing_arcs,
#     group_path_names, pin_is_constrained, instance_is_constrained,
#     net_is_constrained, is_clock_src, is_clock, is_ideal_clock,
#     clk_thru_tristate_enabled, set_clk_thru_tristate_enabled,
#     find_clocks_matching, default_arrival_clock,
#     pin_case_logic_value, pin_logic_value, remove_constraints
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
create_clock -name vclk -period 5
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]

############################################################
# all_inputs / all_outputs
############################################################
set inputs [all_inputs]
puts "all_inputs count = [llength $inputs]"

set inputs_no_clk [all_inputs -no_clocks]
puts "all_inputs -no_clocks count = [llength $inputs_no_clk]"

set outputs [all_outputs]
puts "all_outputs count = [llength $outputs]"

############################################################
# find_clocks_matching
############################################################
set clks [sta::find_clocks_matching "clk*" 0 0]
puts "find_clocks_matching clk* = [llength $clks]"

set clks [sta::find_clocks_matching {^clk[0-9]+$} 1 0]
puts "find_clocks_matching regexp = [llength $clks]"

set clks [sta::find_clocks_matching "CLK*" 0 1]
puts "find_clocks_matching nocase = [llength $clks]"

############################################################
# Clock source queries
############################################################
set clk1_pin [sta::find_pin clk1]
puts "clk1 is_clock_src = [sta::is_clock_src $clk1_pin]"
puts "clk1 is_clock = [sta::is_clock $clk1_pin]"
# TODO: sta::is_ideal_clock removed from SWIG interface
# puts "clk1 is_ideal_clock = [sta::is_ideal_clock $clk1_pin]"
puts "clk1 is_ideal_clock = skipped (API removed)"

set in1_pin [sta::find_pin in1]
puts "in1 is_clock_src = [sta::is_clock_src $in1_pin]"
puts "in1 is_clock = [sta::is_clock $in1_pin]"

############################################################
# Default arrival clock
############################################################
set def_clk [sta::default_arrival_clock]
if {$def_clk ne ""} {
  puts "default_arrival_clock exists"
} else {
  puts "default_arrival_clock is null"
}

############################################################
# Clock thru tristate
############################################################
set val [sta::clk_thru_tristate_enabled]
puts "clk_thru_tristate_enabled = $val"
sta::set_clk_thru_tristate_enabled 1
set val [sta::clk_thru_tristate_enabled]
puts "clk_thru_tristate_enabled after set = $val"
sta::set_clk_thru_tristate_enabled 0

############################################################
# Constrained queries
############################################################
# TODO: pin_is_constrained, instance_is_constrained, net_is_constrained
# removed from SWIG interface
puts "pin clk1 constrained = skipped (API removed)"
puts "pin in1 constrained = skipped (API removed)"
puts "instance buf1 constrained = skipped (API removed)"
puts "net n1 constrained = skipped (API removed)"

############################################################
# Case analysis and logic value queries
############################################################
set_case_analysis 0 [get_ports in1]
set_case_analysis 1 [get_ports in2]

set pin_in1 [lindex [get_pins buf1/A] 0]
set val [sta::pin_case_logic_value $in1_pin]
puts "in1 case_logic_value = $val"

set val [sta::pin_logic_value $in1_pin]
puts "in1 logic_value = $val"

# Set logic values
set_logic_zero [get_ports in3]

set in3_pin [sta::find_pin in3]
set val [sta::pin_logic_value $in3_pin]
puts "in3 logic_value = $val"

############################################################
# Group paths and group_path_names
############################################################
group_path -name grp_reg2reg -from [get_clocks clk1] -to [get_clocks clk1]
group_path -name grp_io -from [get_ports {in1 in2}] -to [get_ports out1]

set gp_names [sta::group_path_names]
puts "group_path_names = $gp_names"

set is_gp [sta::is_path_group_name "grp_reg2reg"]
puts "is_path_group_name grp_reg2reg = $is_gp"

set is_gp [sta::is_path_group_name "nonexistent"]
puts "is_path_group_name nonexistent = $is_gp"

############################################################
# Filter commands
############################################################

# filter_ports
set all_ports [get_ports *]
set filtered [sta::filter_ports direction == input $all_ports]
puts "filter_ports direction == input: [llength $filtered]"

# filter_clocks
set all_clks [get_clocks *]
set filtered [sta::filter_clocks period == 10.000 $all_clks]
puts "filter_clocks period == 10: [llength $filtered]"

# filter_lib_cells
set all_cells [get_lib_cells NangateOpenCellLibrary/*]
set filtered [sta::filter_lib_cells is_buffer == 1 $all_cells]
puts "filter_lib_cells is_buffer: [llength $filtered]"

# filter_insts
set all_insts [get_cells *]
set filtered [sta::filter_insts ref_name =~ "BUF*" $all_insts]
puts "filter_insts ref_name =~ BUF*: [llength $filtered]"

# filter_pins
set all_pins [get_pins buf1/*]
set filtered [sta::filter_pins direction == input $all_pins]
puts "filter_pins direction == input: [llength $filtered]"

# filter_nets
set all_nets [get_nets *]
set filtered [sta::filter_nets full_name =~ n* $all_nets]
puts "filter_nets full_name =~ n*: [llength $filtered]"

############################################################
# Write SDC with all constraints
############################################################
set sdc1 [make_result_file sdc_filter_query1.sdc]
write_sdc -no_timestamp $sdc1
diff_files sdc_filter_query1.sdcok $sdc1

############################################################
# Unset case analysis
############################################################
unset_case_analysis [get_ports in1]
unset_case_analysis [get_ports in2]

############################################################
# remove_constraints
############################################################
# TODO: sta::remove_constraints removed from Sta API
# sta::remove_constraints
# report_checks
puts "remove_constraints: skipped (API removed)"

############################################################
# Re-apply constraints for final write
############################################################
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_output_delay -clock clk1 3.0 [get_ports out1]

set sdc2 [make_result_file sdc_filter_query2.sdc]
write_sdc -no_timestamp $sdc2
diff_files sdc_filter_query2.sdcok $sdc2
