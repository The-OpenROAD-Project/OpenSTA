# Guard for stale path handles across a search update (OpenROAD #10210).
# Holding a PathEnd, or a Path saved separately, past the next query must error
# cleanly, not crash.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog ../examples/example1.v
link_design top
create_clock -name clk -period 10 {clk1 clk2 clk3}
set_input_delay -clock clk 0 {in1 in2}

report_checks

# Stale PathEnd, second query = find_timing_paths.
set pe [lindex [find_timing_paths -through u1/Z] 0]
find_timing_paths -through u2/ZN
catch { puts "stale PathEnd : slack=[$pe slack]" } msg
puts $msg

# Stale PathEnd, second query = report_checks.
set pe [lindex [find_timing_paths -through u1/Z] 0]
report_checks -path_delay max
catch { puts "stale PathEnd : slack=[$pe slack]" } msg
puts $msg

# Stale Path saved separately (set p [$pe path]) across the next query.
set pe [lindex [find_timing_paths -through u1/Z] 0]
set p [$pe path]
find_timing_paths -through u2/ZN
catch { puts "stale Path : arrival=[$p arrival]" } msg
puts $msg

# Stale VertexPathIterator held across the next query. Iterate the filtered
# endpoint vertex, whose paths_ array the next query frees/reallocates.
set pe [lindex [find_timing_paths -through u1/Z] 0]
set it [[$pe vertex] path_iterator rise max]
find_timing_paths -through u2/ZN
catch { while {[$it has_next]} { set p [$it next]; puts "stale PathIter : arrival=[$p arrival]" } } msg
puts $msg
