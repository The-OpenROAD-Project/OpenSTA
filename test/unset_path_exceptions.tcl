# unset_path_exceptions/-reset_path with -rise/-fall and group_path
source helpers.tcl
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}

proc report_exceptions {} {
  set sdc_file [make_result_file unset_path_exceptions.sdc]
  write_sdc -no_timestamp $sdc_file
  set stream [open $sdc_file r]
  set sdc [string map {"\\\n" ""} [read $stream]]
  close $stream
  puts "--"
  foreach line [split $sdc "\n"] {
    if { [regexp {^(set_false_path|group_path)} $line] } {
      puts [string trim [regsub -all {\s+} $line " "]]
    }
  }
}

set_false_path -rise -from [get_cells r1]
unset_path_exceptions -fall -from [get_cells r1]
report_exceptions
unset_path_exceptions -rise -from [get_cells r1]
report_exceptions

set_false_path -fall -from [get_cells {r1 r2}]
unset_path_exceptions -from [get_cells r1]
report_exceptions
unset_path_exceptions -from [get_cells r2]

group_path -name g1 -from [get_ports in1]
group_path -name g2 -from [get_ports in2]
unset_path_exceptions -from [get_ports in1]
set_false_path -reset_path -from [get_ports in2]
report_exceptions
