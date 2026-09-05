# connect_pin to a bus port, which has no pin.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog verilog_bus_port_connect.v
link_design top
connect_pin {a[0]} u/b
