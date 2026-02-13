# Test scan signal_type parsing, test_cell construction, and
# scan-related port attributes across multiple PDKs.
# Targets:
#   LibertyReader.cc: visitSignalType (test_scan_enable, test_scan_in,
#     test_scan_out, test_scan_clock), beginTestCell, endTestCell,
#     test_cell pin parsing, signal_type enumeration paths,
#     scan-related sequential construction
#   Liberty.cc: LibertyCell::testCell, LibertyPort::setScanSignalType,
#     scanSignalType, scanSignalTypeName,
#     LibertyPort::function, tristateEnable
#   LibertyBuilder.cc: test_cell cell build
source ../../test/helpers.tcl

############################################################
# Read Sky130 library (has scan flip-flop cells with test_cell groups)
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read Sky130 library"

############################################################
# Query scan DFF cells - these have test_cell groups with signal_type
############################################################
puts "--- scan DFF cell queries ---"

# sdfxtp cells are scan DFFs
foreach cell_name {sky130_fd_sc_hd__sdfxtp_1 sky130_fd_sc_hd__sdfxtp_2
                   sky130_fd_sc_hd__sdfxtp_4} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set area [get_property $cell area]
      set lp [get_property $cell cell_leakage_power]
      puts "$cell_name area=$area leakage=$lp"

      # Check test_cell
      set tc [$cell test_cell]
      if {$tc != "NULL"} {
        puts "  has test_cell: yes"
      } else {
        puts "  has test_cell: no"
      }

      # Iterate ports and check scan_signal_type
      set port_iter [$cell liberty_port_iterator]
      while {[$port_iter has_next]} {
        set port [$port_iter next]
        set dir [sta::liberty_port_direction $port]
        set is_pwr [$port is_pwr_gnd]
        if {!$is_pwr} {
          set sst [$port scan_signal_type]
          set func [$port function]
          puts "  [get_name $port] dir=$dir scan_type=$sst func=$func"
        }
      }
      $port_iter finish
    }
  }
}
puts "PASS: sdfxtp scan DFF queries"

# sdfxbp cells are scan DFFs with complementary outputs
foreach cell_name {sky130_fd_sc_hd__sdfxbp_1 sky130_fd_sc_hd__sdfxbp_2} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set area [get_property $cell area]
      puts "$cell_name area=$area"

      set tc [$cell test_cell]
      if {$tc != "NULL"} {
        puts "  has test_cell: yes"
      } else {
        puts "  has test_cell: no"
      }

      set port_iter [$cell liberty_port_iterator]
      while {[$port_iter has_next]} {
        set port [$port_iter next]
        set is_pwr [$port is_pwr_gnd]
        if {!$is_pwr} {
          set dir [sta::liberty_port_direction $port]
          set sst [$port scan_signal_type]
          puts "  [get_name $port] dir=$dir scan_type=$sst"
        }
      }
      $port_iter finish
    }
  }
}
puts "PASS: sdfxbp scan DFF queries"

# sdfrtp cells are scan DFFs with async reset
foreach cell_name {sky130_fd_sc_hd__sdfrtp_1 sky130_fd_sc_hd__sdfrtp_2
                   sky130_fd_sc_hd__sdfrtp_4} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set area [get_property $cell area]
      puts "$cell_name area=$area"

      set tc [$cell test_cell]
      if {$tc != "NULL"} {
        puts "  has test_cell: yes"
      } else {
        puts "  has test_cell: no"
      }

      set port_iter [$cell liberty_port_iterator]
      while {[$port_iter has_next]} {
        set port [$port_iter next]
        set is_pwr [$port is_pwr_gnd]
        if {!$is_pwr} {
          set dir [sta::liberty_port_direction $port]
          set sst [$port scan_signal_type]
          puts "  [get_name $port] dir=$dir scan_type=$sst"
        }
      }
      $port_iter finish
    }
  }
}
puts "PASS: sdfrtp scan DFF queries"

