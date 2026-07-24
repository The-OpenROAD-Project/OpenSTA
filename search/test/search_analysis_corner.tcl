# analysis_corner: define/get/pattern, define_scene -analysis_corner,
# get_scenes -filter, set_scene_analysis_corner, error cases
read_liberty ../../test/nangate45/Nangate45_slow.lib
read_liberty ../../test/nangate45/Nangate45_fast.lib
read_verilog search_crpr.v
link_design search_crpr

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

define_analysis_corner slow
define_analysis_corner fast
puts "corner count: [llength [get_analysis_corners]]"
foreach corner [get_analysis_corners] {
  puts "corner: [sta::analysis_corner_name $corner]"
}
puts "match f*: [llength [get_analysis_corners f*]]"

# Duplicate define is idempotent.
define_analysis_corner slow
puts "corner count after dup: [llength [get_analysis_corners]]"

define_scene ss -liberty NangateOpenCellLibrary_slow -analysis_corner slow
define_scene ff -liberty NangateOpenCellLibrary_fast -analysis_corner fast

foreach scene [get_scenes] {
  puts "[get_name $scene] corner: [get_property $scene analysis_corner]"
}

set slow_scenes [get_scenes -filter {analysis_corner == slow}]
puts "filter slow: [llength $slow_scenes] [get_name [lindex $slow_scenes 0]]"
puts "filter not fast: [llength [get_scenes -filter {analysis_corner != fast}]]"
puts "filter pattern: [llength [get_scenes -filter {analysis_corner =~ s*}]]"
puts "filter and name: [llength [get_scenes -filter {analysis_corner == fast && name == ff}]]"
puts "filter none: [llength [get_scenes -filter {analysis_corner == typ}]]"

# Reassociate a scene with a different corner.
set_scene_analysis_corner ss fast
puts "after reassoc filter fast: [llength [get_scenes -filter {analysis_corner == fast}]]"
set_scene_analysis_corner ss slow
puts "after restore filter fast: [llength [get_scenes -filter {analysis_corner == fast}]]"

# Error cases.
puts [catch { set_scene_analysis_corner ss no_such_corner } msg]
puts $msg
puts [catch { set_scene_analysis_corner no_such_scene slow } msg]
puts $msg
puts [catch { define_scene bad -liberty NangateOpenCellLibrary_slow -analysis_corner no_such_corner } msg]
puts $msg

# Timing still reports per scene.
report_checks -scenes ss
report_checks -scenes ff
