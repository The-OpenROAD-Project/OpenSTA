# Test ReportPath.cc: All report_path format variants with field combinations,
# reportPathFull, reportPath3, set_report_path_format, set_report_path_fields,
# set_report_path_field_order, reportHierPinsThru, reportInputExternalDelay,
# drvrFanout, hasExtInputDriver, clkNetworkDelayIdealProp,
# descriptionField, descriptionNet.
# Also exercises reportStartpoint, reportEndpoint, reportGroup for
# different PathEnd types, and report_path_cmd with different formats.
# Targets: ReportPath.cc reportPathFull, reportPath3, reportPath4/5/6,
#          setReportFieldOrder, setReportFields, reportPathLine with fields,
#          reportStartpoint, reportEndpoint, reportGroup,
#          reportRequired, reportSlack, reportCommonClkPessimism,
#          reportClkUncertainty, pathStartpoint, pathEndpoint
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_crpr_data_checks.v
link_design search_crpr_data_checks

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 8 [get_ports clk2]
set_propagated_clock [get_clocks clk1]
set_propagated_clock [get_clocks clk2]

set_input_delay -clock clk1 1.0 [get_ports in1]
set_input_delay -clock clk1 1.0 [get_ports in2]
set_output_delay -clock clk1 2.0 [get_ports out1]
set_output_delay -clock clk2 2.0 [get_ports out2]

############################################################
# All format x fields combinations
############################################################
puts "--- format full + all field combos ---"
report_checks -path_delay max -format full -fields {capacitance}
report_checks -path_delay max -format full -fields {slew}
report_checks -path_delay max -format full -fields {fanout}
report_checks -path_delay max -format full -fields {input_pin}
report_checks -path_delay max -format full -fields {net}
report_checks -path_delay max -format full -fields {src_attr}
report_checks -path_delay max -format full -fields {capacitance slew}
report_checks -path_delay max -format full -fields {capacitance fanout}
report_checks -path_delay max -format full -fields {slew fanout}
report_checks -path_delay max -format full -fields {input_pin net}
report_checks -path_delay max -format full -fields {capacitance slew fanout}
report_checks -path_delay max -format full -fields {capacitance slew fanout input_pin}
report_checks -path_delay max -format full -fields {capacitance slew fanout input_pin net}
report_checks -path_delay max -format full -fields {capacitance slew fanout input_pin net src_attr}

puts "--- format full_clock + fields ---"
report_checks -path_delay max -format full_clock -fields {capacitance slew fanout}
report_checks -path_delay max -format full_clock -fields {input_pin net src_attr}
report_checks -path_delay max -format full_clock -fields {capacitance slew fanout input_pin net src_attr}

puts "--- format full_clock_expanded + fields ---"
report_checks -path_delay max -format full_clock_expanded -fields {capacitance slew fanout}
report_checks -path_delay max -format full_clock_expanded -fields {input_pin net src_attr}
report_checks -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin net src_attr}
report_checks -path_delay min -format full_clock_expanded -fields {capacitance slew fanout input_pin net src_attr}

puts "--- format short + fields ---"
report_checks -path_delay max -format short -fields {capacitance}
report_checks -path_delay max -format short -fields {slew}
report_checks -path_delay max -format short -fields {capacitance slew fanout}

puts "--- format end ---"
report_checks -path_delay max -format end
report_checks -path_delay min -format end

puts "--- format summary ---"
report_checks -path_delay max -format summary
report_checks -path_delay min -format summary

puts "--- format slack_only ---"
report_checks -path_delay max -format slack_only
report_checks -path_delay min -format slack_only

############################################################
# report_path_cmd with different internal formats
############################################################
puts "--- report_path_cmd with formats ---"
set paths [find_timing_paths -path_delay max -endpoint_count 3]
foreach pe $paths {
  set p [$pe path]

  # Full format
  sta::set_report_path_format full
  sta::report_path_cmd $p

  # Full clock
  sta::set_report_path_format full_clock
  sta::report_path_cmd $p

  # Full clock expanded
  sta::set_report_path_format full_clock_expanded
  sta::report_path_cmd $p

  # JSON
  sta::set_report_path_format json
  sta::report_path_cmd $p

  sta::set_report_path_format full
  break
}

############################################################
# set_report_path_field_properties / set_report_path_field_width
############################################################
puts "--- field properties ---"
sta::set_report_path_field_properties "delay" "Delay" 10 0
sta::set_report_path_field_width "delay" 12
report_checks -path_delay max

sta::set_report_path_field_properties "total" "Total" 12 0
sta::set_report_path_field_width "total" 14
report_checks -path_delay max

############################################################
# set_report_path_sigmas
############################################################
puts "--- report_path_sigmas ---"
sta::set_report_path_sigmas 1
report_checks -path_delay max
sta::set_report_path_sigmas 0
report_checks -path_delay max

############################################################
# set_report_path_no_split
############################################################
puts "--- report_path_no_split ---"
sta::set_report_path_no_split 1
report_checks -path_delay max -fields {capacitance slew fanout}
sta::set_report_path_no_split 0

############################################################
# Digits
############################################################
puts "--- digits ---"
report_checks -path_delay max -digits 2
report_checks -path_delay max -digits 4
report_checks -path_delay max -digits 6

############################################################
# Per-endpoint reporting
############################################################
puts "--- per-endpoint ---"
report_checks -to [get_ports out1] -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin net}
report_checks -to [get_ports out2] -path_delay max -format full_clock_expanded -fields {capacitance slew fanout input_pin net}
report_checks -to [get_ports out1] -path_delay min -format full_clock_expanded -fields {capacitance slew fanout}
report_checks -to [get_ports out2] -path_delay min -format full_clock_expanded -fields {capacitance slew fanout}

############################################################
# From specific pins
############################################################
puts "--- from pins ---"
report_checks -from [get_pins reg1/CK] -path_delay max -format full_clock_expanded -fields {capacitance slew}
report_checks -from [get_ports in1] -path_delay max -format full_clock -fields {slew fanout}

############################################################
# min_max
############################################################
puts "--- min_max ---"
report_checks -path_delay min_max -format full_clock_expanded -fields {capacitance slew fanout}

############################################################
# report_checks JSON with endpoint_count
############################################################
puts "--- JSON endpoint count ---"
report_checks -path_delay max -format json -endpoint_count 5
report_checks -path_delay min -format json -endpoint_count 5

############################################################
# report_checks with -corner
############################################################
puts "--- corner ---"
set corner [sta::cmd_scene]
report_checks -path_delay max -corner $corner -format full_clock_expanded -fields {capacitance slew}

############################################################
# set_input_transition and verify report includes it
############################################################
puts "--- input_transition in report ---"
set_input_transition 0.15 [get_ports in1]
report_checks -from [get_ports in1] -path_delay max -format full_clock_expanded -fields {slew}

############################################################
# set_driving_cell and report
############################################################
puts "--- driving_cell in report ---"
set_driving_cell -lib_cell BUF_X2 -pin Z [get_ports in2]
report_checks -from [get_ports in2] -path_delay max -format full_clock_expanded -fields {capacitance slew}

############################################################
# report_path_end_header/footer with min paths
############################################################
puts "--- report_path_end min ---"
sta::report_path_end_header
set min_paths [find_timing_paths -path_delay min -endpoint_count 5]
set prev ""
foreach pe $min_paths {
  if { $prev != "" } {
    sta::report_path_end2 $pe $prev 0
  } else {
    sta::report_path_end $pe
  }
  set prev $pe
}
sta::report_path_end_footer
