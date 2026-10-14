# ICG with a tied-high enable must still clock the flops behind it.
# The CLK->GCLK "when: ENA+(!ENA*SE)" arc set only has the fall arc,
# so it must not disable the rising arc of the default set.
# OpenROAD issue #11751.
suppress_msg 1212
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_verilog search_icg_tied_enable.v
link_design top
create_clock -name clk -period 1000 [get_ports clk]
set_propagated_clock [all_clocks]
set_input_delay 0 -clock clk [get_ports {d1 d2}]

puts "--- ENA=1 SE=0: r1 clocked through g1/GCLK ---"
report_checks -to r1/D -format full_clock_expanded
report_edges -from g1/CLK -to g1/GCLK

puts "--- ENA=0 SE=0: gate off, r2 unclocked ---"
report_checks -to r2/D -format full_clock_expanded
report_edges -from g2/CLK -to g2/GCLK

puts "--- disabled edges ---"
report_disabled_edges
