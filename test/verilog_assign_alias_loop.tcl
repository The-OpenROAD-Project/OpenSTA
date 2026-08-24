# Continuous assigns that alias two nets in both directions.
# Nothing is reported; the test is that link_design terminates.
read_liberty ../examples/nangate45_typ.lib.gz
read_verilog verilog_assign_alias_loop.v
link_design top
report_object_full_names [get_nets *]
