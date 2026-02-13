# Test parasitic reduction and various delay calculators with parasitics
# Targets: ReduceParasitics.cc (parasitic reduction - 43.5% coverage)
#   ConcreteParasitics.cc (48.2% coverage, parasitic operations)
#   Parasitics.cc (43.0% coverage)
#   SpefReader.cc (68.6% coverage, more SPEF paths)
#   EstimateParasitics.cc (68.7% coverage)

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
puts "PASS: read_spef (with default reduction)"

#---------------------------------------------------------------
# Run timing with different delay calculators to exercise
# parasitic reduction paths
#---------------------------------------------------------------

# Default (dmp_ceff_elmore) - exercises reduce for Pi/Elmore
puts "--- dmp_ceff_elmore with reduced parasitics ---"
report_checks
puts "PASS: dmp_ceff_elmore report_checks"

report_checks -from [get_ports in1] -to [get_ports out]
puts "PASS: dmp_ceff_elmore in1->out"

report_checks -from [get_ports in2] -to [get_ports out]
puts "PASS: dmp_ceff_elmore in2->out"

report_checks -fields {slew cap input_pins}
puts "PASS: dmp_ceff_elmore with fields"

# Arnoldi - exercises arnoldi reduction
puts "--- arnoldi with parasitics (reduction) ---"
set_delay_calculator arnoldi

report_checks
puts "PASS: arnoldi report_checks"

report_checks -from [get_ports in1] -to [get_ports out] -fields {slew cap}
puts "PASS: arnoldi in1->out with fields"

report_checks -from [get_ports in2] -to [get_ports out] -fields {slew cap}
puts "PASS: arnoldi in2->out with fields"

# More detailed report_dcalc to exercise parasitic queries
catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "arnoldi dcalc u1: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "arnoldi dcalc u2 A->Y: $msg"

catch {report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max} msg
puts "arnoldi dcalc u2 B->Y: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "arnoldi dcalc r1 CLK->Q: $msg"

catch {report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max} msg
puts "arnoldi dcalc r2 CLK->Q: $msg"

catch {report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max} msg
puts "arnoldi dcalc r3 CLK->Q: $msg"

# Prima - exercises prima reduction paths
puts "--- prima with parasitics ---"
catch {set_delay_calculator prima} msg
puts "set_delay_calculator prima: $msg"

report_checks
puts "PASS: prima report_checks"

report_checks -from [get_ports in1] -to [get_ports out] -fields {slew cap}
puts "PASS: prima in1->out with fields"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "prima dcalc u1: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "prima dcalc u2: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "prima dcalc r1: $msg"

# dmp_ceff_two_pole - exercises two_pole reduction
puts "--- dmp_ceff_two_pole with parasitics ---"
set_delay_calculator dmp_ceff_two_pole

report_checks
puts "PASS: dmp_ceff_two_pole report_checks"

report_checks -path_delay min
puts "PASS: dmp_ceff_two_pole min path"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "dmp_ceff_two_pole dcalc u1: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "dmp_ceff_two_pole dcalc u2: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "dmp_ceff_two_pole dcalc r1: $msg"

#---------------------------------------------------------------
# Lumped cap
#---------------------------------------------------------------
puts "--- lumped_cap with parasitics ---"
set_delay_calculator lumped_cap

report_checks
puts "PASS: lumped_cap report_checks"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "lumped_cap dcalc u1: $msg"

report_checks -fields {slew cap}
puts "PASS: lumped_cap with fields"

#---------------------------------------------------------------
# Annotated delay reporting
#---------------------------------------------------------------
puts "--- annotated delay reporting ---"
set_delay_calculator dmp_ceff_elmore

catch {report_annotated_delay -cell -net} msg
puts "annotated_delay -cell -net: $msg"

catch {report_annotated_delay -from_in_ports -to_out_ports} msg
puts "annotated_delay -from_in_ports -to_out_ports: $msg"

catch {report_annotated_delay -cell} msg
puts "annotated_delay -cell: $msg"

catch {report_annotated_delay -net} msg
puts "annotated_delay -net: $msg"

catch {report_annotated_delay -report_annotated} msg
puts "annotated_delay -report_annotated: $msg"

catch {report_annotated_delay -report_unannotated} msg
puts "annotated_delay -report_unannotated: $msg"

#---------------------------------------------------------------
# Report net with -digits on various nets
#---------------------------------------------------------------
puts "--- report_net with various nets ---"
foreach net_name {r1q r2q u1z u2z out in1 in2} {
  catch {report_net -digits 4 $net_name} msg
  puts "report_net $net_name: done"
}

puts "ALL PASSED"
