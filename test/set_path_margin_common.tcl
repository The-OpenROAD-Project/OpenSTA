# Common setup for the set_path_margin tests.

read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

proc report_path_margin { args } {
  report_checks {*}$args
  with_output_to_variable json [list report_checks {*}$args -format json]
  if { [regexp {"path_margin": ([^,\n]+)} $json match margin] } {
    puts "path_margin $margin"
  } else {
    puts "path_margin none"
  }
}
