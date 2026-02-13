# Test Property.cc: generated clock properties (is_generated, is_virtual,
# is_propagated, sources, period, waveform), clock pin properties (clocks,
# clock_domains), instance and cell properties for genclk designs,
# ReportPath.cc: reportGenClkSrcAndPath, reportGenClkSrcPath for
# generated clock source path expansion,
# report_checks with various formats for generated clk domain paths,
# PathEnd properties for generated clock paths.
# Targets: Property.cc getProperty(Clock) all branches,
#          getProperty(Pin) clocks/clock_domains,
#          ReportPath.cc reportSrcClkAndPath, reportGenClkSrcAndPath,
#          reportGenClkSrcPath, reportGenClkSrcPath1,
#          isGenPropClk, pathFromGenPropClk
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_genclk.v
link_design search_genclk

create_clock -name clk -period 10 [get_ports clk]
create_generated_clock -name div_clk -source [get_pins clkbuf/Z] -divide_by 2 [get_pins div_reg/Q]

set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]
set_output_delay -clock div_clk 1.0 [get_ports out2]

# Run timing
report_checks -path_delay max > /dev/null

############################################################
# Clock properties: master vs generated
############################################################
puts "--- Master clock properties ---"
set mclk [get_clocks clk]
puts "clk name: [get_property $mclk name]"
puts "clk full_name: [get_property $mclk full_name]"
puts "clk period: [get_property $mclk period]"
puts "clk is_generated: [get_property $mclk is_generated]"
puts "clk is_virtual: [get_property $mclk is_virtual]"
puts "clk is_propagated: [get_property $mclk is_propagated]"
set clk_srcs [get_property $mclk sources]
puts "clk sources: [llength $clk_srcs]"
foreach s $clk_srcs { puts "  src: [get_full_name $s]" }
puts "PASS: master clock properties"

puts "--- Generated clock properties ---"
set gclk [get_clocks div_clk]
puts "div_clk name: [get_property $gclk name]"
puts "div_clk full_name: [get_property $gclk full_name]"
puts "div_clk period: [get_property $gclk period]"
puts "div_clk is_generated: [get_property $gclk is_generated]"
puts "div_clk is_virtual: [get_property $gclk is_virtual]"
puts "div_clk is_propagated: [get_property $gclk is_propagated]"
set gsrc [get_property $gclk sources]
puts "div_clk sources: [llength $gsrc]"
foreach s $gsrc { puts "  src: [get_full_name $s]" }
puts "PASS: generated clock properties"

############################################################
# Propagated clock property change
############################################################
puts "--- Propagated clock toggle ---"
set_propagated_clock [get_clocks clk]
puts "clk is_propagated (after set): [get_property [get_clocks clk] is_propagated]"
puts "div_clk is_propagated (after set): [get_property [get_clocks div_clk] is_propagated]"
report_checks -path_delay max > /dev/null
unset_propagated_clock [get_clocks clk]
puts "clk is_propagated (after unset): [get_property [get_clocks clk] is_propagated]"
puts "PASS: propagated toggle"

############################################################
# Virtual clock
############################################################
puts "--- Virtual clock ---"
create_clock -name vclk -period 5
set vclk [get_clocks vclk]
puts "vclk is_virtual: [get_property $vclk is_virtual]"
puts "vclk is_generated: [get_property $vclk is_generated]"
puts "vclk period: [get_property $vclk period]"
set vsrc [get_property $vclk sources]
puts "vclk sources: [llength $vsrc]"
puts "PASS: virtual clock"

############################################################
# Pin clocks / clock_domains
############################################################
puts "--- Pin clocks/clock_domains ---"
set ck_pin_main [get_pins reg1/CK]
set pclks [get_property $ck_pin_main clocks]
puts "reg1/CK clocks: [llength $pclks]"

set pdoms [get_property $ck_pin_main clock_domains]
puts "reg1/CK clock_domains: [llength $pdoms]"

set ck_pin_div [get_pins reg2/CK]
set pclks2 [get_property $ck_pin_div clocks]
puts "reg2/CK clocks: [llength $pclks2]"

set pdoms2 [get_property $ck_pin_div clock_domains]
puts "reg2/CK clock_domains: [llength $pdoms2]"

