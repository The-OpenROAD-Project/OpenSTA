# Test Property.cc deeper: more property queries, PathEnd properties,
# Clock generated properties, net_slack, endpoint properties,
# pin arrival/required/slack variants, edge properties deep.
# Also test report_path format variants and field customization.
# Targets: Property.cc getProperty for PathEnd, Path, Edge, Clock (gen),
#          TimingArcSet properties, resistancePropertyValue,
#          capacitancePropertyValue, Port properties deeper,
#          ReportPath.cc reportPath, reportPathFull, reportPath3,
#          setReportFieldOrder, setReportPathFields, reportGenClkSrcAndPath,
#          reportJson, reportShort(PathEndUnconstrained)
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
# Generated clock properties
############################################################
puts "--- Generated clock properties ---"
set gclk [get_clocks div_clk]
puts "gen clock name: [get_property $gclk name]"
puts "gen clock period: [get_property $gclk period]"
puts "gen clock is_generated: [get_property $gclk is_generated]"
puts "gen clock is_virtual: [get_property $gclk is_virtual]"
set gsrc [get_property $gclk sources]
puts "gen clock sources: [llength $gsrc]"

############################################################
# Pin properties on clock pins
############################################################
puts "--- Clock pin properties ---"
set ck_pin [get_pins div_reg/CK]
puts "div_reg/CK is_clock: [get_property $ck_pin is_clock]"
puts "div_reg/CK is_register_clock: [get_property $ck_pin is_register_clock]"
set cks [get_property $ck_pin clocks]
puts "div_reg/CK clocks: [llength $cks]"
set cdoms [get_property $ck_pin clock_domains]
puts "div_reg/CK clock_domains: [llength $cdoms]"

############################################################
# Pin timing properties - arrival, slew, slack variants
############################################################
puts "--- Pin timing properties deep ---"
set dpin [get_pins reg1/D]
puts "arrival_max_rise: [get_property $dpin arrival_max_rise]"
puts "arrival_max_fall: [get_property $dpin arrival_max_fall]"
puts "arrival_min_rise: [get_property $dpin arrival_min_rise]"
puts "arrival_min_fall: [get_property $dpin arrival_min_fall]"
puts "slack_max: [get_property $dpin slack_max]"
puts "slack_max_rise: [get_property $dpin slack_max_rise]"
puts "slack_max_fall: [get_property $dpin slack_max_fall]"
puts "slack_min: [get_property $dpin slack_min]"
puts "slack_min_rise: [get_property $dpin slack_min_rise]"
puts "slack_min_fall: [get_property $dpin slack_min_fall]"
puts "slew_max: [get_property $dpin slew_max]"
puts "slew_max_rise: [get_property $dpin slew_max_rise]"
puts "slew_max_fall: [get_property $dpin slew_max_fall]"
puts "slew_min: [get_property $dpin slew_min]"
puts "slew_min_rise: [get_property $dpin slew_min_rise]"
puts "slew_min_fall: [get_property $dpin slew_min_fall]"

############################################################
# Port properties
############################################################
puts "--- Port properties deep ---"
set in_port [get_ports in1]
puts "port name: [get_property $in_port name]"
puts "port direction: [get_property $in_port direction]"
set out_port [get_ports out1]
puts "out port direction: [get_property $out_port direction]"
set clk_port [get_ports clk]
puts "clk port direction: [get_property $clk_port direction]"

############################################################
# Net properties
############################################################
puts "--- Net properties ---"
set net1 [get_nets n1]
puts "net name: [get_property $net1 name]"
puts "net full_name: [get_property $net1 full_name]"

############################################################
# Instance properties
############################################################
puts "--- Instance properties deep ---"
set inst1 [get_cells reg1]
puts "inst name: [get_property $inst1 name]"
puts "inst full_name: [get_property $inst1 full_name]"
puts "inst ref_name: [get_property $inst1 ref_name]"
set icell [get_property $inst1 cell]
puts "inst cell: [get_name $icell]"

############################################################
# LibertyCell properties
############################################################
puts "--- LibertyCell properties ---"
set lc [get_lib_cells NangateOpenCellLibrary/AND2_X1]
puts "lib_cell name: [get_property $lc name]"
puts "lib_cell full_name: [get_property $lc full_name]"
puts "lib_cell base_name: [get_property $lc base_name]"
puts "lib_cell filename: [get_property $lc filename]"
puts "lib_cell is_buffer: [get_property $lc is_buffer]"
set lib_ref [get_property $lc library]
puts "lib_cell library: [get_name $lib_ref]"

############################################################
# LibertyPort properties
############################################################
puts "--- LibertyPort properties ---"
set lp [get_lib_pins NangateOpenCellLibrary/AND2_X1/ZN]
puts "lib_port name: [get_property $lp name]"
puts "lib_port full_name: [get_property $lp full_name]"
puts "lib_port direction: [get_property $lp direction]"

############################################################
# Library properties
############################################################
puts "--- Library properties ---"
set lib [get_libs NangateOpenCellLibrary]
puts "lib name: [get_property $lib name]"
puts "lib full_name: [get_property $lib full_name]"

############################################################
# Edge properties with timing arc details
############################################################
puts "--- Edge properties deep ---"
set edges [get_timing_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]]
foreach edge $edges {
  puts "edge full_name: [get_property $edge full_name]"
  puts "edge delay_min_fall: [get_property $edge delay_min_fall]"
  puts "edge delay_max_fall: [get_property $edge delay_max_fall]"
  puts "edge delay_min_rise: [get_property $edge delay_min_rise]"
  puts "edge delay_max_rise: [get_property $edge delay_max_rise]"
  puts "edge sense: [get_property $edge sense]"
  set efp [get_property $edge from_pin]
  puts "edge from_pin: [get_full_name $efp]"
  set etp [get_property $edge to_pin]
  puts "edge to_pin: [get_full_name $etp]"
  break
}

