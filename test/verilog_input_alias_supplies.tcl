source verilog_input_alias_helpers.tcl

set written_file [make_result_file verilog_input_alias_supplies.v]
foreach net {power_net ground_net} {
  load_input_alias
  connect_pin $net external_input
  connect_pin $net other_input
  foreach options {{} {-include_pwr_gnd}} {
    write_verilog {*}$options $written_file
    puts "Supply net: $net; options: [list $options]"
    report_file $written_file
  }
}
