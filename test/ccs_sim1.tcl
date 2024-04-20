# ccs_sim parallel inverters
read_liberty asap7_invbuf.lib.gz
read_verilog ccs_sim1.v
link_design top
read_spef ccs_sim1.spef
set_input_transition 20 in1 
sta::set_delay_calculator ccs_sim
report_checks -fields {input_pins slew} -unconstrained -digits 3
