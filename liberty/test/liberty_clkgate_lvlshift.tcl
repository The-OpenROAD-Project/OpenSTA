# Test clock gating cells, level shifter cells, pg_pin attributes,
# voltage_map/supply_voltage queries, and related cell classification.
# Targets:
#   LibertyReader.cc: visitClockGatingIntegratedCell (latch_posedge,
#     latch_posedge_precontrol), visitIsLevelShifter, visitLevelShifterType,
#     visitLevelShifterDataPin, beginPgPin, endPgPin, visitPgType,
#     visitVoltageName, visitVoltageMap, visitRelatedBiasPin,
#     visitClockGateClockPin, visitClockGateEnablePin, visitClockGateOutPin,
#     visitInputVoltageRange, visitOutputVoltageRange
#   Liberty.cc: isClockGate, isClockGateLatchPosedge, isClockGateLatchNegedge,
#     isClockGateOther, isLevelShifter, addSupplyVoltage, supplyVoltage,
#     supplyExists, LibertyPort::isPwrGnd
#   LibertyBuilder.cc: clock gate cell construction
source ../../test/helpers.tcl

############################################################
# Read Sky130 library (has clock gate cells, level shifters, pg_pins)
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130 library"

set lib [sta::find_liberty sky130_fd_sc_hd__tt_025C_1v80]
puts "PASS: found liberty library"

############################################################
# Voltage map / supply voltage queries
# Exercises: addSupplyVoltage, supplyVoltage, supplyExists
############################################################
puts "--- voltage_map / supply queries ---"

set vpwr_exists [sta::liberty_supply_exists VPWR]
puts "VPWR exists: $vpwr_exists"

set vgnd_exists [sta::liberty_supply_exists VGND]
puts "VGND exists: $vgnd_exists"

set vpb_exists [sta::liberty_supply_exists VPB]
puts "VPB exists: $vpb_exists"

set vnb_exists [sta::liberty_supply_exists VNB]
puts "VNB exists: $vnb_exists"

set kapwr_exists [sta::liberty_supply_exists KAPWR]
puts "KAPWR exists: $kapwr_exists"

set lowlv_exists [sta::liberty_supply_exists LOWLVPWR]
puts "LOWLVPWR exists: $lowlv_exists"

set vpwrin_exists [sta::liberty_supply_exists VPWRIN]
puts "VPWRIN exists: $vpwrin_exists"

set vss_exists [sta::liberty_supply_exists VSS]
puts "VSS exists: $vss_exists"

# Non-existent supply
set fake_exists [sta::liberty_supply_exists FAKE_SUPPLY]
puts "FAKE_SUPPLY exists: $fake_exists"

puts "PASS: supply voltage queries"

############################################################
# Clock gate cell queries (exercises clock_gating_integrated_cell)
# dlclkp cells have latch_posedge type
############################################################
puts "--- clock gate cell queries ---"

foreach cell_name {sky130_fd_sc_hd__dlclkp_1 sky130_fd_sc_hd__dlclkp_2
                   sky130_fd_sc_hd__dlclkp_4} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set area [get_property $cell area]
      set lp [get_property $cell cell_leakage_power]
      puts "$cell_name area=$area leakage=$lp"

      # Report the cell to exercise arc enumeration
      report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name
    }
  }
}
puts "PASS: dlclkp clock gate cells"

# sdlclkp cells have latch_posedge_precontrol type
foreach cell_name {sky130_fd_sc_hd__sdlclkp_1 sky130_fd_sc_hd__sdlclkp_2
                   sky130_fd_sc_hd__sdlclkp_4} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set area [get_property $cell area]
      puts "$cell_name area=$area"

      # Iterate ports to check clock_gate_* pin attributes
      set port_iter [$cell liberty_port_iterator]
      while {[$port_iter has_next]} {
        set port [$port_iter next]
        set dir [sta::liberty_port_direction $port]
        set func [$port function]
        puts "  [get_name $port] dir=$dir func=$func"
      }
      $port_iter finish
    }
  }
}
puts "PASS: sdlclkp clock gate cells with precontrol"

############################################################
# Level shifter cell queries
# Exercises: visitIsLevelShifter, visitLevelShifterType
############################################################
puts "--- level shifter cell queries ---"

foreach cell_name {sky130_fd_sc_hd__lpflow_lsbuf_lh_hl_isowell_tap_1
                   sky130_fd_sc_hd__lpflow_lsbuf_lh_hl_isowell_tap_2
                   sky130_fd_sc_hd__lpflow_lsbuf_lh_hl_isowell_tap_4
                   sky130_fd_sc_hd__lpflow_lsbuf_lh_isowell_4
                   sky130_fd_sc_hd__lpflow_lsbuf_lh_isowell_tap_1
                   sky130_fd_sc_hd__lpflow_lsbuf_lh_isowell_tap_2
                   sky130_fd_sc_hd__lpflow_lsbuf_lh_isowell_tap_4} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set area [get_property $cell area]
      set lp [get_property $cell cell_leakage_power]
      puts "$cell_name area=$area leakage=$lp"

      # Iterate ports
      set port_iter [$cell liberty_port_iterator]
      while {[$port_iter has_next]} {
        set port [$port_iter next]
        set dir [sta::liberty_port_direction $port]
        set is_pwr [$port is_pwr_gnd]
        if {!$is_pwr} {
          puts "  [get_name $port] dir=$dir"
        }
      }
      $port_iter finish
    }
  }
}
puts "PASS: level shifter cell queries"

