# write_timing_model -scene uses the delays of its scene with several scenes defined
source helpers.tcl
set fast_lib ../examples/nangate45_fast.lib.gz
set slow_lib ../examples/nangate45_slow.lib.gz
read_liberty $fast_lib
read_liberty $slow_lib
read_verilog write_timing_model_scenes.v
link_design top
define_scene fast -liberty $fast_lib
define_scene slow -liberty $slow_lib
create_clock -name clk -period 10 [get_ports clk]

# First row of the cell_rise table of the arc from_pin -> to_pin in a model file.
proc model_cell_rise { model to_pin from_pin } {
  set stream [open $model r]
  set text [read $stream]
  close $stream
  set pattern [format {pin\("%s"\).*?related_pin : "%s";} $to_pin $from_pin]
  append pattern {.*?cell_rise\(\w+\).*?values\("([^"]*)"}
  if { [regexp $pattern $text ignore values] } {
    return $values
  }
  return "not found"
}

foreach scene {fast slow} {
  set model [make_result_file write_timing_model_scenes_$scene.lib]
  write_timing_model -scene $scene -library_name model_$scene -cell_name top $model
  puts "$scene a -> y: [model_cell_rise $model y a]"
  puts "$scene clk -> q: [model_cell_rise $model q clk]"
  report_checks -scenes $scene -from [get_ports a] -to [get_ports y] \
    -unconstrained -format end
}
