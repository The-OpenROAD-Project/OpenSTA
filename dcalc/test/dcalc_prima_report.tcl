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

# read_spef -name <spef> only parks the parasitics under <spef> in the
# parasitics_name_map_; it does NOT bind them to any Scene (this is the
# deferred-binding flow for MCMM -- see examples/mcmm3.tcl and Sta::readSpef
# in search/Sta.cc). Without an explicit Scene binding, report_dcalc /
# report_checks would see only pin capacitance (~0.57 fF here) and produce
# incorrect table lookups, instead of the wire-cap-annotated ~13.97 fF we
# expect for this design. define_scene with -spef spef creates a Scene
# bound to the -name'd Parasitics via Scene::setParasitics and makes it the
# current Scene, so Prima receives the full RC network.
define_scene scene1 \
  -liberty {asap7sc7p5t_SEQ_RVT_FF_nldm_220123 \
            asap7sc7p5t_INVBUF_RVT_FF_nldm_211120 \
            asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120 \
            asap7sc7p5t_OA_RVT_FF_nldm_211120 \
            asap7sc7p5t_AO_RVT_FF_nldm_211120} \
  -spef spef

report_checks
report_dcalc -from u1/A -to u1/Y
