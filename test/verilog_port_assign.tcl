# verilog assignments preserve input connections.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog verilog_port_assign.v
link_design top
if {[get_full_name [get_nets -of_objects [get_pins g1/A]]] ne "a"
    || [get_full_name [get_nets -of_objects [get_pins g2/A]]] ne "b"} {
  puts "ordinary wire aliases lost independent input connections"
}
