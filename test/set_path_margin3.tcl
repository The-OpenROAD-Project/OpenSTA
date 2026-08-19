# set_path_margin -from and -to

read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

proc setup_at { args } {
  report_checks {*}$args -path_delay max -digits 4 -fields {} -group_path_count 1
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

set_path_margin -setup 5.0 -from [get_pins r1/CK] -to [get_pins r3/D]
# Should see path margin.
setup_at -from [get_pins r1/CK] -to [get_pins r3/D]
report_json_path_margin setup_from_to_r1 max -from [get_pins r1/CK] -to [get_pins r3/D]
# Should not see path margin.
setup_at -from [get_pins r2/CK] -to [get_pins r3/D]
report_json_path_margin setup_from_to_r2 max -from [get_pins r2/CK] -to [get_pins r3/D]
