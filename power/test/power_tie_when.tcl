# Tie cells holding a flop's set/reset at 1 and 0, with when-conditioned clock power.
# From parallaxsw/OpenSTA#539. A tie output is a constant (activity 0, duty 1 or 0),
# so each tied flop matches the same flop driven from a port held at that value.
read_liberty tie_when.lib
read_verilog tie_when.v
link_design top
create_clock -name clk -period 1 [get_ports clk]
set_power_activity -input_ports [get_ports set_n] -activity 0 -duty 1
set_power_activity -input_ports [get_ports rst] -activity 0 -duty 0
foreach pin {tie/Y ff_tied/SETN tie_lo/Y ff_tied_lo/RST} {
  puts "$pin [get_property [get_pins $pin] activity]"
}
# One instance per report: equal powers have no defined order in a report.
foreach inst {ff_tied ff_input ff_tied_lo ff_input_lo} {
  report_power -instances [get_cells $inst]
}
# An annotation does not override a constant.
set_power_activity -pins [get_pins tie/Y] -activity 0.5 -duty 0
puts "tie/Y [get_property [get_pins tie/Y] activity]"
