# write_timing_model's clock tree paths when a clock sink is a hard
# macro with its own clock tree (min/max_clock_tree_path on its clock
# pin).
#
# The model's max_clock_tree_path is the latest clock latency at a
# register inside the block, its min_clock_tree_path the earliest. The
# registers inside a macro sit its own clock tree behind its clock pin,
# so their latency is the arrival at the pin plus that tree. Here they
# are the latest sinks (pin + 0.40/0.42 ns) and the flops behind the
# clock buffer the earliest, so the model's max must be the macro's pin
# arrival plus 0.40/0.42 ns, above its min.
#
# Passes by exit status.
source helpers.tcl

proc clk_tree_path { lib_file type } {
  set in [open $lib_file r]
  set text [read $in]
  close $in
  set re {timing_type : TYPE_clock_tree_path;\s*cell_rise\(scalar\) \{\s*values\("([^"]+)"\);\s*\}\s*cell_fall\(scalar\) \{\s*values\("([^"]+)"\);}
  if { ![regexp [string map [list TYPE $type] $re] $text ignore rise fall] } {
    puts "FAIL: no ${type}_clock_tree_path in $lib_file"
    exit 1
  }
  return [list $rise $fall]
}

set failed 0
proc check { what ok } {
  if { !$ok } {
    puts "FAIL: $what"
    set ::failed 1
  }
}

read_liberty nangate45/Nangate45_typ.lib
read_liberty timing_model_clktree_macro.lib
read_verilog timing_model_clktree_macro.v
link_design clktree_macro_top

create_clock -name clk -period 10 [get_ports clk]
set_propagated_clock [get_clocks clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_output_delay -clock clk 1.0 [get_ports out1]
set_input_transition 0.05 [all_inputs]

set model [make_result_file timing_model_clktree_macro_model.lib]
write_timing_model -library_name clktree_macro_model \
  -cell_name clktree_macro_top $model

lassign [clk_tree_path $model min] min_rise min_fall
lassign [clk_tree_path $model max] max_rise max_fall
set mac_rise [get_property [get_pins mac/CLK] arrival_max_rise]
# The falling edge arrives half a period after the rising edge.
set mac_fall [expr { [get_property [get_pins mac/CLK] arrival_max_fall] - 5.0 }]
set ff_rise [get_property [get_pins ff1/CK] arrival_min_rise]

check "max_clock_tree_path ($max_rise/$max_fall) is below\
  min_clock_tree_path ($min_rise/$min_fall)" \
  [expr { $max_rise >= $min_rise && $max_fall >= $min_fall }]
check "max_clock_tree_path rise $max_rise is not the macro's pin\
  arrival $mac_rise plus its 0.40 ns tree" \
  [expr { abs($max_rise - ($mac_rise + 0.40)) < 1e-3 }]
check "max_clock_tree_path fall $max_fall is not the macro's pin\
  arrival $mac_fall plus its 0.42 ns tree" \
  [expr { abs($max_fall - ($mac_fall + 0.42)) < 1e-3 }]
check "min_clock_tree_path rise $min_rise is not the earliest flop's\
  clock arrival $ff_rise" \
  [expr { abs($min_rise - $ff_rise) < 1e-3 }]
exit $failed
