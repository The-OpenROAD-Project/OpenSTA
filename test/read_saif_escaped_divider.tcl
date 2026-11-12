# read_saif flat net name containing escaped divider
read_liberty read_saif_null_instance.lib
read_verilog read_saif_escaped_divider.v
link_design top
read_saif -scope top read_saif_escaped_divider.saif
