# read_saif references missing instance
read_liberty read_saif_null_instance.lib
read_verilog read_saif_null_instance.v
link_design top
read_saif -scope TOP read_saif_null_instance.saif
