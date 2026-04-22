# Regression for Kahn's BFS narrowed discovery under clks_only.
#
# ArrivalVisitor with clks_only=true (the path exercised by
# Search::findClkArrivals) uses postponeClkFanouts to stop propagation
# at register CK boundaries. Kahn's Stage 1 discovery must mirror that
# stop via VertexVisitor::stopDiscoveryAtRegClk(); otherwise it eagerly
# walks the full data fanout past every reg CK.
#
# Dispatch count alone doesn't catch the regression on small designs:
# the wider Kahn's walk still drains through per-worker batches without
# exceeding the spill threshold, so the dispatch counter doesn't move.
# The direct observable is the arrival BFS visit count -- every vertex
# Kahn's Stage 1 discovers is visited exactly once in Stage 2, so the
# cumulative visit count tracks the active-set size one-to-one.

read_liberty ../../test/sky130hd/sky130hd_tt.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd

sta::set_thread_count 2
source ../../examples/gcd_sky130hd.sdc

# OFF: level-based BFS with clks_only. Level-BFS stops at reg CK via
# ArrivalVisitor's postponeClkFanouts, so the visit count is bounded
# to the clock network only.
sta::arrivals_invalid
set sta_use_kahns_bfs 0
set before [sta::arrival_visit_count]
report_clock_skew -setup > /dev/null
set off_visits [expr {[sta::arrival_visit_count] - $before}]

# ON: Kahn's with stop-at-reg-CK under clks_only. Stage 1 discovery
# mirrors the level-BFS narrowing, so visit count should be in the
# same ballpark as OFF. A regression that removes the stop will make
# Kahn's visit every data-path vertex downstream of reg CK and this
# number will jump sharply.
sta::arrivals_invalid
set sta_use_kahns_bfs 1
set before [sta::arrival_visit_count]
report_clock_skew -setup > /dev/null
set on_visits [expr {[sta::arrival_visit_count] - $before}]

puts "clks_only_off_visits=$off_visits"
puts "clks_only_on_visits=$on_visits"
puts [format "clks_only_on_to_off_ratio=%.2fx" \
          [expr {$on_visits / double([expr {$off_visits > 0 ? $off_visits : 1}])}]]
