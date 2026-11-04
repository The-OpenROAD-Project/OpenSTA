# Incremental delay calc thru output->output timing arc (FAx1 CON->SN).
suppress_msg 1212
read_liberty asap7_simple.lib.gz
read_verilog incr_dcalc_out_to_out_arc.v
link_design top
report_checks -unconstrained
set_load 5 [get_ports con]
report_checks -unconstrained
