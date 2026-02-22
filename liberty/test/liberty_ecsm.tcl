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
