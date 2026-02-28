source ../../test/helpers.tcl

read_liberty liberty_ecsm.lib

set ecsm_src [open liberty_ecsm.lib r]
set ecsm_text [read $ecsm_src]
close $ecsm_src
foreach token {ecsm_waveform ecsm_waveform_set ecsm_capacitance} {
  if {[string first $token $ecsm_text] < 0} {
    error "liberty_ecsm.lib is missing expected ECSM token '$token'"
  }
}

set cell_names {}
foreach cell [get_lib_cells test_ecsm_lib/*] {
  lappend cell_names [get_name $cell]
}
set cell_names [lsort $cell_names]
puts "ecsm cell count: [llength $cell_names]"
foreach cell_name $cell_names {
  puts "ecsm cell: $cell_name"
}

if {$cell_names ne [list ECSM1 ECSM2]} {
  error "unexpected ECSM cell list: $cell_names"
}

foreach cell_name $cell_names {
  set cell [get_lib_cell test_ecsm_lib/$cell_name]
  set arc_sets [$cell timing_arc_sets]
  if {[llength $arc_sets] == 0} {
    error "$cell_name has no timing arc sets"
  }

  set arc_count 0
  foreach arc_set $arc_sets {
    incr arc_count [llength [$arc_set timing_arcs]]
  }
  if {$arc_count == 0} {
    error "$cell_name has no timing arcs"
  }
}

# Verify ECSM-tagged library also participates in actual timing paths.
read_verilog liberty_ecsm_test.v
link_design liberty_ecsm_test

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_output_delay -clock clk 1.0 [get_ports out1]

with_output_to_variable max_rep {
  report_checks -path_delay max -from [get_ports in1] -to [get_ports out1]
}
if {![regexp {Startpoint:\s+in1} $max_rep]
    || ![regexp {Endpoint:\s+out1} $max_rep]
    || ![regexp {u1/} $max_rep]
    || ![regexp {u2/} $max_rep]
    || ![regexp {slack} $max_rep]} {
  error "ECSM max timing report missing expected path content"
}
puts "ecsm timing max: ok"

with_output_to_variable min_rep {
  report_checks -path_delay min -from [get_ports in1] -to [get_ports out1]
}
if {![regexp {Startpoint:\s+in1} $min_rep]
    || ![regexp {Endpoint:\s+out1} $min_rep]
    || ![regexp {u1/} $min_rep]
    || ![regexp {u2/} $min_rep]
    || ![regexp {slack} $min_rep]} {
  error "ECSM min timing report missing expected path content"
}
puts "ecsm timing min: ok"
