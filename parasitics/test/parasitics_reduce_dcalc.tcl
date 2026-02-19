# Test parasitic reduction with different delay calculators
# Targets: ReduceParasitics.cc (parasitic reduction algorithms)
#   ConcreteParasitics.cc (parasitic network operations, node/resistor/cap creation)
#   Parasitics.cc (parasitic find/make/delete operations)
#   SpefReader.cc (SPEF parsing with parasitic network)
#   EstimateParasitics.cc (estimated parasitic paths)

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
# Timing without parasitics (baseline)
#---------------------------------------------------------------
puts "--- baseline without parasitics ---"
report_checks

report_checks -path_delay min

#---------------------------------------------------------------
# Read SPEF - exercises parasitic network creation and reduction
#---------------------------------------------------------------
puts "--- read SPEF ---"
read_spef ../../test/reg1_asap7.spef

#---------------------------------------------------------------
# Test dmp_ceff_elmore (default)
# Exercises Elmore reduction paths
#---------------------------------------------------------------
puts "--- dmp_ceff_elmore ---"
report_checks

report_checks -path_delay min

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -from [get_ports in2] -to [get_ports out]

report_checks -fields {slew cap input_pins nets fanout}

# Detailed dcalc
report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "dmp_ceff_elmore dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "dmp_ceff_elmore dcalc u2 A: done"

report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max
puts "dmp_ceff_elmore dcalc u2 B: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "dmp_ceff_elmore dcalc r1: done"

report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max
puts "dmp_ceff_elmore dcalc r2: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max
puts "dmp_ceff_elmore dcalc r3: done"

#---------------------------------------------------------------
# Switch to Arnoldi - exercises Arnoldi reduction
#---------------------------------------------------------------
puts "--- arnoldi ---"
set_delay_calculator arnoldi

report_checks

report_checks -path_delay min

report_checks -from [get_ports in1] -to [get_ports out] -fields {slew cap}

report_checks -from [get_ports in2] -to [get_ports out] -fields {slew cap}

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "arnoldi dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "arnoldi dcalc u2 A: done"

report_dcalc -from [get_pins u2/B] -to [get_pins u2/Y] -max
puts "arnoldi dcalc u2 B: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "arnoldi dcalc r1: done"

report_dcalc -from [get_pins r2/CLK] -to [get_pins r2/Q] -max
puts "arnoldi dcalc r2: done"

report_dcalc -from [get_pins r3/CLK] -to [get_pins r3/Q] -max
puts "arnoldi dcalc r3: done"

#---------------------------------------------------------------
# Switch to dmp_ceff_two_pole
#---------------------------------------------------------------
puts "--- dmp_ceff_two_pole ---"
set_delay_calculator dmp_ceff_two_pole

report_checks

report_checks -path_delay min

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "dmp_ceff_two_pole dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "dmp_ceff_two_pole dcalc u2: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "dmp_ceff_two_pole dcalc r1: done"

#---------------------------------------------------------------
# Switch to lumped_cap
#---------------------------------------------------------------
puts "--- lumped_cap ---"
set_delay_calculator lumped_cap

report_checks

report_checks -path_delay min

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "lumped_cap dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "lumped_cap dcalc u2: done"

report_checks -fields {slew cap}

#---------------------------------------------------------------
# Switch to prima
#---------------------------------------------------------------
puts "--- prima ---"
catch {set_delay_calculator prima} msg
puts "set_delay_calculator prima: $msg"

report_checks

report_checks -path_delay min

report_checks -from [get_ports in1] -to [get_ports out] -fields {slew cap}

report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max
puts "prima dcalc u1: done"

report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max
puts "prima dcalc u2: done"

report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max
puts "prima dcalc r1: done"

#---------------------------------------------------------------
# Switch back to default and verify
#---------------------------------------------------------------
puts "--- back to dmp_ceff_elmore ---"
set_delay_calculator dmp_ceff_elmore

report_checks

#---------------------------------------------------------------
# Report net with various parameters
#---------------------------------------------------------------
puts "--- report_net final ---"
foreach net_name {r1q r2q u1z u2z in1 in2 out} {
  report_net -digits 4 $net_name
  puts "report_net $net_name: done"
}

#---------------------------------------------------------------
# Report parasitic annotation
#---------------------------------------------------------------
puts "--- parasitic annotation ---"
report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Annotated delay final
#---------------------------------------------------------------
puts "--- annotated delay final ---"
report_annotated_delay -cell
puts "annotated -cell: done"

report_annotated_delay -net
puts "annotated -net: done"

report_annotated_delay -cell -net
puts "annotated -cell -net: done"

report_annotated_delay -from_in_ports -to_out_ports
puts "annotated from/to ports: done"
