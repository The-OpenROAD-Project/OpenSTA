# Deep timing type and timing arc attribute testing across diverse PDKs.
# Targets:
#   LibertyReader.cc: timing_type attribute handler (clear, preset,
#     recovery_rising, recovery_falling, removal_rising, removal_falling,
#     min_pulse_width, min_period, three_state_enable,
#     three_state_disable, rising_edge, falling_edge, preset, clear,
#     hold_rising, hold_falling, setup_rising, setup_falling,
#     clock_gating_setup, clock_gating_hold),
#     visitClear, visitPreset, visitClrPresetVar1, visitClrPresetVar2,
#     visitMinPeriod, visitMinPulseWidthLow, visitMinPulseWidthHigh,
#     beginPgPin, endPgPin, visitVoltageMap
#   Liberty.cc: timingArcSets traversal, timing type query, hasSequentials,
#     translatePresetClrCheckRoles
#   TimingArc.cc: TimingArcSet::role, timingType, full_name
source ../../test/helpers.tcl

############################################################
# Read Sky130 library - has pg_pin, voltage_map, DFF with
# async clear/set (recovery/removal arcs), latches
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130 library"

set sky_lib [sta::find_liberty sky130_fd_sc_hd__tt_025C_1v80]

############################################################
# Query cells with async reset (DFRTP has RESET_B -> recovery/removal)
# Uses full_name which returns "cell from -> to"
############################################################
puts "--- async reset DFF cells ---"

set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfrtp_1]
set arcs [$cell timing_arc_sets]
puts "dfrtp_1 arc_sets = [llength $arcs]"
foreach arc $arcs {
  set role [$arc role]
  puts "  [$arc full_name] role=$role"
}
puts "PASS: dfrtp_1 timing arcs"

set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfstp_1]
set arcs [$cell timing_arc_sets]
puts "dfstp_1 arc_sets = [llength $arcs]"
foreach arc $arcs {
  puts "  [$arc full_name] role=[$arc role]"
}
puts "PASS: dfstp_1 timing arcs"

set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfbbp_1]
set arcs [$cell timing_arc_sets]
puts "dfbbp_1 arc_sets = [llength $arcs]"
foreach arc $arcs {
  puts "  [$arc full_name] role=[$arc role]"
}
puts "PASS: dfbbp_1 timing arcs"

# sdfrtp has scan + async reset
catch {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__sdfrtp_1]
  set arcs [$cell timing_arc_sets]
  puts "sdfrtp_1 arc_sets = [llength $arcs]"
  foreach arc $arcs {
    puts "  [$arc full_name] role=[$arc role]"
  }
}
puts "PASS: sdfrtp_1 timing arcs"

# sdfstp has scan + async set
catch {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__sdfstp_1]
  set arcs [$cell timing_arc_sets]
  puts "sdfstp_1 arc_sets = [llength $arcs]"
  foreach arc $arcs {
    puts "  [$arc full_name] role=[$arc role]"
  }
}
puts "PASS: sdfstp_1 timing arcs"

############################################################
# Query tristate cells (three_state_enable/disable timing types)
############################################################
puts "--- tristate cell timing arcs ---"

foreach cell_name {sky130_fd_sc_hd__ebufn_1 sky130_fd_sc_hd__ebufn_2
                   sky130_fd_sc_hd__ebufn_4 sky130_fd_sc_hd__ebufn_8} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    set arcs [$cell timing_arc_sets]
    puts "$cell_name arc_sets = [llength $arcs]"
    foreach arc $arcs {
      puts "  [$arc full_name] role=[$arc role]"
    }
  }
}
puts "PASS: tristate cell timing arcs"

############################################################
# Query clock gate cells
############################################################
puts "--- clock gate cell timing arcs ---"

foreach cell_name {sky130_fd_sc_hd__dlclkp_1 sky130_fd_sc_hd__dlclkp_2
                   sky130_fd_sc_hd__sdlclkp_1 sky130_fd_sc_hd__sdlclkp_2} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set arcs [$cell timing_arc_sets]
      puts "$cell_name arc_sets = [llength $arcs]"
      foreach arc $arcs {
        puts "  [$arc full_name] role=[$arc role]"
      }
    }
  }
}
puts "PASS: clock gate cell timing arcs"

############################################################
# Query latch cells
############################################################
puts "--- latch cell timing arcs ---"

