# noise delay example
suppress_msg 1212 1645
read_liberty ../test/asap7_simple.lib.gz
read_verilog noise_delay1.v
link_design top
read_spef -keep_capacitive_coupling noise_delay1.spef
sta::set_delay_calculator prima
include noise_delay1.sdc

report_checks -from in1 -unconstrained \
  -path_delay min_max \
  -fields {input_pin noise_delay}

report_noise_delays
annotate_noise_delays

report_checks -from in1 -unconstrained \
  -path_delay min_max \
  -fields {input_pin noise_delay}
