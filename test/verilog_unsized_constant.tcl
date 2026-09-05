# Unsized constant in a port connection.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog verilog_unsized_constant.v
link_design top