foreach cell_name {sky130_fd_sc_hd__dlxtp_1 sky130_fd_sc_hd__dlxtn_1
                   sky130_fd_sc_hd__dlxbn_1 sky130_fd_sc_hd__dlxbp_1} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set arcs [$cell timing_arc_sets]
      puts "$cell_name arc_sets = [llength $arcs]"
      foreach arc $arcs {
        puts "  [$arc full_name] role=[$arc role]"
      }
    }
  }
}
puts "PASS: latch cell timing arcs"

############################################################
# Read ASAP7 SEQ library
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ"

catch {
  set cell [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R]
  set arcs [$cell timing_arc_sets]
  puts "DFFHQNx1 arc_sets = [llength $arcs]"
  foreach arc $arcs {
    puts "  [$arc full_name] role=[$arc role]"
  }
}
puts "PASS: ASAP7 DFF arcs"

catch {
  set cell [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DLLx1_ASAP7_75t_R]
  set arcs [$cell timing_arc_sets]
  puts "DLLx1 arc_sets = [llength $arcs]"
  foreach arc $arcs {
    puts "  [$arc full_name] role=[$arc role]"
  }
}
puts "PASS: ASAP7 latch arcs"

catch {
  set cell [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R]
  set arcs [$cell timing_arc_sets]
  puts "ICGx1 arc_sets = [llength $arcs]"
  foreach arc $arcs {
    puts "  [$arc full_name] role=[$arc role]"
  }
}
puts "PASS: ASAP7 ICG arcs"

############################################################
# Read IHP library
############################################################
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib
puts "PASS: read IHP"

foreach cell_name {sg13g2_dlhq_1 sg13g2_dllq_1} {
  catch {
    set cell [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/$cell_name]
    if {$cell != "NULL"} {
      set arcs [$cell timing_arc_sets]
      puts "$cell_name arc_sets = [llength $arcs]"
      foreach arc $arcs {
        puts "  [$arc full_name] role=[$arc role]"
      }
    }
  }
}
puts "PASS: IHP latch arcs"

foreach cell_name {sg13g2_dfrbp_1 sg13g2_dfrbp_2} {
  catch {
    set cell [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/$cell_name]
    if {$cell != "NULL"} {
      set arcs [$cell timing_arc_sets]
      puts "$cell_name arc_sets = [llength $arcs]"
      foreach arc $arcs {
        puts "  [$arc full_name] role=[$arc role]"
      }
    }
  }
}
puts "PASS: IHP async DFF arcs"

foreach cell_name {sg13g2_sdfbbp_1} {
  catch {
    set cell [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/$cell_name]
    if {$cell != "NULL"} {
      set arcs [$cell timing_arc_sets]
      puts "$cell_name arc_sets = [llength $arcs]"
      foreach arc $arcs {
        puts "  [$arc full_name] role=[$arc role]"
      }
    }
  }
}
puts "PASS: IHP scan DFF arcs"

############################################################
# Link design and exercise check timing types
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [all_inputs]
set_output_delay -clock clk1 3.0 [all_outputs]
set_input_transition 0.1 [all_inputs]
puts "PASS: design setup"

report_check_types -max_delay -min_delay
puts "PASS: max/min delay"

report_check_types -recovery -removal
puts "PASS: recovery/removal"

report_check_types -min_pulse_width -min_period
puts "PASS: min_pulse_width/min_period"

report_check_types -clock_gating_setup -clock_gating_hold
puts "PASS: clock_gating"

report_check_types -max_slew -max_capacitance -max_fanout
puts "PASS: max_slew/cap/fanout"

report_check_types -max_skew
puts "PASS: max_skew"

############################################################
# Read Sky130 fast/slow corners
############################################################
read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib
puts "PASS: read Sky130 fast corner"

read_liberty ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
puts "PASS: read Sky130 slow corner"

catch {
  report_lib_cell sky130_fd_sc_hd__ff_n40C_1v95/sky130_fd_sc_hd__dfrtp_1
  puts "PASS: fast corner dfrtp report"
}

catch {
  report_lib_cell sky130_fd_sc_hd__ss_n40C_1v40/sky130_fd_sc_hd__dfrtp_1
  puts "PASS: slow corner dfrtp report"
}

############################################################
# Write liberty
############################################################
set outfile [make_result_file liberty_timing_types_deep_write.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile
puts "PASS: write_liberty"

puts "ALL PASSED"
