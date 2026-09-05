# Concatenation port expression in the module header.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog verilog_port_concat.v
link_design top
