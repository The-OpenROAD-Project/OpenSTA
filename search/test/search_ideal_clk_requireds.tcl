# Required times are not propagated from register clock pins back into an
# ideal clock network (OpenROAD fork). Clock network pins above the
# register clock pins report INF slack with ideal clocks and finite slack
# with propagated clocks. Register clock pins, data pins and clock gating
# enables (ICG and inferred AND gate) keep exact slacks, including after
# incremental resizes of a gate on the enable path and of a register.
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_ideal_clk_requireds.v
link_design search_ideal_clk_requireds
create_clock -name clk -period 0.3 [get_ports clk]
set_input_delay -clock clk 0 [get_ports {in1 in2}]
set_output_delay -clock clk 0 [get_ports {out1 out2}]

proc report_pin_slacks { title } {
  puts "--- $title ---"
  foreach pin {ckbuf/A ckbuf/Z icg/CK cg/A1 r1/CK r2/CK r1/Q r2/Q \
                 u2/Z icg/E u3/Z cg/A2 r3/D r4/D} {
    set p [get_pins $pin]
    puts [format "%-8s max %-10s min %s" $pin \
            [get_property $p slack_max] [get_property $p slack_min]]
  }
  report_wns
  report_tns
  report_worst_slack -min
  report_checks -to [get_pins {icg/E cg/A2}] -group_path_count 2 -format end
  report_checks -to [get_pins {icg/E cg/A2}] -path_delay min \
    -group_path_count 2 -format end
}

report_pin_slacks "ideal"
replace_cell u1 BUF_X4
report_pin_slacks "ideal, resized u1"
replace_cell r1 DFF_X2
report_pin_slacks "ideal, resized r1"
set_propagated_clock [all_clocks]
report_pin_slacks "propagated"
unset_propagated_clock [all_clocks]
report_pin_slacks "ideal again"