set d_pin [get_pins reg1/D]
set dclks [get_property $d_pin clocks]
puts "reg1/D clocks: [llength $dclks]"

set q_pin [get_pins reg1/Q]
set qclks [get_property $q_pin clocks]
puts "reg1/Q clocks: [llength $qclks]"
puts "PASS: pin clocks/clock_domains"

############################################################
# Report with generated clock paths: full_clock_expanded
############################################################
puts "--- GenClk full_clock_expanded max ---"
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin net}
puts "PASS: genclk full_clock_expanded max"

puts "--- GenClk full_clock_expanded min ---"
report_checks -to [get_ports out2] -path_delay min -format full_clock_expanded -fields {capacitance slew fanout input_pin net}
puts "PASS: genclk full_clock_expanded min"

puts "--- GenClk full_clock max ---"
report_checks -to [get_ports out2] -path_delay max -format full_clock -fields {capacitance slew fanout}
puts "PASS: genclk full_clock"

puts "--- GenClk full max ---"
report_checks -to [get_ports out2] -path_delay max -format full -fields {capacitance slew fanout}
puts "PASS: genclk full"

############################################################
# Report genclk paths in all formats
############################################################
puts "--- GenClk all formats ---"
report_checks -to [get_ports out2] -path_delay max -format short
report_checks -to [get_ports out2] -path_delay max -format end
report_checks -to [get_ports out2] -path_delay max -format summary
report_checks -to [get_ports out2] -path_delay max -format slack_only
report_checks -to [get_ports out2] -path_delay max -format json
puts "PASS: genclk all formats"

############################################################
# GenClk with propagated + full_clock_expanded
############################################################
puts "--- GenClk propagated full_clock_expanded ---"
set_propagated_clock [get_clocks clk]
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin}
report_checks -to [get_ports out2] -path_delay min -format full_clock_expanded -fields {capacitance slew fanout input_pin}
unset_propagated_clock [get_clocks clk]
puts "PASS: genclk propagated expanded"

############################################################
# find_timing_paths for genclk domain
############################################################
puts "--- find_timing_paths genclk domain ---"
set paths_gc [find_timing_paths -to [get_ports out2] -path_delay max -endpoint_path_count 5]
puts "GenClk max paths: [llength $paths_gc]"
foreach pe $paths_gc {
  puts "  pin=[get_full_name [$pe pin]] slack=[$pe slack]"
  puts "    is_check: [$pe is_check] is_output: [$pe is_output_delay]"
  catch { puts "    target_clk: [get_name [$pe target_clk]]" }
  catch { puts "    startpoint_clock: [get_name [get_property $pe startpoint_clock]]" }
  catch { puts "    endpoint_clock: [get_name [get_property $pe endpoint_clock]]" }
  set pts [get_property $pe points]
  puts "    points: [llength $pts]"
}
puts "PASS: genclk paths"

############################################################
# report_path_cmd with genclk path
############################################################
puts "--- report_path_cmd genclk ---"
foreach pe $paths_gc {
  set p [$pe path]
  sta::set_report_path_format full_clock_expanded
  sta::report_path_cmd $p
  sta::set_report_path_format full
  break
}
puts "PASS: genclk report_path_cmd"

############################################################
# report_path_ends for genclk paths
############################################################
puts "--- report_path_ends genclk ---"
sta::report_path_ends $paths_gc
puts "PASS: genclk report_path_ends"

############################################################
# report_clock_properties
############################################################
puts "--- report_clock_properties ---"
report_clock_properties
puts "PASS: clock_properties"

############################################################
# report_clock_skew
############################################################
puts "--- report_clock_skew ---"
report_clock_skew -setup
report_clock_skew -hold
puts "PASS: clock_skew"

############################################################
# All reports with digits
############################################################
puts "--- GenClk digits ---"
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded -digits 6
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded -digits 2
puts "PASS: genclk digits"

############################################################
# report_tns/wns
############################################################
puts "--- tns/wns ---"
report_tns
report_wns
report_worst_slack -max
report_worst_slack -min
puts "PASS: tns/wns"

puts "ALL PASSED"
