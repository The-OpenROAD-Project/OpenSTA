# Regression for Kahn's BFS vs reference-pin input delay.
#
# set_input_delay -reference_pin causes ArrivalVisitor::visit() to fire
# enqueueRefPinInputDelays() while propagating a clock arrival. That
# call pushes new vertices into the arrival iterator's queue_ during
# Stage 2 with bfsInQueue=true. The Kahn Phase 3 post-finishTasks
# cleanup has to keep those entries -- a blind queue_[level].clear()
# drops the pointer but leaves the flag stuck at true, making every
# future enqueue() of the same vertex a silent no-op and causing
# missed propagation on any subsequent invalidate + report_checks.
#
# Test flow:
#   1. Baseline report_checks with Kahn's OFF.
#   2. Invalidate arrivals; run report_checks with Kahn's ON.
#      During Stage 2, enqueueRefPinInputDelays(clk) pushes the in1
#      vertex into queue_; the cleanup fix must preserve it instead
#      of orphaning its bfsInQueue flag.
#   3. Invalidate arrivals again; run report_checks with Kahn's OFF.
#      If step 2 orphaned in1, arrivals_invalid's enqueue is a no-op,
#      level-BFS runs without in1 as a seed, and the report differs
#      from the baseline.
#
# The test asserts that steps 1 and 3 produce byte-identical reports,
# which is only possible if Kahn's cleanup preserved the ref-pin
# vertex correctly.

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

sta::set_thread_count 2

create_clock -name clk -period 10 [get_ports clk]
# in1 carries an input delay whose ARRIVAL reference is the clk pin.
# When clk propagates and ArrivalVisitor::visit() sees is_clk=true on
# clk's vertex, it calls enqueueRefPinInputDelays(clk, sdc) which
# enqueues the in1 vertex into the arrival iterator.
set_input_delay -clock clk -reference_pin [get_ports clk] 1.5 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

proc capture_checks {} {
  sta::redirect_string_begin
  report_checks -path_delay max -group_count 10
  return [sta::redirect_string_end]
}

# Step 1: baseline, Kahn's OFF.
set sta_use_kahns_bfs 0
set baseline [capture_checks]

# Step 2: full re-propagation under Kahn's ON. This is the call that
# exercises enqueueRefPinInputDelays during Stage 2 and depends on
# the post-finishTasks cleanup to preserve the ref-pin vertex's
# bfsInQueue flag. The captured output is discarded; we only care
# about the side effects on bfsInQueue state.
sta::arrivals_invalid
set sta_use_kahns_bfs 1
capture_checks

# Step 3: fall back to OFF and re-propagate. If Kahn's orphaned in1,
# the arrivals_invalid below is a silent no-op for that vertex, and
# level-BFS runs without in1 as a seed -- changing the report.
sta::arrivals_invalid
set sta_use_kahns_bfs 0
set after_kahn [capture_checks]

if { $baseline eq $after_kahn } {
  puts "refpin_preservation=ok"
} else {
  puts "refpin_preservation=FAIL"
  puts "--- BASELINE (Kahn's OFF, first run) ---"
  puts $baseline
  puts "--- AFTER KAHN (Kahn's OFF, after ON-then-invalidate) ---"
  puts $after_kahn
}
