# Test FindRegister.cc deeper: all_registers with various pin types,
# inferred sequentials, edge/level sensitive, rise/fall clk filtering.
# Also test data checks (set_data_check) and write_sdc.
# Targets: FindRegister.cc FindRegVisitor, FindRegPins,
#          FindRegClkPins, FindRegAsyncPins, FindRegOutputPins,
#          findInferedSequential, hasTimingCheck, seqExpr1/seqExpr2,
#          Sta.cc findRegisterInstances, findRegisterDataPins,
#          findRegisterClkPins, findRegisterAsyncPins, findRegisterOutputPins,
#          findRegisterPreamble, setDataCheck, removeDataCheck,
#          writeSdc
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_data_check_gated.v
link_design search_data_check_gated

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports en]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_input_delay -clock clk 0.5 [get_ports rst]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock clk 2.0 [get_ports out2]
set_output_delay -clock clk 2.0 [get_ports out3]

report_checks > /dev/null

############################################################
# all_registers -cells (default)
############################################################
puts "--- all_registers default ---"
set regs [all_registers]
puts "registers: [llength $regs]"
foreach r $regs { puts "  [get_full_name $r]" }

############################################################
# all_registers -cells
############################################################
puts "--- all_registers -cells ---"
set reg_cells [all_registers -cells]
puts "register cells: [llength $reg_cells]"
foreach r $reg_cells { puts "  [get_full_name $r]" }

############################################################
# all_registers -data_pins
############################################################
puts "--- all_registers -data_pins ---"
set dpins [all_registers -data_pins]
puts "data pins: [llength $dpins]"
foreach p $dpins { puts "  [get_full_name $p]" }

############################################################
# all_registers -clock_pins
############################################################
puts "--- all_registers -clock_pins ---"
set ckpins [all_registers -clock_pins]
puts "clock pins: [llength $ckpins]"
foreach p $ckpins { puts "  [get_full_name $p]" }

############################################################
# all_registers -async_pins
############################################################
puts "--- all_registers -async_pins ---"
set apins [all_registers -async_pins]
puts "async pins: [llength $apins]"
foreach p $apins { puts "  [get_full_name $p]" }

############################################################
# all_registers -output_pins
############################################################
puts "--- all_registers -output_pins ---"
set opins [all_registers -output_pins]
puts "output pins: [llength $opins]"
foreach p $opins { puts "  [get_full_name $p]" }

############################################################
# all_registers -edge_triggered
############################################################
puts "--- all_registers -edge_triggered ---"
set et_cells [all_registers -cells -edge_triggered]
puts "edge-triggered: [llength $et_cells]"
foreach c $et_cells { puts "  [get_full_name $c]" }

############################################################
# all_registers -level_sensitive
############################################################
puts "--- all_registers -level_sensitive ---"
set ls_cells [all_registers -cells -level_sensitive]
puts "level-sensitive: [llength $ls_cells]"

############################################################
# all_registers -clock clk with various pin types
############################################################
puts "--- all_registers -clock clk -cells ---"
set clk_regs [all_registers -cells -clock clk]
puts "cells on clk: [llength $clk_regs]"
foreach c $clk_regs { puts "  [get_full_name $c]" }

puts "--- all_registers -clock clk -data_pins ---"
set clk_dpins [all_registers -data_pins -clock clk]
puts "data pins on clk: [llength $clk_dpins]"
foreach p $clk_dpins { puts "  [get_full_name $p]" }

puts "--- all_registers -clock clk -clock_pins ---"
set clk_ckpins [all_registers -clock_pins -clock clk]
puts "clock pins on clk: [llength $clk_ckpins]"
foreach p $clk_ckpins { puts "  [get_full_name $p]" }

puts "--- all_registers -clock clk -async_pins ---"
set clk_apins [all_registers -async_pins -clock clk]
puts "async pins on clk: [llength $clk_apins]"
foreach p $clk_apins { puts "  [get_full_name $p]" }

puts "--- all_registers -clock clk -output_pins ---"
set clk_opins [all_registers -output_pins -clock clk]
puts "output pins on clk: [llength $clk_opins]"
foreach p $clk_opins { puts "  [get_full_name $p]" }

############################################################
# all_registers -rise_clock / -fall_clock
############################################################
puts "--- all_registers -rise_clock ---"
set rise_regs [all_registers -cells -rise_clock clk]
puts "rise clk cells: [llength $rise_regs]"

puts "--- all_registers -fall_clock ---"
set fall_regs [all_registers -cells -fall_clock clk]
puts "fall clk cells: [llength $fall_regs]"

