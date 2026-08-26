# Corner SDC bundles: define_analysis_corner -sdc files are applied to
# the (mode, corner) overlay Sdc when the first scene naming the corner
# is defined. No synthetic modes: one mode, N corners, full whitelist
# per corner (derates, IO delays, clock uncertainty, clock latency).
# Checks: equivalence vs a manually composed mode, mode list stays clean,
# per-pin IO override + corner-only IO constraint, clock-object
# uncertainty isolation between corners, structural commands in a corner
# bundle error, whole-bundle redefine rules, property/filter.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_slow.lib
read_liberty ../../test/nangate45/Nangate45_fast.lib
read_verilog search_crpr.v
link_design search_crpr

set base_sdc [make_result_file corner_bundle_base.sdc]
set stream [open $base_sdc "w"]
puts $stream {create_clock -name clk -period 10 [get_ports clk]}
puts $stream {set_input_delay -clock clk 1.0 [get_ports in1]}
puts $stream {set_output_delay -clock clk 2.0 [get_ports out1]}
puts $stream {set_clock_uncertainty 0.1 [get_clocks clk]}
puts $stream {set_clock_latency 0.5 [get_clocks clk]}
close $stream

# Full whitelist in the corner SDC: derate, IO override, corner-only IO
# constraint (in2 is unconstrained in the mode), clock uncertainty,
# clock latency, source latency.
set corner_ss_sdc [make_result_file corner_bundle_ss.sdc]
set stream [open $corner_ss_sdc "w"]
puts $stream {set_timing_derate -late 1.2}
puts $stream {set_input_delay -clock clk 2.5 [get_ports in1]}
puts $stream {set_input_delay -clock clk 3.0 [get_ports in2]}
puts $stream {set_output_delay -clock clk 3.5 [get_ports out1]}
puts $stream {set_clock_uncertainty 0.4 [get_clocks clk]}
puts $stream {set_clock_latency 1.5 [get_clocks clk]}
puts $stream {set_clock_latency -source 0.3 [get_clocks clk]}
close $stream

read_sdc -mode func $base_sdc

define_analysis_corner ss -liberty NangateOpenCellLibrary_slow -sdc [list $corner_ss_sdc]
define_analysis_corner ff -liberty NangateOpenCellLibrary_fast

define_scene s_ss -mode func -analysis_corner ss
define_scene s_ff -mode func -analysis_corner ff

# Manual composition in a separate mode for the equivalence check.
read_sdc -mode msyn $base_sdc
read_sdc -mode msyn $corner_ss_sdc
define_scene s_manual -mode msyn -liberty NangateOpenCellLibrary_slow

# No synthetic modes are minted.
set mode_names {}
foreach mode [get_modes] { lappend mode_names [get_name $mode] }
puts "modes: [lsort $mode_names]"
foreach scene [get_scenes] {
  puts "[get_name $scene] corner: [get_property $scene analysis_corner]"
}
puts "filter ss: [llength [get_scenes -filter {analysis_corner == ss}]]"

puts "=== corner-composed scene (Mode: func) ==="
report_checks -scenes s_ss -digits 4
puts "=== plain corner scene (mode constraints only) ==="
report_checks -scenes s_ff -digits 4
puts "equiv vs manual: [expr {[worst_slack -scene s_ss -max] == [worst_slack -scene s_manual -max]}]"
# ff keeps the mode's 0.1 uncertainty: the corner's clock uncertainty is
# stored on the corner, not the shared Clock object.
puts "ff isolated: [expr {[worst_slack -scene s_ff -max] != [worst_slack -scene s_ss -max]}]"

# Structural commands in a corner bundle error out (guard active while
# the bundle is applied in corner scope).
set bad_sdc [make_result_file corner_bundle_bad.sdc]
set stream [open $bad_sdc "w"]
puts $stream {create_clock -name clk2 -period 5}
close $stream
define_analysis_corner bad -liberty NangateOpenCellLibrary_slow -sdc [list $bad_sdc]
puts [catch { define_scene s_bad -mode func -analysis_corner bad } msg]
puts $msg

# Bare corner redefine preserves the sdc bundle; redefine with data
# replaces the whole bundle.
define_analysis_corner ss
puts "bundle after bare redefine: [llength [sta::analysis_corner_sdc [sta::find_analysis_corner ss]]]"
define_analysis_corner ss -liberty NangateOpenCellLibrary_slow
puts "bundle after data redefine: [llength [sta::analysis_corner_sdc [sta::find_analysis_corner ss]]]"

# set_scene_analysis_corner applies the corner's SDC bundle and
# invalidates cached timing: re-association after a report changes slack.
set assoc_sdc [make_result_file corner_assoc.sdc]
set stream [open $assoc_sdc "w"]
puts $stream {set_timing_derate -late 1.5}
close $stream
define_analysis_corner assoc -liberty NangateOpenCellLibrary_slow -sdc [list $assoc_sdc]
set before [worst_slack -scene s_manual -max]
set_scene_analysis_corner s_manual assoc
puts "assoc bundle applied: [expr {$before != [worst_slack -scene s_manual -max]}]"
