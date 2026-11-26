# write_timing_model on a wide-port design: most inputs are timed on
# the flat graph, the input that reaches a latch by the search, and a
# false path from one input leaves it without arcs.
source ../../test/helpers.tcl
source timing_model_wide_design.tcl

set verilog [make_result_file "timing_model_wide.v"]
write_wide_model_design $verilog 24 48 12 4 3 1

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog $verilog
link_design wide_model

create_clock -name clk -period 2 [get_ports clk]
set_input_transition 0.05 [all_inputs]
set_false_path -from [get_ports in2]
set_max_delay 1.5 -from [all_inputs -no_clocks] -to [get_clocks clk]
group_path -name in2reg -from [all_inputs -no_clocks] -to [get_clocks clk]

proc report_lib_pins { file pins } {
  set stream [open $file r]
  set show 0
  while { [gets $stream line] >= 0 } {
    if { [regexp {^\s*pin\("([^"]+)"\)} $line -> pin] } {
      set show [expr { [lsearch -exact $pins $pin] >= 0 }]
    } elseif { [regexp {^\s*cell\(} $line] } {
      set show 0
    }
    if { $show } {
      puts $line
    }
  }
  close $stream
}

proc report_lib_arc_counts { file } {
  set stream [open $file r]
  set counts [dict create]
  while { [gets $stream line] >= 0 } {
    if { [regexp {timing_type : (\w+)} $line -> type] } {
      dict incr counts $type
    }
  }
  close $stream
  foreach type [lsort [dict keys $counts]] {
    puts "$type [dict get $counts $type]"
  }
}

set model [make_result_file "timing_model_wide.lib"]
write_timing_model -library_name wide_model_lib $model
report_lib_arc_counts $model
report_lib_pins $model {in0 in1 in2 in5 in23 out0}
