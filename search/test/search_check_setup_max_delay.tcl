# A macro block constrains its boundary paths with set_max_delay and no
# input/output delays (an optimization target, as in ORFS's
# flow/platforms/asap7/constraints.sdc). check_setup must not report
# the ports or endpoints set_max_delay covers: d, q and r/D. It must
# still report e, p and u/D, which have no constraint at all.
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_verilog search_check_setup_max_delay.v
link_design top
create_clock -name clk -period 1000 [get_ports clk]
set_max_delay 800 -from [get_ports d] -to [get_cells r]
set_max_delay 800 -from [get_cells r] -to [get_ports q]

check_setup -verbose -no_input_delay -no_output_delay \
  -unconstrained_endpoints
