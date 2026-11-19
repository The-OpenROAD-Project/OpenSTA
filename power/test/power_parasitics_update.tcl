# Power is recalculated after delay, slew and load changes.
# Covers read_spef, set_load and set_assigned_transition.
read_liberty ../../examples/sky130hd_tt.lib.gz
suppress_msg 198
read_verilog ../../examples/gcd_sky130hd.v.gz
link_design gcd
read_sdc ../../examples/gcd_sky130hd.sdc
set_propagated_clock clk
set_power_activity -input -activity .1
set_power_activity -input_port reset -activity 0

proc total {} {
  lassign [sta::design_power [sta::cmd_scene]] internal switching leakage total
  puts [format "internal %.4e switching %.4e total %.4e" \
          $internal $switching $total]
}
puts "No parasitics"
total
read_spef ../../examples/gcd_sky130hd.spef.gz
puts "After read_spef"
total
set_load 0.05 [get_nets _000_]
puts "After set_load"
total
set_assigned_transition 1.0 \
  [get_pins -of_objects [get_cells -filter "ref_name=~*dfxtp*"] \
     -filter "direction==input"]
puts "After set_assigned_transition"
total
