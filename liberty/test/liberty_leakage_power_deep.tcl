# Test deep leakage power groups with when conditions, internal power
# with related_pg_pin, and power attribute parsing across PDKs.
# Targets:
#   LibertyReader.cc: beginLeakagePower, endLeakagePower, makeLeakagePowers,
#     visitLeakagePowerValue, visitLeakagePowerWhen, visitRelatedPgPin,
#     beginInternalPower, endInternalPower, visitCellLeakagePower,
#     visitPowerUnit, beginPgPin, endPgPin, visitPgType, visitVoltageName,
#     visitWhen (power context), visitRelatedBiasPin
#   LeakagePower.cc: LeakagePower construction, LeakagePowerGroup,
#     setRelatedPgPin, when condition parsing
#   InternalPower.cc: InternalPowerAttrs, setRelatedPgPin,
#     InternalPower construction
#   Liberty.cc: addLeakagePower, leakagePower, addInternalPower,
#     internalPowers, hasInternalPower
source ../../test/helpers.tcl

############################################################
# Read Sky130 library (has leakage_power groups with when conditions)
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

############################################################
# Query leakage power on various cell types
# Sky130 has per-state leakage_power groups with when conditions
############################################################
puts "--- leakage power queries ---"

# Combinational cells
foreach cell_name {sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__inv_2
                   sky130_fd_sc_hd__buf_1 sky130_fd_sc_hd__buf_2
                   sky130_fd_sc_hd__nand2_1 sky130_fd_sc_hd__nand3_1
                   sky130_fd_sc_hd__nor2_1 sky130_fd_sc_hd__nor3_1
                   sky130_fd_sc_hd__and2_1 sky130_fd_sc_hd__or2_1
                   sky130_fd_sc_hd__xor2_1 sky130_fd_sc_hd__xnor2_1
                   sky130_fd_sc_hd__a21o_1 sky130_fd_sc_hd__a21oi_1
                   sky130_fd_sc_hd__o21a_1 sky130_fd_sc_hd__o21ai_0
                   sky130_fd_sc_hd__mux2_1 sky130_fd_sc_hd__mux2i_1} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set lp [get_property $cell cell_leakage_power]
      puts "$cell_name leakage=$lp"
    }
  }
}

# Sequential cells (these have more leakage states)
foreach cell_name {sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dfxtp_2
                   sky130_fd_sc_hd__dfrtp_1 sky130_fd_sc_hd__dfstp_1
                   sky130_fd_sc_hd__dlxtp_1 sky130_fd_sc_hd__dlxtn_1
                   sky130_fd_sc_hd__sdfxtp_1 sky130_fd_sc_hd__sdfrtp_1
                   sky130_fd_sc_hd__sdfstp_1 sky130_fd_sc_hd__dfbbp_1} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set lp [get_property $cell cell_leakage_power]
      set area [get_property $cell area]
      puts "$cell_name leakage=$lp area=$area"
    }
  }
}

# Tristate cells
foreach cell_name {sky130_fd_sc_hd__ebufn_1 sky130_fd_sc_hd__ebufn_2
                   sky130_fd_sc_hd__ebufn_4 sky130_fd_sc_hd__ebufn_8} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set lp [get_property $cell cell_leakage_power]
      puts "$cell_name leakage=$lp"
    }
  }
}

# Clock gate cells
foreach cell_name {sky130_fd_sc_hd__dlclkp_1 sky130_fd_sc_hd__dlclkp_2
                   sky130_fd_sc_hd__sdlclkp_1} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set lp [get_property $cell cell_leakage_power]
      puts "$cell_name leakage=$lp"
    }
  }
}

############################################################
# Report lib cells to exercise detailed leakage/power info
############################################################
puts "--- detailed cell reports ---"

catch {report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1}

catch {report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__nand2_1}

catch {report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfxtp_1}

############################################################
# Read Nangate library for internal power with when conditions
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib

# Query Nangate cell leakage
foreach cell_name {INV_X1 INV_X2 INV_X4 BUF_X1 BUF_X2 BUF_X4
                   NAND2_X1 NOR2_X1 AND2_X1 OR2_X1 XOR2_X1
                   AOI21_X1 OAI21_X1 MUX2_X1 HA_X1 FA_X1
                   DFF_X1 DFF_X2 DFFR_X1 DFFS_X1 DFFRS_X1
                   SDFF_X1 SDFFR_X1 SDFFRS_X1
                   TINV_X1 TLAT_X1 CLKGATETST_X1} {
  catch {
    set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
    if {$cell != "NULL"} {
      set lp [get_property $cell cell_leakage_power]
      puts "$cell_name leakage=$lp"
    }
  }
}

############################################################
# Link design and run power analysis to exercise internal power
############################################################
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [all_inputs]
set_output_delay -clock clk1 3.0 [all_outputs]
set_input_transition 0.1 [all_inputs]

# Power reports exercise internal power evaluation
report_power

report_power -digits 8

# Per-instance power
foreach inst_name {buf1 inv1 and1 or1 nand1 nor1 reg1 reg2 reg3} {
  catch {
    report_power -instances [get_cells $inst_name]
  }
}

############################################################
# Read IHP library for different power model format
############################################################
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

foreach cell_name {sg13g2_inv_1 sg13g2_buf_1 sg13g2_nand2_1
                   sg13g2_nor2_1 sg13g2_and2_1 sg13g2_or2_1} {
  catch {
    set cell [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/$cell_name]
    if {$cell != "NULL"} {
      set lp [get_property $cell cell_leakage_power]
      set area [get_property $cell area]
      puts "IHP $cell_name leakage=$lp area=$area"
    }
  }
}

############################################################
# Write liberty roundtrip for Sky130 (exercises power writer)
############################################################
set outfile [make_result_file liberty_leakage_power_deep_write.lib]
catch {
  sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile
}
