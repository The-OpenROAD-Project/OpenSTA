# Corner-scope coverage audit. Every registered command must be
# classified with respect to analysis_corner scope:
#   - corner-whitelisted: writes the (mode, corner) overlay Sdc
#     (cmdCornerSdc swaps in sdc/Sdc.i) — listed here,
#   - guarded / pending: errors in corner scope
#     (sta::corner_scope_disallowed_cmds / corner_scope_pending_cmds),
#   - corner-neutral: everything else, snapshotted in the golden below.
# When an upstream merge adds a command, this test fails until the
# command is classified: add it to the guard list or whitelist, or
# accept it as corner-neutral by regenerating the golden (save_ok is
# the recorded audit decision).

# Source insertion has no command of its own: it is the -source form of
# set_clock_latency (set_clock_insertion_cmd in sdc/Sdc.i sits under it).
set whitelisted {
  set_timing_derate unset_timing_derate
  set_input_delay unset_input_delay
  set_output_delay unset_output_delay
  set_clock_uncertainty unset_clock_uncertainty
  set_clock_latency unset_clock_latency
}
set covered [concat $sta::corner_scope_disallowed_cmds \
               $sta::corner_scope_pending_cmds $whitelisted]

# Sanity: no command is in two classes, and each guarded/whitelisted
# name is a real command (documented-only stubs like set_ideal_net are
# expected to be absent and reported as such).
set seen [dict create]
set dup_count 0
foreach cmd $covered {
  if { [dict exists $seen $cmd] } { incr dup_count }
  dict set seen $cmd 1
}
puts "duplicate classifications: $dup_count"
set missing {}
foreach cmd [lsort $covered] {
  if { [info commands ::sta::$cmd] == "" } {
    lappend missing $cmd
  }
}
puts "listed but not a command: $missing"

# The audit: registered commands with no corner-scope classification.
puts "corner-neutral commands:"
foreach cmd [lsort [array names sta::cmd_args]] {
  if { ![dict exists $seen $cmd] } {
    puts "  $cmd"
  }
}