############################################################
# PathEnd properties deep
############################################################
puts "--- PathEnd properties deep ---"
set paths [find_timing_paths -path_delay max -endpoint_count 5]
foreach pe $paths {
  set sp [get_property $pe startpoint]
  puts "startpoint: [get_full_name $sp]"
  set ep [get_property $pe endpoint]
  puts "endpoint: [get_full_name $ep]"
  puts "slack: [get_property $pe slack]"
  set sc [get_property $pe startpoint_clock]
  puts "startpoint_clock: [get_name $sc]"
  set ec [get_property $pe endpoint_clock]
  puts "endpoint_clock: [get_name $ec]"
  if { [$pe is_check] } {
    set ecp [get_property $pe endpoint_clock_pin]
    puts "endpoint_clock_pin: [get_full_name $ecp]"
  }
  # PathEnd methods
  puts "  is_check: [$pe is_check]"
  puts "  is_output_delay: [$pe is_output_delay]"
  puts "  is_unconstrained: [$pe is_unconstrained]"
  puts "  is_path_delay: [$pe is_path_delay]"
  puts "  is_latch_check: [$pe is_latch_check]"
  puts "  is_data_check: [$pe is_data_check]"
  puts "  is_gated_clock: [$pe is_gated_clock]"
  puts "  margin: [$pe margin]"
  puts "  data_required_time: [$pe data_required_time]"
  puts "  data_arrival_time: [$pe data_arrival_time]"
  puts "  source_clk_offset: [$pe source_clk_offset]"
  puts "  source_clk_latency: [$pe source_clk_latency]"
  puts "  source_clk_insertion_delay: [$pe source_clk_insertion_delay]"
  puts "  target_clk: [get_name [$pe target_clk]]"
  puts "  target_clk_time: [$pe target_clk_time]"
  puts "  target_clk_offset: [$pe target_clk_offset]"
  puts "  target_clk_delay: [$pe target_clk_delay]"
  puts "  target_clk_insertion_delay: [$pe target_clk_insertion_delay]"
  puts "  target_clk_uncertainty: [$pe target_clk_uncertainty]"
  puts "  target_clk_arrival: [$pe target_clk_arrival]"
  puts "  inter_clk_uncertainty: [$pe inter_clk_uncertainty]"
  puts "  check_crpr: [$pe check_crpr]"
  puts "  clk_skew: [$pe clk_skew]"
  puts "  min_max: [$pe min_max]"
  puts "  end_transition: [$pe end_transition]"
  puts "  check_role: [$pe check_role]"
  break
}

############################################################
# Path properties
############################################################
puts "--- Path properties deep ---"
foreach pe $paths {
  set p [$pe path]
  puts "path pin: [get_full_name [$p pin]]"
  puts "path arrival: [$p arrival]"
  puts "path required: [$p required]"
  puts "path slack: [$p slack]"
  puts "path edge: [$p edge]"
  set ppins [$p pins]
  puts "path pins count: [llength $ppins]"
  break
}

############################################################
# Report checks in all format variants
############################################################
puts "--- report_checks -format full ---"
report_checks -path_delay max -format full

puts "--- report_checks -format full_clock ---"
report_checks -path_delay max -format full_clock

puts "--- report_checks -format full_clock_expanded ---"
report_checks -path_delay max -format full_clock_expanded

puts "--- report_checks -format short ---"
report_checks -path_delay max -format short

puts "--- report_checks -format end ---"
report_checks -path_delay max -format end

puts "--- report_checks -format slack_only ---"
report_checks -path_delay max -format slack_only

puts "--- report_checks -format summary ---"
report_checks -path_delay max -format summary

puts "--- report_checks -format json ---"
report_checks -path_delay max -format json

############################################################
# Report checks with different -fields
############################################################
puts "--- report_checks with -fields combinations ---"
report_checks -path_delay max -fields {capacitance slew fanout}
report_checks -path_delay max -fields {input_pin net}
report_checks -path_delay max -fields {capacitance slew fanout input_pin net src_attr}

############################################################
# report_checks with -digits
############################################################
puts "--- report_checks -digits 6 ---"
report_checks -path_delay max -digits 6

############################################################
# report_checks -no_line_splits
############################################################
puts "--- report_checks -no_line_splits ---"
report_checks -path_delay max -no_line_splits

############################################################
# report_checks to generated clock domain
############################################################
puts "--- report_checks to div_clk domain ---"
report_checks -to [get_ports out2] -format full_clock_expanded

############################################################
# report_checks -unconstrained
############################################################
puts "--- report_checks -unconstrained ---"
report_checks -path_delay max -unconstrained

############################################################
# get_property -object_type
############################################################
puts "--- get_property -object_type ---"
puts "inst: [get_property -object_type instance reg1 name]"
puts "pin: [get_property -object_type pin reg1/D name]"
puts "net: [get_property -object_type net n1 name]"
puts "port: [get_property -object_type port in1 name]"
puts "clock: [get_property -object_type clock clk name]"
puts "lib_cell: [get_property -object_type liberty_cell NangateOpenCellLibrary/AND2_X1 name]"
puts "lib_pin: [get_property -object_type liberty_port NangateOpenCellLibrary/AND2_X1/ZN name]"
puts "library: [get_property -object_type library NangateOpenCellLibrary name]"
