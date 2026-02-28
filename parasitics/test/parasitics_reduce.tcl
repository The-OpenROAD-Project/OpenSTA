# Test parasitic reduction and various delay calculators with parasitics

# Read ASAP7 libraries
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 [get_ports out]
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
set_propagated_clock {clk1 clk2 clk3}

#---------------------------------------------------------------
# Read SPEF with reduce=true (default)
#---------------------------------------------------------------
puts "--- read_spef with reduction ---"
read_spef ../../test/reg1_asap7.spef

#---------------------------------------------------------------
# Run timing with different delay calculators to exercise
# parasitic reduction paths
#---------------------------------------------------------------

# Default (dmp_ceff_elmore) - exercises reduce for Pi/Elmore
puts "--- dmp_ceff_elmore with reduced parasitics ---"
report_checks

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

report_checks -fields {slew cap input_pins}

# Arnoldi - exercises arnoldi reduction
puts "--- arnoldi with parasitics (reduction) ---"
set_delay_calculator arnoldi

report_checks

report_checks -from [get_ports in1] -to [get_ports out] -fields {slew cap}

report_checks -from [get_ports in2] -to [get_ports out] -fields {slew cap}

# More detailed report_dcalc to exercise parasitic queries
set msg [report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max]
puts "arnoldi dcalc u1: $msg"

set msg [report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max]
puts "arnoldi dcalc u2 A->Y: $msg"

set msg [report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max]
puts "arnoldi dcalc u2 B->Y: $msg"

set msg [report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max]
puts "arnoldi dcalc r1 CLK->Q: $msg"

set msg [report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max]
puts "arnoldi dcalc r2 CLK->Q: $msg"

set msg [report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max]
puts "arnoldi dcalc r3 CLK->Q: $msg"

# Prima - exercises prima reduction paths
puts "--- prima with parasitics ---"
set msg [set_delay_calculator prima]
puts "set_delay_calculator prima: $msg"

report_checks

report_checks -from [get_ports in1] -to [get_ports out] -fields {slew cap}

set msg [report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max]
puts "prima dcalc u1: $msg"

set msg [report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max]
puts "prima dcalc u2: $msg"

set msg [report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max]
puts "prima dcalc r1: $msg"

# dmp_ceff_two_pole - exercises two_pole reduction
puts "--- dmp_ceff_two_pole with parasitics ---"
set_delay_calculator dmp_ceff_two_pole

report_checks

report_checks -path_delay min

set msg [report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max]
puts "dmp_ceff_two_pole dcalc u1: $msg"

set msg [report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max]
puts "dmp_ceff_two_pole dcalc u2: $msg"

set msg [report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max]
puts "dmp_ceff_two_pole dcalc r1: $msg"

#---------------------------------------------------------------
# Lumped cap
#---------------------------------------------------------------
puts "--- lumped_cap with parasitics ---"
set_delay_calculator lumped_cap

report_checks

set msg [report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max]
puts "lumped_cap dcalc u1: $msg"

report_checks -fields {slew cap}

#---------------------------------------------------------------
# Annotated delay reporting
#---------------------------------------------------------------
puts "--- annotated delay reporting ---"
set_delay_calculator dmp_ceff_elmore

set msg [report_annotated_delay -cell -net]
puts "annotated_delay -cell -net: $msg"

set msg [report_annotated_delay -from_in_ports -to_out_ports]
puts "annotated_delay -from_in_ports -to_out_ports: $msg"

set msg [report_annotated_delay -cell]
puts "annotated_delay -cell: $msg"

set msg [report_annotated_delay -net]
puts "annotated_delay -net: $msg"

set msg [report_annotated_delay -report_annotated]
puts "annotated_delay -report_annotated: $msg"

set msg [report_annotated_delay -report_unannotated]
puts "annotated_delay -report_unannotated: $msg"

#---------------------------------------------------------------
# Report net with -digits on various nets
#---------------------------------------------------------------
puts "--- report_net with various nets ---"
foreach net_name {r1q r2q u1z u2z out in1 in2} {
  report_net -digits 4 $net_name
  puts "report_net $net_name: done"
}
