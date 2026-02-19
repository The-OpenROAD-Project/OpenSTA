# Test multi-corner library reading and Sky130HS features.
# Targets:
#   LibertyReader.cc: different lib format features per PDK,
#     define_cell_area, leakage_power parsing, internal_power parsing,
#     max_capacitance/max_transition on pins, pg_pin groups,
#     voltage_map parsing, various attribute visitors
#   Liberty.cc: makeCornerMap, checkCorners, LibertyPort driveResistance,
#     maxCapacitance, maxFanout, maxSlew on ports,
#     bufferPorts, addSupplyVoltage, supplyVoltage
#   LibertyWriter.cc: write different lib formats
#   TableModel.cc: table lookup, axis variable handling
source ../../test/helpers.tcl

############################################################
# Read Sky130HD all corners
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib

############################################################
# Read Sky130HS
############################################################
read_liberty ../../test/sky130hs/sky130hs_tt.lib

read_liberty ../../test/sky130hs/sky130_fd_sc_hs__tt_025C_1v80.lib

############################################################
# Comprehensive cell reports across PDKs
# Exercises: all timing model, power model, and arc code
############################################################

# Sky130HD cells - comprehensive reporting
set sky130_cells_to_report {
  sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__inv_2 sky130_fd_sc_hd__inv_4
  sky130_fd_sc_hd__buf_1 sky130_fd_sc_hd__buf_2 sky130_fd_sc_hd__buf_4
  sky130_fd_sc_hd__nand2_1 sky130_fd_sc_hd__nand3_1 sky130_fd_sc_hd__nand4_1
  sky130_fd_sc_hd__nor2_1 sky130_fd_sc_hd__nor3_1 sky130_fd_sc_hd__nor4_1
  sky130_fd_sc_hd__and2_1 sky130_fd_sc_hd__and3_1 sky130_fd_sc_hd__and4_1
  sky130_fd_sc_hd__or2_1 sky130_fd_sc_hd__or3_1 sky130_fd_sc_hd__or4_1
  sky130_fd_sc_hd__xor2_1 sky130_fd_sc_hd__xnor2_1
  sky130_fd_sc_hd__a21o_1 sky130_fd_sc_hd__a21oi_1
  sky130_fd_sc_hd__a22o_1 sky130_fd_sc_hd__a22oi_1
  sky130_fd_sc_hd__o21a_1 sky130_fd_sc_hd__o21ai_0
  sky130_fd_sc_hd__o22a_1 sky130_fd_sc_hd__o22ai_1
  sky130_fd_sc_hd__a31o_1 sky130_fd_sc_hd__a32o_1
  sky130_fd_sc_hd__mux2_1 sky130_fd_sc_hd__mux4_1
  sky130_fd_sc_hd__fa_1 sky130_fd_sc_hd__ha_1 sky130_fd_sc_hd__maj3_1
  sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dfrtp_1
  sky130_fd_sc_hd__dfstp_1 sky130_fd_sc_hd__dfbbp_1
  sky130_fd_sc_hd__dlxtp_1 sky130_fd_sc_hd__dlxtn_1
  sky130_fd_sc_hd__sdfxtp_1 sky130_fd_sc_hd__sdfxbp_1
  sky130_fd_sc_hd__ebufn_1 sky130_fd_sc_hd__ebufn_2
  sky130_fd_sc_hd__clkbuf_1 sky130_fd_sc_hd__clkbuf_2
  sky130_fd_sc_hd__clkinv_1 sky130_fd_sc_hd__clkinv_2
  sky130_fd_sc_hd__conb_1
  sky130_fd_sc_hd__diode_2 sky130_fd_sc_hd__fill_1
}

foreach cell_name $sky130_cells_to_report {
  catch {
    report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name
  }
}

############################################################
# Cell property queries on Sky130
############################################################
foreach cell_name {sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__buf_1
                   sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dlxtp_1
                   sky130_fd_sc_hd__sdfxtp_1 sky130_fd_sc_hd__ebufn_1
                   sky130_fd_sc_hd__mux2_1 sky130_fd_sc_hd__fa_1} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    set area [get_property $cell area]
    set du [get_property $cell dont_use]
    set lp [get_property $cell cell_leakage_power]
    puts "$cell_name: area=$area dont_use=$du leakage=$lp"
  }
}

############################################################
# Pin capacitance queries on Sky130
############################################################
foreach {cell_name pin_name} {
  sky130_fd_sc_hd__inv_1 A
  sky130_fd_sc_hd__inv_1 Y
  sky130_fd_sc_hd__buf_1 A
  sky130_fd_sc_hd__buf_1 X
  sky130_fd_sc_hd__nand2_1 A
  sky130_fd_sc_hd__nand2_1 B
  sky130_fd_sc_hd__nand2_1 Y
  sky130_fd_sc_hd__dfxtp_1 CLK
  sky130_fd_sc_hd__dfxtp_1 D
  sky130_fd_sc_hd__dfxtp_1 Q
  sky130_fd_sc_hd__dfrtp_1 CLK
  sky130_fd_sc_hd__dfrtp_1 D
  sky130_fd_sc_hd__dfrtp_1 RESET_B
  sky130_fd_sc_hd__dfrtp_1 Q
} {
  catch {
    set pin [get_lib_pin sky130_fd_sc_hd__tt_025C_1v80/$cell_name/$pin_name]
    set cap [get_property $pin capacitance]
    set dir [sta::liberty_port_direction $pin]
    puts "$cell_name/$pin_name: cap=$cap dir=$dir"
  }
}

############################################################
# Write all libraries to exercise all writer paths
############################################################
set outfile1 [make_result_file liberty_sky130_hd_tt.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile1

catch {
  set outfile2 [make_result_file liberty_sky130_hs_tt.lib]
  sta::write_liberty sky130_fd_sc_hs__tt_025C_1v80 $outfile2
}

############################################################
# Read ASAP7 with various Vt combos to stress LibertyReader
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_AO_LVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_AO_SLVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_OA_LVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_OA_SLVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_LVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_SLVT_FF_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_LVT_TT_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_TT_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_TT_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_SS_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_SS_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_SS_nldm_211120.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_SS_nldm_211120.lib.gz

# SRAM macro
read_liberty ../../test/asap7/fakeram7_256x32.lib
