# Vectorless activity of enable flops whose next_state holds the state.
# For next_state "(D&DE) | (IQ&!DE)", Q holds D from the last cycle with
# DE = 1, so its duty is P(D). Clock period 6ns.
read_liberty power_enable_flop.lib
read_verilog power_enable_flop.v
link_design top

set_cmd_units -time ns
create_clock -name clk -period 6 [get_ports clk]

proc show { pin } {
  lassign [get_property [get_pins $pin] activity] density duty origin
  puts [format "%-6s toggles/cycle %.4f duty %.3f %s" \
          $pin [expr {$density * 6e-9}] $duty $origin]
}

foreach d_duty {0.5 0.2 0.8} {
  puts "P(D) $d_duty P(DE) 0.1"
  set_power_activity -input_ports [get_ports d] -density 0.08 -duty $d_duty
  set_power_activity -input_ports [get_ports de] -density 0.02 -duty 0.1
  show r/Q
  show rb/Q_N
}

# Enable never on: the state never changes.
puts "DE constant 0"
set_case_analysis 0 [get_ports de]
show r/Q

# Enable always on: a plain register, so Q follows the rate of D.
puts "DE constant 1, D 0.012/cycle"
set_case_analysis 1 [get_ports de]
set_power_activity -input_ports [get_ports d] -density 0.002 -duty 0.5
show r/Q
show rb/Q_N
