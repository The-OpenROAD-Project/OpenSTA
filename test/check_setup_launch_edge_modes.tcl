# check_setup -no_clock looks only at the clock arrivals of the mode
# it checks. In m1 the clock is on clk, so only its falling edge
# reaches r/CLK, through b, and r launches on the rising edge: s1 must
# report r/CLK. In m2 the clock is on b/Z, so both edges reach r/CLK:
# s2 must not report r/CLK, and its rising arrival must not hide r/CLK
# in s1. f launches on the falling edge and is clocked in both modes.
read_liberty asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty check_setup_launch_edge.lib
read_verilog check_setup_launch_edge.v
link_design top
set_mode m1
create_clock -name clk -period 1000 [get_ports clk]
set_mode m2
create_clock -name clk -period 1000 [get_pins b/Z]
set libs {asap7sc7p5t_SEQ_RVT_FF_nldm_220123 fall_only}
define_scene s1 -mode m1 -liberty $libs
define_scene s2 -mode m2 -liberty $libs

proc unclocked { scene } {
  set_scene $scene
  # no_input_delay no_output_delay multiple_clock no_clock
  # unconstrained_endpoints loops generated_clocks
  foreach error [sta::check_timing_cmd 0 0 0 1 0 0 0] {
    if { [string match "*unclocked register*" [lindex $error 0]] } {
      return [lrange $error 1 end]
    }
  }
  return {}
}

set failed 0
set s1 [unclocked s1]
set s2 [unclocked s2]
if { [lsearch -exact $s1 r/CLK] < 0 } {
  puts "FAIL: s1 does not report r/CLK, which no m1 clock reaches on its launch edge"
  set failed 1
}
if { [lsearch -exact $s2 r/CLK] >= 0 } {
  puts "FAIL: s2 reports r/CLK, which the m2 clock reaches on its launch edge"
  set failed 1
}
foreach scene {s1 s2} {
  if { [lsearch -exact [set $scene] f/CLK] >= 0 } {
    puts "FAIL: $scene reports f/CLK, which is clocked"
    set failed 1
  }
}
exit $failed
