read_liberty /Users/ethanmoon/OpenSTA/sky130hd_tt.lib
read_verilog /Users/ethanmoon/OpenSTA/verilog_counter_synth_synth_output.v
link_design counter
create_clock -name clk [get_ports clk] -period 50
set timing_paths [find_timing_paths -sort_by_slack]
set critical_path [lindex $timing_paths 0]
set critical_path_length [expr {- [get_property $critical_path slack]}]
report_checks -format full
puts stderr "Critical path length is $critical_path_length\n"

set instance [sta::top_instance]
set cell [$instance cell]
set name [$cell name]
set src_location [$cell get_attribute "src"]
puts "$critical_path for src = $src_location \n"
puts "********allgood********"