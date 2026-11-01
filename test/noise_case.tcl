# write_delay_noise_case
source helpers.tcl
suppress_msg 198 1579
read_liberty ../examples/sky130hd_tt.lib.gz
read_verilog ../examples/gcd_sky130hd.v
link_design gcd
read_sdc ../examples/gcd_sky130hd.sdc
read_spef -keep_capacitive_coupling ../examples/gcd_sky130hd.spef

# Print the nets of a written case SPEF, skipping the header.
proc report_case_spef_nets { file } {
  set stream [open $file r]
  set in_nets 0
  while { [gets $stream line] >= 0 } {
    if { [string match "\*D_NET *" $line] } {
      set in_nets 1
    }
    if { $in_nets } {
      puts $line
    }
  }
  close $stream
}

# -min_coupling_ratio is accepted.
write_delay_noise_case _006_ [make_result_file noise_case1] \
  -rise -min_coupling_ratio 0.1 -no_timestamp

# Aggressor _417_ has an internal pin (IQ) with no net.
set case2 [make_result_file noise_case2]
write_delay_noise_case _006_ $case2 -rise \
  -min_coupling_sum_ratio 0.0001 -no_timestamp
report_file $case2.v
report_case_spef_nets $case2.spef

# Victim parasitic node is the output port resp_msg[1].
set case3 [make_result_file noise_case3]
write_delay_noise_case {resp_msg[1]} $case3 -rise \
  -min_coupling_sum_ratio 0.0001 -no_timestamp
report_file $case3.v
report_case_spef_nets $case3.spef
