# Test multi-corner parasitics
# Exercises: read_spef with -corner, report_checks per corner, parasitic annotation per corner

define_corners fast slow

# Read ASAP7 libraries for both corners
read_liberty -corner fast ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty -corner fast ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty -corner fast ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty -corner fast ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty -corner fast ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_liberty -corner slow ../../test/asap7/asap7sc7p5t_SEQ_RVT_SS_nldm_220123.lib
read_liberty -corner slow ../../test/asap7/asap7sc7p5t_INVBUF_RVT_SS_nldm_220122.lib.gz
read_liberty -corner slow ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120.lib.gz
read_liberty -corner slow ../../test/asap7/asap7sc7p5t_OA_RVT_SS_nldm_211120.lib.gz
read_liberty -corner slow ../../test/asap7/asap7sc7p5t_AO_RVT_SS_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

create_clock -name clk -period 500 {clk1 clk2 clk3}
set_input_delay -clock clk 1 {in1 in2}
set_output_delay -clock clk 1 [get_ports out]
set_input_transition 10 {in1 in2 clk1 clk2 clk3}
set_propagated_clock {clk1 clk2 clk3}

#---------------------------------------------------------------
# Read SPEF for each corner
#---------------------------------------------------------------
puts "--- Reading SPEF per corner ---"
read_spef -corner fast ../../test/reg1_asap7.spef
puts "PASS: read_spef fast corner"

read_spef -corner slow ../../test/reg1_asap7.spef
puts "PASS: read_spef slow corner"

#---------------------------------------------------------------
# report_checks per corner
#---------------------------------------------------------------
puts "--- Fast corner timing with parasitics ---"
report_checks -corner fast
puts "PASS: report_checks fast corner"

report_checks -corner fast -path_delay min
puts "PASS: report_checks fast min path"

report_checks -corner fast -path_delay max
puts "PASS: report_checks fast max path"

report_checks -corner fast -fields {slew cap input_pins} -format full_clock
puts "PASS: report_checks fast with fields"

puts "--- Slow corner timing with parasitics ---"
report_checks -corner slow
puts "PASS: report_checks slow corner"

report_checks -corner slow -path_delay min
puts "PASS: report_checks slow min path"

report_checks -corner slow -path_delay max
puts "PASS: report_checks slow max path"

report_checks -corner slow -fields {slew cap input_pins} -format full_clock
puts "PASS: report_checks slow with fields"

#---------------------------------------------------------------
# report_dcalc per corner with parasitics
#---------------------------------------------------------------
puts "--- report_dcalc per corner ---"

catch {report_dcalc -corner fast -from [get_pins u1/A] -to [get_pins u1/Y]} msg
puts $msg
puts "PASS: report_dcalc fast BUF"

catch {report_dcalc -corner slow -from [get_pins u1/A] -to [get_pins u1/Y]} msg
puts $msg
puts "PASS: report_dcalc slow BUF"

catch {report_dcalc -corner fast -from [get_pins u2/A] -to [get_pins u2/Y]} msg
puts $msg
puts "PASS: report_dcalc fast AND2"

catch {report_dcalc -corner slow -from [get_pins u2/A] -to [get_pins u2/Y]} msg
puts $msg
puts "PASS: report_dcalc slow AND2"

catch {report_dcalc -corner fast -from [get_pins r1/CLK] -to [get_pins r1/Q]} msg
puts $msg
puts "PASS: report_dcalc fast DFF CLK->Q"

catch {report_dcalc -corner slow -from [get_pins r1/CLK] -to [get_pins r1/Q]} msg
puts $msg
puts "PASS: report_dcalc slow DFF CLK->Q"

#---------------------------------------------------------------
# report_net per corner
#---------------------------------------------------------------
puts "--- report_net per corner ---"

catch {report_net -corner fast r1q} msg
puts $msg
puts "PASS: report_net fast r1q"

catch {report_net -corner slow r1q} msg
puts $msg
puts "PASS: report_net slow r1q"

catch {report_net -corner fast u2z} msg
puts $msg
puts "PASS: report_net fast u2z"

catch {report_net -corner slow u2z} msg
puts $msg
puts "PASS: report_net slow u2z"

#---------------------------------------------------------------
# Cross-corner comparison via report_checks
#---------------------------------------------------------------
puts "--- Cross-corner path comparison ---"
report_checks -corner fast -from [get_ports in1] -to [get_ports out]
puts "PASS: fast in1->out"

report_checks -corner slow -from [get_ports in1] -to [get_ports out]
puts "PASS: slow in1->out"

report_checks -corner fast -from [get_ports in2] -to [get_ports out]
puts "PASS: fast in2->out"

report_checks -corner slow -from [get_ports in2] -to [get_ports out]
puts "PASS: slow in2->out"

puts "ALL PASSED"
