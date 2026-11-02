# Vectorless activity propagation vs. zero-delay gate-level VCD activity
# on a registered 16x16 array multiplier. Inputs toggle with probability
# 0.1 per cycle (duty 0.5) in both cases.
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog power_mult16.v
link_design mult
create_clock -name clk -period 3 [get_ports clk]

set_power_activity -input -activity 0.1
set vectorless [lindex [sta::design_power [sta::cmd_scene]] 3]
unset_power_activity -input

read_vcd -scope tb/dut power_mult16_zd.vcd.gz
set vcd [lindex [sta::design_power [sta::cmd_scene]] 3]

puts [format "vectorless %.3e vcd %.3e ratio %.2f" $vectorless $vcd \
        [expr {$vectorless / $vcd}]]
