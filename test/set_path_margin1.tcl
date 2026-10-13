# set_path_margin -to with -setup/-hold

read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

proc setup_at { args } {
  report_checks {*}$args -path_delay max -digits 4 -fields {} -group_path_count 1
}
proc hold_at { args } {
  report_checks {*}$args -path_delay min -digits 4 -fields {} -group_path_count 1
}
proc report_json_path_margin { label path_delay args } {
  set cmd [concat [list report_checks] $args \
    [list -path_delay $path_delay -format json -group_path_count 1]]
  with_output_to_variable json $cmd
  if { [regexp {"path_margin": ([^,\n]+)} $json match margin] } {
    puts "$label $margin"
  } else {
    puts "$label none"
  }
}

set_path_margin -setup 0.50 -comment {tighten setup time} -to [get_pins r3/D]
setup_at -to [get_pins r3/D]
report_json_path_margin setup_to_tighten max -to [get_pins r3/D]
set_path_margin -hold 0.50 -comment {tighten hold time} -to [get_pins r3/D]
hold_at -to [get_pins r3/D]
report_json_path_margin hold_to_tighten min -to [get_pins r3/D]
set_path_margin -setup -67 -comment {loosen setup time} -to [get_pins r3/D]
setup_at -to [get_pins r3/D]
report_json_path_margin setup_to_loosen max -to [get_pins r3/D]
set_path_margin -hold -0.50 -comment {loosen hold time} -to [get_pins r3/D]
hold_at -to [get_pins r3/D]
report_json_path_margin hold_to_loosen min -to [get_pins r3/D]
