# liberty default_max_capacitance applies to output pins without max_capacitance
read_liberty liberty_default_max_cap.lib
read_verilog liberty_default_max_cap.v
link_design top
set_load 0.08 [get_ports out]
report_check_types -max_capacitance -verbose