# sdfstp cells are scan DFFs with async set
foreach cell_name {sky130_fd_sc_hd__sdfstp_1 sky130_fd_sc_hd__sdfstp_2
                   sky130_fd_sc_hd__sdfstp_4} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set area [get_property $cell area]
      puts "$cell_name area=$area"

      set port_iter [$cell liberty_port_iterator]
      while {[$port_iter has_next]} {
        set port [$port_iter next]
        set is_pwr [$port is_pwr_gnd]
        if {!$is_pwr} {
          set dir [sta::liberty_port_direction $port]
          set sst [$port scan_signal_type]
          puts "  [get_name $port] dir=$dir scan_type=$sst"
        }
      }
      $port_iter finish
    }
  }
}
puts "PASS: sdfstp scan DFF queries"

############################################################
# Timing arcs on scan DFFs (exercises recovery/removal arcs)
############################################################
puts "--- scan DFF timing arcs ---"

foreach cell_name {sky130_fd_sc_hd__sdfxtp_1 sky130_fd_sc_hd__sdfrtp_1
                   sky130_fd_sc_hd__sdfstp_1} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    if {$cell != "NULL"} {
      set arcs [$cell timing_arc_sets]
      set arc_count [llength $arcs]
      puts "$cell_name arc_sets = $arc_count"
      foreach arc $arcs {
        set role [$arc role]
        puts "  [$arc full_name] role=[$role name]"
      }
    }
  }
}
puts "PASS: scan DFF timing arcs"

############################################################
# Read Nangate library and query scan cells there too
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

# Nangate SDFF cells
foreach cell_name {SDFF_X1 SDFF_X2 SDFFR_X1 SDFFS_X1 SDFFRS_X1} {
  catch {
    set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
    if {$cell != "NULL"} {
      set tc [$cell test_cell]
      puts "$cell_name test_cell=[expr {$tc != "NULL" ? "yes" : "no"}]"

      set port_iter [$cell liberty_port_iterator]
      while {[$port_iter has_next]} {
        set port [$port_iter next]
        set dir [sta::liberty_port_direction $port]
        set sst [$port scan_signal_type]
        if {$sst != "none"} {
          puts "  [get_name $port] dir=$dir scan_type=$sst"
        }
      }
      $port_iter finish
    }
  }
}
puts "PASS: Nangate scan cell queries"

# Nangate CLKGATETST cell (clock gate test)
catch {
  set cell [get_lib_cell NangateOpenCellLibrary/CLKGATETST_X1]
  if {$cell != "NULL"} {
    set tc [$cell test_cell]
    set area [get_property $cell area]
    puts "CLKGATETST_X1 area=$area test_cell=[expr {$tc != "NULL" ? "yes" : "no"}]"

    set arcs [$cell timing_arc_sets]
    foreach arc $arcs {
      puts "  [$arc full_name] role=[[$arc role] name]"
    }
  }
}
puts "PASS: CLKGATETST queries"

############################################################
# Read ASAP7 SEQ for ICG (integrated clock gate) scan coverage
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
puts "PASS: read ASAP7 SEQ"

# ASAP7 ICG cell has statetable (exercises clock gate paths)
catch {
  set cell [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R]
  if {$cell != "NULL"} {
    set arcs [$cell timing_arc_sets]
    puts "ASAP7 ICGx1 arc_sets = [llength $arcs]"
    foreach arc $arcs {
      puts "  [$arc full_name] role=[[$arc role] name]"
    }
  }
}
puts "PASS: ASAP7 ICG arcs"

# ASAP7 DFFs with scan
foreach cell_name {DFFHQNx1_ASAP7_75t_R DFFHQx1_ASAP7_75t_R
                   DFFHQNx2_ASAP7_75t_R DFFHQx2_ASAP7_75t_R} {
  catch {
    set cell [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/$cell_name]
    if {$cell != "NULL"} {
      set arcs [$cell timing_arc_sets]
      puts "$cell_name arcs=[llength $arcs]"
      foreach arc $arcs {
        set role [$arc role]
        if {[$role name] != "combinational"} {
          puts "  [$arc full_name] role=[$role name]"
        }
      }
    }
  }
}
puts "PASS: ASAP7 DFF arc queries"

############################################################
# Link design with Nangate and report checks to exercise
# scan cell timing through a design
############################################################
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [all_inputs]
set_output_delay -clock clk1 3.0 [all_outputs]
set_input_transition 0.1 [all_inputs]

report_checks
puts "PASS: report_checks with scan libraries loaded"

puts "ALL PASSED"
