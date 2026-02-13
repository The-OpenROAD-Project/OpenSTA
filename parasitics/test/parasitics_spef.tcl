# Test SPEF read and parasitics reporting
# reg1_asap7 uses ASAP7 cells
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

create_clock -name clk1 -period 10 [get_ports clk1]

# Read SPEF
read_spef ../../test/reg1_asap7.spef
puts "PASS: read_spef completed"

report_checks
puts "PASS: report_checks with parasitics"

puts "ALL PASSED"
