# netlist edit moving a loop cut invalidates stale arrivals
read_liberty asap7_small.lib.gz
read_verilog levelize_loop_cut_move.v
link_design top
set_input_delay 0 [get_ports {in0 in1}]
report_checks -unconstrained -format end
disconnect_pin in0 g6/B
connect_pin in1 g6/B
report_checks -unconstrained -format end
# Deleting the g4 loop edge relevelizes and moves the g2 B loop cut to g6 A.
disconnect_pin n4 g4/A
report_checks -unconstrained
