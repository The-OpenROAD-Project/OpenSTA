# Report pin activities
proc report_activities { } {
  set pins [get_pins -hierarchical *]
  set clk_freq [expr 1.0 / (10 * 1e-12)]
  puts "Pin Name Activity Duty Cycle"
  puts "--------------------------------------------------------"
  foreach pin $pins {
    set prop [get_property $pin activity]
    set transitions_per_sec [lindex $prop 0]
    set duty [lindex $prop 1]
    set activity [expr double($transitions_per_sec) / [expr $clk_freq * 2]]
    puts "[get_full_name $pin] $activity $duty"
  }
  puts ""
}

# Setup
read_liberty asap7_invbuf.lib.gz
read_verilog vcd_begin_end_time.v
link_design top

# Define clock period in ps
create_clock -name vclk -period 10

# Full VCD reading works (normal behavior)
# VCD changes at time 50 and 100 (inverter)
sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top
report_activities

# Read VCD from start to first transition point
sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -end_time 50
report_activities

# Read VCD from first transition point to second transition point
sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -begin_time 50 -end_time 100
report_activities

# Read VCD from second transition point to end
sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -begin_time 100
report_activities

# Read VCD around the first transition point
sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -begin_time 40 -end_time 60
report_activities

sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -begin_time 20 -end_time 60
report_activities

sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -begin_time 40 -end_time 80
report_activities

# Read VCD around the second transition point (should mirror the first)
sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -begin_time 90 -end_time 110
report_activities

sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -begin_time 70 -end_time 110
report_activities

sta::clear_power
read_vcd vcd_begin_end_time.vcd -scope top -begin_time 90 -end_time 130
report_activities
