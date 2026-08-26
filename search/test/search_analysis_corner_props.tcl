# analysis_corner object properties: built-ins (name/full_name),
# user-defined properties (define_property -object_type analysis_corner,
# set_property, get_property), get_analysis_corners -filter, base
# property-command passthrough, error cases.
read_liberty ../../test/nangate45/Nangate45_slow.lib
read_liberty ../../test/nangate45/Nangate45_fast.lib
read_verilog search_crpr.v
link_design search_crpr

create_clock -name clk -period 10 [get_ports clk]

define_analysis_corner ss_cmax -liberty NangateOpenCellLibrary_slow
define_analysis_corner ff_rcmin -liberty NangateOpenCellLibrary_fast
set ss [lindex [get_analysis_corners ss_cmax] 0]
set ff [lindex [get_analysis_corners ff_rcmin] 0]

# Built-in properties.
puts "name: [get_property $ss name]"
puts "full_name: [get_property $ff full_name]"

# User-defined properties.
define_property -object_type analysis_corner -type float voltage
define_property -object_type analysis_corner -type string rc_tag
define_property -object_type analysis_corner -type bool is_slow
set_property $ss voltage 0.72
set_property $ff voltage 0.88
set_property $ss rc_tag cmax
set_property $ss is_slow true

puts "ss voltage: [get_property $ss voltage]"
puts "ff voltage: [get_property $ff voltage]"
puts "ss rc_tag: [get_property $ss rc_tag]"
puts "ss is_slow: [get_property $ss is_slow]"
# Declared but never set reads back empty.
puts "ff rc_tag: '[get_property $ff rc_tag]'"

# -filter.
puts "cmax: [llength [get_analysis_corners -filter {rc_tag == cmax}]]"
puts "not cmax: [llength [get_analysis_corners -filter {rc_tag != cmax}]]"
puts "tagged: [llength [get_analysis_corners -filter {defined(rc_tag)}]]"
puts "untagged: [llength [get_analysis_corners -filter {undefined(rc_tag)}]]"
puts "slow: [llength [get_analysis_corners -filter {is_slow == true}]]"
puts "volt+pattern: [llength [get_analysis_corners -filter {defined(voltage)} ss*]]"
foreach corner [get_analysis_corners -filter {defined(voltage)}] {
  puts "has voltage: [get_property $corner name]"
}

# Base property commands still work through the wrappers.
define_property -object_type mode -type string mode_tag
set_property [lindex [get_modes] 0] mode_tag functional
puts "mode tag: [get_property [lindex [get_modes] 0] mode_tag]"

# Errors: undeclared property set, unknown property get, missing -type.
puts "set undeclared: [catch {set_property $ss undeclared_prop 1} msg] $msg"
puts "get unknown: [catch {get_property $ss no_such_prop} msg] $msg"
puts "no type: [catch {define_property -object_type analysis_corner voltage} msg] $msg"
