# find_pin on a bundle port, which has no pin.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog verilog_port_bundle_find_pin.v
link_design top
puts [[get_cells u] find_pin px]