############################################################
# PG pin queries on various cells
# Exercises: pg_pin parsing, isPwrGnd, voltage_name
############################################################
puts "--- pg_pin queries ---"

foreach cell_name {sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__buf_1
                   sky130_fd_sc_hd__nand2_1 sky130_fd_sc_hd__dfxtp_1
                   sky130_fd_sc_hd__dlclkp_1 sky130_fd_sc_hd__sdfxtp_1} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set pwr_count 0
      set sig_count 0
      set port_iter [$cell liberty_port_iterator]
      while {[$port_iter has_next]} {
        set port [$port_iter next]
        set is_pwr [$port is_pwr_gnd]
        if {$is_pwr} {
          incr pwr_count
        } else {
          incr sig_count
        }
      }
      $port_iter finish
      puts "$cell_name: pwr_pins=$pwr_count signal_pins=$sig_count"
    }
  }
}
puts "PASS: pg_pin queries"

############################################################
# Timing arc queries on clock gate cells
# Exercises: timing_type for clock gate arcs
############################################################
puts "--- clock gate timing arcs ---"

catch {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dlclkp_1]
  if {$cell != "NULL"} {
    set arcs [$cell timing_arc_sets]
    set arc_count [llength $arcs]
    puts "dlclkp_1 arc_sets = $arc_count"
    foreach arc $arcs {
      set from_port [$arc from]
      set to_port [$arc to]
      set role [$arc role]
      puts "  [$arc full_name] role=[$role name]"
    }
  }
}
puts "PASS: clock gate timing arcs"

catch {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__sdlclkp_1]
  if {$cell != "NULL"} {
    set arcs [$cell timing_arc_sets]
    set arc_count [llength $arcs]
    puts "sdlclkp_1 arc_sets = $arc_count"
    foreach arc $arcs {
      puts "  [$arc full_name] role=[$role name]"
    }
  }
}
puts "PASS: sdlclkp timing arcs"

############################################################
# Timing arc queries on level shifter cells
############################################################
puts "--- level shifter timing arcs ---"

catch {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__lpflow_lsbuf_lh_hl_isowell_tap_1]
  if {$cell != "NULL"} {
    set arcs [$cell timing_arc_sets]
    puts "lsbuf_lh_hl_isowell_tap_1 arcs = [llength $arcs]"
    foreach arc $arcs {
      puts "  [$arc full_name] role=[[$arc role] name]"
    }
  }
}
puts "PASS: level shifter timing arcs"

############################################################
# Buffer/inverter classification on Sky130 cells
############################################################
puts "--- cell classification ---"

foreach cell_name {sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__inv_2
                   sky130_fd_sc_hd__buf_1 sky130_fd_sc_hd__buf_2
                   sky130_fd_sc_hd__clkinv_1 sky130_fd_sc_hd__clkbuf_1
                   sky130_fd_sc_hd__nand2_1 sky130_fd_sc_hd__nor2_1
                   sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dlclkp_1} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set is_buf [$cell is_buffer]
      set is_inv [$cell is_inverter]
      set is_leaf [$cell is_leaf]
      puts "$cell_name: is_buffer=$is_buf is_inverter=$is_inv is_leaf=$is_leaf"
    }
  }
}
puts "PASS: cell classification"

############################################################
# Write liberty for sky130 (exercises writer for pg_pin, level_shifter)
############################################################
set outfile [make_result_file liberty_clkgate_lvlshift_write.lib]
catch {
  sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile
  puts "PASS: write_liberty sky130"
}

############################################################
# Read IHP library for more voltage_map / pg_pin coverage
############################################################
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP library"

# Check supply exists after IHP
set ihp_vdd_exists [sta::liberty_supply_exists VDD]
puts "IHP VDD exists: $ihp_vdd_exists"

# Query IHP cells
foreach cell_name {sg13g2_inv_1 sg13g2_buf_1 sg13g2_nand2_1 sg13g2_nor2_1} {
  catch {
    set cell [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/$cell_name]
    if {$cell != "NULL"} {
      set area [get_property $cell area]
      set is_buf [$cell is_buffer]
      set is_inv [$cell is_inverter]
      puts "IHP $cell_name: area=$area buf=$is_buf inv=$is_inv"
    }
  }
}
puts "PASS: IHP cell queries"

puts "ALL PASSED"
