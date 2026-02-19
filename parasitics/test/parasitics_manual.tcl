# Test manual parasitic model setting and parasitic reduction

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../test/reg1_asap7.v

# Use nangate45 design instead
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
# Report before any parasitics
#---------------------------------------------------------------
puts "--- before parasitics ---"
report_checks

report_parasitic_annotation

#---------------------------------------------------------------
# Set manual pi model on a driver pin
# set_pi_model drvr_pin c2 rpi c1
#---------------------------------------------------------------
puts "--- set_pi_model ---"

# Set pi model on u1/Y (driver of net u1z)
catch {sta::set_pi_model u1/Y 0.002 5.0 0.001} msg
puts "set_pi_model u1/Y: $msg"

# Set pi model on u2/Y (driver of net u2z)
catch {sta::set_pi_model u2/Y 0.003 8.0 0.002} msg
puts "set_pi_model u2/Y: $msg"

# Set pi model on r1/Q (driver of r1q)
catch {sta::set_pi_model r1/Q 0.001 3.0 0.001} msg
puts "set_pi_model r1/Q: $msg"

# Set pi model on r2/Q (driver of r2q)
catch {sta::set_pi_model r2/Q 0.001 3.0 0.001} msg
puts "set_pi_model r2/Q: $msg"

#---------------------------------------------------------------
# Set elmore delays on load pins
# set_elmore drvr_pin load_pin elmore
#---------------------------------------------------------------
puts "--- set_elmore ---"

# Elmore delays from u1/Y to its loads
catch {sta::set_elmore u1/Y u2/A 0.002} msg
puts "set_elmore u1/Y -> u2/A: $msg"

catch {sta::set_elmore u1/Y u2/B 0.003} msg
puts "set_elmore u1/Y -> u2/B: $msg"

# Elmore delays from u2/Y to its loads
catch {sta::set_elmore u2/Y r3/D 0.004} msg
puts "set_elmore u2/Y -> r3/D: $msg"

# Elmore delays from r1/Q to loads
catch {sta::set_elmore r1/Q u1/A 0.001} msg
puts "set_elmore r1/Q -> u1/A: $msg"

# Elmore delays from r2/Q to loads
catch {sta::set_elmore r2/Q u2/B 0.001} msg
puts "set_elmore r2/Q -> u2/B: $msg"

#---------------------------------------------------------------
# Report checks with manual parasitics
#---------------------------------------------------------------
puts "--- report_checks with manual parasitics ---"
report_checks

report_checks -path_delay min

report_checks -path_delay max

report_checks -from [get_ports in1] -to [get_ports out]

report_checks -fields {slew cap input_pins}

#---------------------------------------------------------------
# Report net with manual parasitics
#---------------------------------------------------------------
puts "--- report_net with manual parasitics ---"
catch {report_net r1q} msg
puts "report_net r1q: $msg"

catch {report_net u1z} msg
puts "report_net u1z: $msg"

catch {report_net u2z} msg
puts "report_net u2z: $msg"

#---------------------------------------------------------------
# Report parasitic annotation after manual setting
#---------------------------------------------------------------
puts "--- report_parasitic_annotation after manual ---"
report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# report_dcalc with manual parasitics
#---------------------------------------------------------------
puts "--- report_dcalc with manual parasitics ---"

catch {report_dcalc -from [get_pins u1/A] -to [get_pins u1/Y] -max} msg
puts "dcalc u1 A->Y: $msg"

catch {report_dcalc -from [get_pins u2/A] -to [get_pins u2/Y] -max} msg
puts "dcalc u2 A->Y: $msg"

catch {report_dcalc -from [get_pins r1/CLK] -to [get_pins r1/Q] -max} msg
puts "dcalc r1 CLK->Q: $msg"

#---------------------------------------------------------------
# Now read SPEF on top of manual parasitics to test override
#---------------------------------------------------------------
puts "--- read_spef to override manual parasitics ---"
read_spef ../../test/reg1_asap7.spef

report_checks

report_parasitic_annotation

report_parasitic_annotation -report_unannotated

#---------------------------------------------------------------
# Test report_net after SPEF
#---------------------------------------------------------------
puts "--- report_net after SPEF ---"
foreach net_name {r1q r2q u1z u2z} {
  report_net $net_name
  puts "report_net $net_name: done"
}

# report_net with digits
report_net -digits 3 r1q
puts "report_net -digits 3 r1q: done"

report_net -digits 6 u1z
puts "report_net -digits 6 u1z: done"

report_net -digits 8 u2z
puts "report_net -digits 8 u2z: done"
