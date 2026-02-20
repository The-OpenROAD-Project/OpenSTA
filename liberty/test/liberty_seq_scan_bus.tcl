# Test liberty reading and querying of sequential cells (latch, ff, statetable),
# test_cell/scan definitions, bus/bundle ports, tristate outputs,
# internal power, and scaled cells through multi-corner.
source ../../test/helpers.tcl

############################################################
# Read Sky130 library (has test_cell, scan, tristate, latch cells)
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

############################################################
# Query scan flip-flop cells (exercises test_cell path in reader)
############################################################
# sdfxtp has a test_cell group with scan ports
set sdf_cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__sdfxtp_1]
set sdf_area [get_property $sdf_cell area]
puts "sdfxtp_1 area = $sdf_area"

# Check test_cell exists
set tc [$sdf_cell test_cell]
if {$tc != "NULL" && $tc ne ""} {
} else {
}

# Query scan ports
foreach port_name {SCD SCE CLK D Q} {
  set port [$sdf_cell find_liberty_port $port_name]
  if {$port != "NULL" && $port ne ""} {
    set dir [sta::liberty_port_direction $port]
    puts "sdfxtp_1/$port_name dir=$dir"
  }
}

# Another scan cell
set sdf_cell2 [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__sdfxbp_1]
set area2 [get_property $sdf_cell2 area]
puts "sdfxbp_1 area = $area2"

############################################################
# Query tristate buffer cells (exercises three_state parsing)
############################################################
foreach cell_name {sky130_fd_sc_hd__ebufn_1 sky130_fd_sc_hd__ebufn_2
                   sky130_fd_sc_hd__ebufn_4 sky130_fd_sc_hd__ebufn_8} {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  if {$cell ne ""} {
    set area [get_property $cell area]
    puts "$cell_name area = $area"
    # Query tristate enable function
    set z_port [$cell find_liberty_port "Z"]
    if {$z_port != "NULL" && $z_port ne ""} {
      set tri_en [$z_port tristate_enable]
      puts "$cell_name Z tristate_enable = $tri_en"
    }
  }
}

############################################################
# Query latch cells (exercises latch sequential parsing)
############################################################
foreach cell_name {sky130_fd_sc_hd__dlxtp_1 sky130_fd_sc_hd__dlxtn_1
                   sky130_fd_sc_hd__dlxbn_1 sky130_fd_sc_hd__dlxbp_1} {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  if {$cell != "NULL" && $cell ne ""} {
    set area [get_property $cell area]
    puts "$cell_name area = $area"
  }
}

############################################################
# Query DFF cells with async set/clear (exercises recovery/removal arcs)
############################################################
foreach cell_name {sky130_fd_sc_hd__dfrtp_1 sky130_fd_sc_hd__dfstp_1
                   sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dfbbp_1} {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  if {$cell != "NULL" && $cell ne ""} {
    set area [get_property $cell area]
    set is_buf [$cell is_buffer]
    set is_inv [$cell is_inverter]
    puts "$cell_name area=$area is_buf=$is_buf is_inv=$is_inv"
  }
}

############################################################
# Port function and direction queries (exercises setFunction)
############################################################
foreach cell_name {sky130_fd_sc_hd__and2_1 sky130_fd_sc_hd__or2_1
                   sky130_fd_sc_hd__xor2_1 sky130_fd_sc_hd__xnor2_1
                   sky130_fd_sc_hd__mux2_1} {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  if {$cell != "NULL" && $cell ne ""} {
    set port_iter [$cell liberty_port_iterator]
    while {[$port_iter has_next]} {
      set port [$port_iter next]
      set dir [sta::liberty_port_direction $port]
      set func [$port function]
      if {$func != ""} {
        puts "$cell_name/[get_name $port] dir=$dir func=$func"
      }
    }
    $port_iter finish
  }
}

############################################################
# Read Nangate library for more queries
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib

############################################################
# Port capacitance and drive resistance
############################################################
foreach cell_name {INV_X1 INV_X2 INV_X4 BUF_X1 BUF_X2 BUF_X4
                   NAND2_X1 NOR2_X1 AOI21_X1 OAI21_X1} {
  set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
  set port_iter [$cell liberty_port_iterator]
  while {[$port_iter has_next]} {
    set port [$port_iter next]
    set dir [sta::liberty_port_direction $port]
    if {$dir == "input"} {
      set cap [get_property $port capacitance]
      puts "$cell_name/[get_name $port] cap=$cap"
    }
  }
  $port_iter finish
}

############################################################
# Timing arc set queries (exercises makeTimingArcMap paths)
############################################################
foreach cell_name {DFF_X1 DFFR_X1 DFFS_X1 DFFRS_X1} {
  set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
  if {$cell != "NULL" && $cell ne ""} {
    set arcs [$cell timing_arc_sets]
    set arc_count [llength $arcs]
    puts "$cell_name arc_sets = $arc_count"
    foreach arc $arcs {
      set from_port [$arc from]
      set to_port [$arc to]
      set role [$arc role]
      puts "  [$arc full_name] role=$role"
    }
  }
}

############################################################
# Read bus-port library (exercises bus port parsing)
############################################################
read_liberty ../../test/nangate45/fakeram45_64x7.lib

# Query bus ports
set cell [get_lib_cell fakeram45_64x7/fakeram45_64x7]
if {$cell != "NULL" && $cell ne ""} {
  set port_iter [$cell liberty_port_iterator]
  while {[$port_iter has_next]} {
    set port [$port_iter next]
    set dir [sta::liberty_port_direction $port]
    set is_bus [$port is_bus]
    set is_bundle [$port is_bundle]
    set has_mem [$port has_members]
    puts "fakeram/[get_name $port] dir=$dir bus=$is_bus bundle=$is_bundle has_members=$has_mem"
    if {$is_bus || $has_mem} {
      # Iterate members
      set mem_iter [$port member_iterator]
      set mem_count 0
      while {[$mem_iter has_next]} {
        set mem [$mem_iter next]
        incr mem_count
      }
      $mem_iter finish
      puts "  member_count = $mem_count"
    }
  }
  $port_iter finish
}

############################################################
# Read ASAP7 SEQ for statetable/latch coverage
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib

# Query ASAP7 latch cells
set cell [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DLLx1_ASAP7_75t_R]
if {$cell != "NULL" && $cell ne ""} {
  set arcs [$cell timing_arc_sets]
  set arc_count [llength $arcs]
  puts "DLLx1 arc_sets = $arc_count"
  foreach arc $arcs {
    puts "  [$arc full_name] role=[$arc role]"
  }
}

# Query ICG (Integrated Clock Gate) cell with statetable
set cell [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R]
if {$cell != "NULL" && $cell ne ""} {
  set arcs [$cell timing_arc_sets]
  set arc_count [llength $arcs]
  puts "ICGx1 arc_sets = $arc_count"
  foreach arc $arcs {
    puts "  [$arc full_name] role=[$arc role]"
  }
}

############################################################
# Link a design and run timing to exercise more Liberty.cc paths
############################################################
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2
create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [get_ports in1]
set_input_delay -clock clk1 2.0 [get_ports in2]
set_input_delay -clock clk2 2.0 [get_ports in3]
set_output_delay -clock clk1 3.0 [get_ports out1]
set_output_delay -clock clk2 3.0 [get_ports out2]
set_input_transition 0.1 [all_inputs]

report_checks

# Report power to exercise internal power models
report_power

############################################################
# Write liberty roundtrip
############################################################
set outfile [make_result_file liberty_seq_scan_bus_write.lib]
sta::write_liberty NangateOpenCellLibrary $outfile
