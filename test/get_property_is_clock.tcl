# Pin is_clock property with and without liberty ports.
read_liberty asap7_small.lib.gz
read_verilog get_property_is_clock.v
link_design top
# Hierarchical (b1/*) and black box (bb1/*) pins have no liberty port.
report_object_full_names [get_pins -hierarchical -filter is_clock *]
