# Performance regression: DispatchQueue dispatch() counts under Kahn's
# BFS OFF vs ON. The count is a wall-clock-independent proxy for
# parallel-dispatch overhead, so a shift in the golden signals a real
# change in the BFS/dispatch strategy.

read_liberty ../../test/sky130hd/sky130hd_tt.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd

sta::set_thread_count 2
source ../../examples/gcd_sky130hd.sdc

# OFF phase: level-based BFS.
set sta_use_kahns_bfs 0
set before [sta::dispatch_call_count]
report_checks -path_delay min_max -group_count 10 > /dev/null
set off_dispatches [expr {[sta::dispatch_call_count] - $before}]

# ON phase: Kahn's BFS. Invalidate arrivals so the iterators
# re-propagate through visitParallel instead of returning cached
# results.
sta::arrivals_invalid
set sta_use_kahns_bfs 1
set before [sta::dispatch_call_count]
report_checks -path_delay min_max -group_count 10 > /dev/null
set on_dispatches [expr {[sta::dispatch_call_count] - $before}]

puts "off_dispatches=$off_dispatches"
puts "on_dispatches=$on_dispatches"
puts [format "on_to_off_ratio=%.2fx" \
          [expr {$on_dispatches / double($off_dispatches)}]]
