# Store/retrieve/write max area and power SDC constraints.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top

set_max_area 123.5
set_max_dynamic_power 1.25
set_max_leakage_power 0.75

puts "max_area [sta::max_area]"
puts "max_dynamic_power [sta::max_dynamic_power]"
puts "max_leakage_power [sta::max_leakage_power]"

set sdc_file [make_result_file max_power_area.sdc]
write_sdc -no_timestamp $sdc_file
set stream [open $sdc_file r]
gets $stream line
while { ![eof $stream] } {
  if {[string match "set_max_*" $line]} {
    puts $line
  }
  gets $stream line
}
close $stream
