# Store/retrieve/write max area and power SDC constraints.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top

proc report_max_power {} {
  puts "max_dynamic_power [format %.3e [sta::max_dynamic_power]]"
  puts "max_leakage_power [format %.3e [sta::max_leakage_power]]"
}

# Bare power is in the command power unit (the library's 1pW); a unit argument overrides it.
set_max_area 123.5
set_max_dynamic_power 1.25
set_max_leakage_power 0.75 uW

puts "max_area [sta::max_area]"
report_max_power

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

set_cmd_units -power mW
set_max_dynamic_power 2
set_max_leakage_power 3 W
report_max_power

# Written budgets carry their unit, so they read back under another command unit.
write_sdc -no_timestamp $sdc_file
set_cmd_units -power uW
set_max_dynamic_power 0
set_max_leakage_power 0
read_sdc $sdc_file
report_max_power

# The SDC 2.1 parser's -unit form.
set_max_dynamic_power 4 -unit aW
set_max_leakage_power 2 -unit KW
report_max_power

catch { set_max_leakage_power 8 uA } msg
puts $msg
catch { set_max_leakage_power 8 uW -unit uW } msg
puts $msg
