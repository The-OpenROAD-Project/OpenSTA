# Guard for stale path handles across a search update (OpenROAD #10210).
# Holding a PathEnd past the next query must error cleanly, not crash.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

# Second query = find_timing_paths.
set ends [find_timing_paths -through u1/Z]
set pe [lindex $ends 0]
set junk [find_timing_paths -through u2/ZN]
catch { puts "stale : slack=[$pe slack] arrival=[[$pe path] arrival]" } msg
puts $msg

# Second query = report_checks.
set ends [find_timing_paths -through u1/Z]
set pe [lindex $ends 0]
report_checks -path_delay max
catch { puts "stale : slack=[$pe slack] arrival=[[$pe path] arrival]" } msg
puts $msg
