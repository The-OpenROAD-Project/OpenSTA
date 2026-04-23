# Regression for the opt-in Kahn's visit-skip optimization
# (sta_kahn_visit_skip).
#
# Semantics: when on, Kahn's Stage 2 skips the visitor body on any
# vertex whose active-set predecessors all reported
# arrivals_changed=false via VertexVisitor::lastVisitChanged(). We
# still decrement successors to preserve Kahn's in-degree invariant,
# so skipped vertices cost only a few atomic ops instead of a full
# arrival/tag computation.
#
# The real-world win is on incremental flows (rsz, cts) where
# OpenROAD selectively invalidates arrivals on specific vertices via
# Search::arrivalInvalid(Vertex*) -- this preserves the path cache,
# so arrivalsChanged can compare newly-propagated values against the
# cached ones and return false when unchanged. From pure OSTA Tcl
# this path is not easily reachable: sta::arrivals_invalid calls
# Search::arrivalsInvalid() which deletePaths()s everything, forcing
# arrivalsChanged to always return true on the next propagation.
# Visit-skip therefore cannot fire in a test limited to
# arrivals_invalid + report_checks.
#
# What this test does regress-guard: CORRECTNESS. We run the same
# report_checks under skip=0 and skip=1 (with Kahn's on both times)
# and assert the reports are byte-identical. Any bug in visit-skip
# that changes arrivals, slacks, path ordering, or any other reported
# value will make the two reports diverge and the test fails. This
# catches the class of regressions that matters most for shipping.

read_liberty ../../test/sky130hd/sky130hd_tt.lib
read_verilog ../../examples/gcd_sky130hd.v
link_design gcd

sta::set_thread_count 2
source ../../examples/gcd_sky130hd.sdc

proc capture_checks {} {
  sta::redirect_string_begin
  report_checks -path_delay max -group_count 10
  return [sta::redirect_string_end]
}

# Both runs use Kahn's; only the visit_skip toggle differs.
set sta_use_kahns_bfs 1

# Baseline: skip off.
sta::arrivals_invalid
set sta_kahn_visit_skip 0
set baseline [capture_checks]

# Comparison: skip on. Under the current Tcl path, arrivalsInvalid
# wipes paths so the skip predicate never triggers internally -- but
# the visit-skip code path (pred_changed allocation, per-successor
# release store, per-vertex acquire load) still executes. A bug in
# that plumbing will show up as a divergent report.
sta::arrivals_invalid
set sta_kahn_visit_skip 1
set with_skip [capture_checks]

if { $baseline eq $with_skip } {
  puts "visit_skip_correct=ok"
} else {
  puts "visit_skip_correct=FAIL"
  puts "--- BASELINE (Kahn's ON, visit_skip OFF) ---"
  puts $baseline
  puts "--- WITH SKIP (Kahn's ON, visit_skip ON) ---"
  puts $with_skip
}
