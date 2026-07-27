# Corner-scoped SDC: per-corner timing derates.
# 1 mode x 2 corners with different set_timing_derate must match the
# equivalent 2-synthetic-mode setup numerically. Also checks wholesale
# override of mode derates, fallback for corners without derates, the
# corner-scope command guard, and scope save/restore.
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_slow.lib
read_liberty ../../test/nangate45/Nangate45_fast.lib
read_verilog search_crpr.v
link_design search_crpr

# Base SDC shared by the corner-scoped mode and both synthetic modes.
set base_sdc [make_result_file corner_sdc_base.sdc]
set stream [open $base_sdc "w"]
puts $stream {create_clock -name clk -period 10 [get_ports clk]}
puts $stream {set_input_delay -clock clk 1.0 [get_ports in1]}
puts $stream {set_input_delay -clock clk 1.0 [get_ports in2]}
puts $stream {set_output_delay -clock clk 2.0 [get_ports out1]}
close $stream

set derate_slow_sdc [make_result_file corner_sdc_derate_slow.sdc]
set stream [open $derate_slow_sdc "w"]
puts $stream {set_timing_derate -late 1.2}
close $stream

set derate_fast_sdc [make_result_file corner_sdc_derate_fast.sdc]
set stream [open $derate_fast_sdc "w"]
puts $stream {set_timing_derate -late 1.05}
close $stream

read_sdc -mode func $base_sdc
read_sdc -mode ms $base_sdc
read_sdc -mode mf $base_sdc

define_analysis_corner slow
define_analysis_corner fast
define_analysis_corner typ

# All scenes defined before timing queries.
# 1 mode x 3 corners.
define_scene ss -mode func -liberty NangateOpenCellLibrary_slow -analysis_corner slow
define_scene ff -mode func -liberty NangateOpenCellLibrary_fast -analysis_corner fast
define_scene st -mode func -liberty NangateOpenCellLibrary_slow -analysis_corner typ
# Equivalent synthetic modes.
define_scene ss_syn -mode ms -liberty NangateOpenCellLibrary_slow
define_scene ff_syn -mode mf -liberty NangateOpenCellLibrary_fast

puts "=== baseline, no derates ==="
report_checks -scenes ss -digits 4
report_checks -scenes ff -digits 4

# Corner-scoped derates on mode func; mode-level derates on the synthetic
# modes with the same values.
read_sdc -mode func -analysis_corner slow $derate_slow_sdc
read_sdc -mode func -analysis_corner fast $derate_fast_sdc
read_sdc -mode ms $derate_slow_sdc
read_sdc -mode mf $derate_fast_sdc

puts "=== corner-scoped derates ==="
report_checks -scenes ss -digits 4
report_checks -scenes ff -digits 4
puts "=== synthetic-mode equivalents ==="
report_checks -scenes ss_syn -digits 4
report_checks -scenes ff_syn -digits 4
puts "equiv ss: [expr {[worst_slack -scene ss -max] == [worst_slack -scene ss_syn -max]}]"
puts "equiv ff: [expr {[worst_slack -scene ff -max] == [worst_slack -scene ff_syn -max]}]"

# Mode-level derate on func: corners with derates override it wholesale;
# the typ corner (no derates) falls back to it.
set derate_base_sdc [make_result_file corner_sdc_derate_base.sdc]
set stream [open $derate_base_sdc "w"]
puts $stream {set_timing_derate -late 1.5}
close $stream
read_sdc -mode func $derate_base_sdc

puts "=== mode derate 1.5: ss/ff unchanged, typ corner derated ==="
puts "ss still corner-derated: [expr {[worst_slack -scene ss -max] == [worst_slack -scene ss_syn -max]}]"
puts "ff still corner-derated: [expr {[worst_slack -scene ff -max] == [worst_slack -scene ff_syn -max]}]"
report_checks -scenes st -digits 4

# Corner unset_timing_derate empties the overlay; the corner then falls
# back to the mode derate (1.5).
set_cmd_analysis_corner slow
unset_timing_derate
unset_cmd_analysis_corner
puts "=== slow overlay emptied: falls back to mode derate ==="
puts "ss matches typ: [expr {[worst_slack -scene ss -max] == [worst_slack -scene st -max]}]"

# Guard: structural and not-yet-supported commands error in corner scope.
set_cmd_analysis_corner slow
puts [catch { create_clock -name clk2 -period 5 } msg]
puts $msg
puts [catch { set_false_path -from [get_ports in1] } msg]
puts $msg
puts [catch { set_input_delay -clock clk 2.0 [get_ports in1] } msg]
puts $msg
unset_cmd_analysis_corner
# Out of corner scope the guarded commands work again.
puts [catch { set_input_delay -clock clk 1.0 [get_ports in1] } msg]
puts $msg

# Error cases.
puts [catch { set_cmd_analysis_corner no_such_corner } msg]
puts $msg
puts [catch { read_sdc -analysis_corner no_such_corner $derate_slow_sdc } msg]
puts $msg
