# ccs_ceff delay calculator with a pi-model load.
# The RC is large enough that the Elmore exponential in the load waveform
# dominates the ceff solution.
read_liberty ../../test/asap7_ccsn.lib.gz
read_verilog dcalc_ccs_ceff.v
link_design top
read_spef dcalc_ccs_ceff.spef
set_input_transition 20 [all_inputs]
set_delay_calculator ccs_ceff
report_checks -unconstrained -fields {slew cap} -digits 3
report_dcalc -from u1/A1 -to u1/Y -digits 3
