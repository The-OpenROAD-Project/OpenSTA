# Instances deleted one at a time from exceptions that name many of them,
# as set_max_delay -from/-to [all_registers] does: the exceptions keep
# applying to the instances that remain, exactly as before, and an
# exception left with no -to instance is deleted.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog sdc_delete_exception_insts.v
link_design sdc_delete_exception_insts
create_clock -name clk -period 1000 [get_ports clk]
# -to instances, -from instances, and instances -through (whose pin pairs
# are indexed by pin).
set_max_delay 100 -from [all_inputs] -to [get_cells {r1 r2 r3 r4}]
set_max_delay 200 -from [get_cells {r1 r2 r3 r4}] -to [all_outputs]
set_max_delay 50 -through [get_cells {ob1 ob4}] -to [get_ports {out1 out4}]

# The slack of the worst path, or "none" when no path is found.
proc slack { args } {
  set paths [find_timing_paths {*}$args]
  if { $paths eq {} } {
    return none
  }
  return [get_property [lindex $paths 0] slack]
}
proc endpoint_slacks {} {
  set slacks {}
  foreach reg {r2 r3 r4} {
    lappend slacks [slack -to $reg/D] [slack -from $reg -to [get_ports out[string index $reg 1]]]
  }
  lappend slacks [slack -through [get_cells ob4] -to [get_ports out4]]
  return $slacks
}

set failed 0
proc check { what ok } {
  global failed
  if { !$ok } {
    puts "FAIL: $what"
    set failed 1
  }
}

# Every path is under its set_max_delay, not the 1000 period: its slack
# is less than half the period.
set before [endpoint_slacks]
foreach s $before {
  check "slack $s is not from a set_max_delay" \
    [expr { [string is double $s] && $s < 500 }]
}

# Delete one instance of each exception: the rest keep their exceptions.
delete_instance r1
delete_instance ob1
set after [endpoint_slacks]
check "slacks changed by deleting r1 and ob1: $before -> $after" \
  [expr { $before eq $after }]

# A -to left with no instance: the exception goes, its paths are timed
# against the clock.
delete_instance r2
delete_instance r3
set s [slack -to r4/D]
check "r4 lost its -to exception while it is named: slack $s" \
  [expr { [string is double $s] && $s < 500 }]
delete_instance r4
set sdc_file [make_result_file sdc_delete_exception_insts.sdc]
write_sdc -no_timestamp $sdc_file
set f [open $sdc_file]
set text [read $f]
close $f
# With every register gone, both register exceptions are gone; the
# -through exception, whose ob4 remains, is kept.
check "register exceptions kept with no register left, or -through lost:\n$text" \
  [expr { [regexp -all {set_max_delay} $text] == 1
          && [string first {-through [get_cells {ob4}]} $text] >= 0 }]
exit $failed