############################################################
# all_registers -edge_triggered -clock combos
############################################################
puts "--- all_registers -edge_triggered -clock clk -data_pins ---"
set et_dpins [all_registers -data_pins -edge_triggered -clock clk]
puts "edge-triggered data pins on clk: [llength $et_dpins]"

puts "--- all_registers -edge_triggered -clock clk -async_pins ---"
set et_apins [all_registers -async_pins -edge_triggered -clock clk]
puts "edge-triggered async pins on clk: [llength $et_apins]"

puts "--- all_registers -edge_triggered -clock clk -output_pins ---"
set et_opins [all_registers -output_pins -edge_triggered -clock clk]
puts "edge-triggered output pins on clk: [llength $et_opins]"

puts "--- all_registers -edge_triggered -clock clk -clock_pins ---"
set et_ckpins [all_registers -clock_pins -edge_triggered -clock clk]
puts "edge-triggered clock pins on clk: [llength $et_ckpins]"

############################################################
# set_data_check
############################################################
puts "--- set_data_check ---"
set_data_check -from [get_pins reg1/CK] -to [get_pins reg1/D] -setup 0.5
report_checks -path_delay max

puts "--- remove_data_check ---"
catch {
  remove_data_check -from [get_pins reg1/CK] -to [get_pins reg1/D] -setup
}

############################################################
# set_clock_gating_check
############################################################
puts "--- set_clock_gating_check ---"
set_clock_gating_check -setup 0.5 [get_cells clk_gate]
report_checks -path_delay max

############################################################
# write_sdc
############################################################
puts "--- write_sdc ---"
set sdc_file [make_result_file "search_reg_deep.sdc"]
write_sdc $sdc_file

############################################################
# startpoints and endpoints
############################################################
puts "--- startpoints ---"
set starts [sta::startpoints]
puts "startpoints: [llength $starts]"

puts "--- endpoints ---"
set ends [sta::endpoints]
puts "endpoints: [llength $ends]"

puts "--- endpoint_path_count ---"
set epc [sta::endpoint_path_count]
puts "endpoint_path_count: $epc"

############################################################
# find_timing_paths with -from/-to/-through combos
############################################################
puts "--- find_timing_paths -from -to ---"
set paths_ft [find_timing_paths -from [get_ports in1] -to [get_pins reg1/D] -path_delay max]
puts "paths from/to: [llength $paths_ft]"

puts "--- find_timing_paths -through ---"
set paths_thru [find_timing_paths -through [get_pins and1/ZN] -path_delay max]
puts "paths through: [llength $paths_thru]"

puts "--- find_timing_paths -rise_from ---"
set paths_rf [find_timing_paths -rise_from [get_ports in1] -path_delay max]
puts "paths rise_from: [llength $paths_rf]"

puts "--- find_timing_paths -fall_from ---"
set paths_ff [find_timing_paths -fall_from [get_ports in1] -path_delay max]
puts "paths fall_from: [llength $paths_ff]"

puts "--- find_timing_paths -rise_to ---"
set paths_rt [find_timing_paths -rise_to [get_pins reg1/D] -path_delay max]
puts "paths rise_to: [llength $paths_rt]"

puts "--- find_timing_paths -fall_to ---"
set paths_flt [find_timing_paths -fall_to [get_pins reg1/D] -path_delay max]
puts "paths fall_to: [llength $paths_flt]"

puts "--- find_timing_paths -rise_through ---"
set paths_rthru [find_timing_paths -rise_through [get_pins and1/ZN] -path_delay max]
puts "paths rise_through: [llength $paths_rthru]"

puts "--- find_timing_paths -fall_through ---"
set paths_fthru [find_timing_paths -fall_through [get_pins and1/ZN] -path_delay max]
puts "paths fall_through: [llength $paths_fthru]"

############################################################
# check_setup subcommands
############################################################
puts "--- check_setup individual flags ---"
check_setup -verbose -no_input_delay
check_setup -verbose -no_output_delay
check_setup -verbose -no_clock
check_setup -verbose -multiple_clock
check_setup -verbose -unconstrained_endpoints
check_setup -verbose -loops
check_setup -verbose -generated_clocks

############################################################
# report_tns / report_wns / report_worst_slack
############################################################
puts "--- report_tns ---"
report_tns -max
report_tns -min
report_tns -max -digits 6

puts "--- report_wns ---"
report_wns -max
report_wns -min
report_wns -max -digits 6

puts "--- report_worst_slack ---"
report_worst_slack -max
report_worst_slack -min
report_worst_slack -max -digits 6
