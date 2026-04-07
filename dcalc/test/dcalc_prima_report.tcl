# Prima report_dcalc with -name spef
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz
read_verilog ../../test/reg1_asap7.v
link_design top

read_spef -name spef -reduce ../../test/reg1_asap7.spef
create_clock -name clk -period 500 -waveform {0 250} {clk1 clk2 clk3}

set_delay_calculator prima
read_spef -name spef ../../test/reg1_asap7.spef
report_checks
report_dcalc -from u1/A -to u1/Y
