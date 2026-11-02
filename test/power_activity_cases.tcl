# Vectorless activity propagation cases.
# Clock period 10ns, so a density of 1e8 is one transition per cycle.
read_liberty asap7_simple.lib.gz
read_liberty asap7_seq.lib.gz
read_liberty asap7_invbuf.lib.gz
read_liberty power_activity_cases.lib
read_verilog power_activity_cases.v
link_design power_activity_cases
set_cmd_units -time ns
create_clock -name clk -period 10 [get_ports clk]
# Fast virtual clock; must not change the sampling period of clk logic.
create_clock -name vclk -period 1
create_generated_clock -name clk_div -source [get_ports clk] -edges {1 2 5} \
  [get_pins u_buf/Y]
set_input_delay 0 -clock clk [all_inputs -no_clocks]
set_case_analysis 0 [get_ports se]

set_power_activity -input_ports {a b} -density 0.05 -duty 0.5
set_power_activity -input_ports {d en} -density 0.01 -duty 0.25
set_power_activity -input -density 0.02 -duty 0.5
set_power_activity -pins u_bb1/Z -density 0.03 -duty 0.5
# More than one transition per cycle, as in a glitchy vcd.
set_power_activity -pins r_glitch/D -density 0.3 -duty 0.25

proc show { pin } {
  puts "$pin [get_property [get_pins $pin] activity]"
}

# Simultaneous input transitions: XOR2 0.5/cycle, AND2 0.375/cycle.
show u_xor/Y
show u_and/Y
# Gated clock: clock transitions while enabled, 2/cycle * 0.25.
show u_icg/GCLK
# Register output follows D, limited to min(duty, 1 - duty) * clk density
# (r_glitch: 0.5/cycle).
show r_reg/Q
show r_glitch/Q
# Generated clock duty from its own waveform (0.25).
show r_gen/CLK
# Inverted state output: duty 1 - 0.25.
show r_niq/QN
# Functionless outputs: annotated activity, else default input activity.
show u_inv1/Y
show u_inv2/Y
# Internal power: the "!B" group cannot switch Z from A.
report_power -instances u_when -digits 4
# Constant input.
set_case_analysis 0 [get_ports b]
show u_and/Y
show u_xor/Y
# Annotated driver above one transition per cycle: its load keeps the
# annotated rate, the inverter output it drives is limited.
set_power_activity -pins u_bb2/Z -density 0.3 -duty 0.5
show u_inv2/A
show u_inv2/Y
