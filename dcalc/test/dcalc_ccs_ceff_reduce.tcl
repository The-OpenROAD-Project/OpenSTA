# ccs_ceff report_dcalc must not discard the driver's reduced pi model.
# With read_spef -reduce there is no RC network to re-reduce from, so a
# report_checks after report_dcalc must still see the wire load on u1/Y.
read_liberty ../../test/asap7_ccsn.lib.gz
read_verilog dcalc_ccs_ceff.v
link_design top
read_spef -reduce dcalc_ccs_ceff.spef
set_input_transition 20 [all_inputs]
set_delay_calculator ccs_ceff
report_checks -unconstrained -fields {slew cap} -digits 3
report_dcalc -from u1/A1 -to u1/Y -digits 3
report_checks -unconstrained -fields {slew cap} -digits 3
