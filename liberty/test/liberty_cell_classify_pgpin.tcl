# Test cell classification (isBuffer, isInverter, isClockGate, etc.),
# pg_pin iteration, bus port member iteration, internal power queries,
# and port function queries across multiple PDKs.
source ../../test/helpers.tcl

############################################################
# Read libraries with pg_pin info (Sky130 has pg_pin groups)
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

read_liberty ../../test/nangate45/Nangate45_typ.lib

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib

read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

############################################################
# Cell classification queries on Nangate45
############################################################
puts "--- Nangate45 cell classification ---"

# Buffers
set buf_x1 [get_lib_cell NangateOpenCellLibrary/BUF_X1]
puts "BUF_X1 is_buffer = [$buf_x1 is_buffer]"
puts "BUF_X1 is_inverter = [$buf_x1 is_inverter]"
puts "BUF_X1 is_leaf = [$buf_x1 is_leaf]"

# Inverters
set inv_x1 [get_lib_cell NangateOpenCellLibrary/INV_X1]
puts "INV_X1 is_buffer = [$inv_x1 is_buffer]"
puts "INV_X1 is_inverter = [$inv_x1 is_inverter]"

# Clock gate cells
catch {
  set clkgate [get_lib_cell NangateOpenCellLibrary/CLKGATETST_X1]
  puts "CLKGATETST_X1 is_buffer = [$clkgate is_buffer]"
  puts "CLKGATETST_X1 is_inverter = [$clkgate is_inverter]"
}

# DFF
set dff [get_lib_cell NangateOpenCellLibrary/DFF_X1]
puts "DFF_X1 is_buffer = [$dff is_buffer]"
puts "DFF_X1 is_inverter = [$dff is_inverter]"
puts "DFF_X1 is_leaf = [$dff is_leaf]"

# Test cell for scan DFF
catch {
  set sdff [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
  set tc [$sdff test_cell]
  puts "SDFF_X1 test_cell = $tc"
}

############################################################
# Port function queries (exercises FuncExpr::to_string)
############################################################
puts "--- port function queries ---"
foreach {lib_name cell_name} {
  NangateOpenCellLibrary INV_X1
  NangateOpenCellLibrary BUF_X1
  NangateOpenCellLibrary NAND2_X1
  NangateOpenCellLibrary NOR2_X1
  NangateOpenCellLibrary AND2_X1
  NangateOpenCellLibrary OR2_X1
  NangateOpenCellLibrary XOR2_X1
  NangateOpenCellLibrary XNOR2_X1
  NangateOpenCellLibrary AOI21_X1
  NangateOpenCellLibrary OAI21_X1
  NangateOpenCellLibrary MUX2_X1
  NangateOpenCellLibrary HA_X1
  NangateOpenCellLibrary FA_X1
} {
  set cell [get_lib_cell $lib_name/$cell_name]
  set port_iter [$cell liberty_port_iterator]
  while {[$port_iter has_next]} {
    set port [$port_iter next]
    set func [$port function]
    set tri [$port tristate_enable]
    set dir [sta::liberty_port_direction $port]
    set pwr [$port is_pwr_gnd]
    if {$func != ""} {
      puts "  $cell_name/[$port bus_name] func=$func dir=$dir"
    }
    if {$tri != ""} {
      puts "  $cell_name/[$port bus_name] tristate=$tri"
    }
  }
  $port_iter finish
}

############################################################
# Bus port and member iteration
############################################################
puts "--- bus port member iteration ---"

# ASAP7 SEQ has bus ports in some cells
set asap7_libs [get_libs asap7sc7p5t_SEQ_RVT_FF_nldm_220123]
set asap7_seq_lib [lindex $asap7_libs 0]
set asap7_cells [get_lib_cells asap7sc7p5t_SEQ_RVT_FF_nldm_220123/*]
foreach cell_obj $asap7_cells {
  set cname [$cell_obj name]
  set port_iter [$cell_obj liberty_port_iterator]
  while {[$port_iter has_next]} {
    set port [$port_iter next]
    if {[$port is_bus]} {
      puts "$cname/[$port bus_name] is_bus=1 has_members=[$port has_members]"
      set mem_iter [$port member_iterator]
      while {[$mem_iter has_next]} {
        set member [$mem_iter next]
        puts "  member: [$member bus_name] is_bus_bit=[$member is_bus_bit]"
      }
      $mem_iter finish
    }
    if {[$port is_bundle]} {
      puts "$cname/[$port bus_name] is_bundle=1"
    }
  }
  $port_iter finish
}

############################################################
# Port capacitance with corner/min_max
############################################################
puts "--- port capacitance corner ---"
foreach cell_name {INV_X1 INV_X4 INV_X16 BUF_X1 BUF_X8 NAND2_X1 DFF_X1} {
  set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
  set port_iter [$cell liberty_port_iterator]
  while {[$port_iter has_next]} {
    set port [$port_iter next]
    set dir [sta::liberty_port_direction $port]
    if {$dir == "input"} {
      catch {
        set cap [$port capacitance [sta::cmd_corner] [sta::find_min_max "max"]]
        puts "$cell_name/[$port bus_name] cap(max)=$cap"
      }
      catch {
        set cap [$port capacitance [sta::cmd_corner] [sta::find_min_max "min"]]
        puts "$cell_name/[$port bus_name] cap(min)=$cap"
      }
    }
  }
  $port_iter finish
}

############################################################
# Timing arc set queries
############################################################
puts "--- timing arc sets ---"
foreach cell_name {INV_X1 BUF_X1 DFF_X1 DFFR_X1 NAND2_X1 AOI21_X1 MUX2_X1 SDFF_X1 CLKGATETST_X1} {
  set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
  set arc_sets [$cell timing_arc_sets]
  puts "$cell_name arc_sets=[llength $arc_sets]"
  foreach arc_set $arc_sets {
    set from_port [$arc_set from]
    set to_port [$arc_set to]
    set role [$arc_set role]
    set is_check [sta::timing_role_is_check $role]
    set from_name [$from_port bus_name]
    set to_name [$to_port bus_name]
    puts "  $from_name -> $to_name is_check=$is_check"
    # Query timing arcs within the set
    set arcs [$arc_set timing_arcs]
    foreach arc $arcs {
      set from_edge [$arc from_edge_name]
      set to_edge [$arc to_edge_name]
      puts "    $from_edge -> $to_edge"
    }
  }
}

############################################################
# Sky130 cell queries (has pg_pin groups, different features)
############################################################
puts "--- Sky130 cell queries ---"
foreach cell_name {
  sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__inv_2 sky130_fd_sc_hd__inv_4
  sky130_fd_sc_hd__buf_1 sky130_fd_sc_hd__buf_2
  sky130_fd_sc_hd__nand2_1 sky130_fd_sc_hd__nor2_1
  sky130_fd_sc_hd__and2_1 sky130_fd_sc_hd__or2_1
  sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dfrtp_1
  sky130_fd_sc_hd__mux2_1
  sky130_fd_sc_hd__dlxtp_1
  sky130_fd_sc_hd__ebufn_1
} {
  catch {
    set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
    puts "$cell_name is_buffer=[$cell is_buffer] is_inverter=[$cell is_inverter]"
    set port_iter [$cell liberty_port_iterator]
    while {[$port_iter has_next]} {
      set port [$port_iter next]
      set func [$port function]
      set dir [sta::liberty_port_direction $port]
      set pwr [$port is_pwr_gnd]
      if {$pwr} {
        puts "  [$port bus_name] pwr_gnd=1"
      }
    }
    $port_iter finish
  }
}

############################################################
# Operating conditions (exercises find_operating_conditions)
############################################################
puts "--- operating conditions ---"
set sky_lib [lindex [get_libs sky130_fd_sc_hd__tt_025C_1v80] 0]
set default_oc [$sky_lib default_operating_conditions]
if {$default_oc != "NULL"} {
  puts "Sky130 default OC process=[$default_oc process] voltage=[$default_oc voltage] temp=[$default_oc temperature]"
}

############################################################
# IHP cell queries (different vendor, might have different features)
############################################################
puts "--- IHP cell queries ---"
foreach cell_name {
  sg13g2_inv_1 sg13g2_inv_2
  sg13g2_buf_1 sg13g2_buf_2
  sg13g2_nand2_1 sg13g2_nor2_1
  sg13g2_and2_1
  sg13g2_dfrbp_1 sg13g2_dfrbp_2
  sg13g2_ebufn_2
} {
  catch {
    set cell [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/$cell_name]
    puts "$cell_name is_buffer=[$cell is_buffer] is_inverter=[$cell is_inverter]"
    set arc_sets [$cell timing_arc_sets]
    puts "  arc_sets=[llength $arc_sets]"
  }
}

############################################################
# Ensure voltage waveforms (exercises ensureVoltageWaveforms)
############################################################
puts "--- ensure voltage waveforms ---"
catch {
  set inv [get_lib_cell NangateOpenCellLibrary/INV_X1]
  $inv ensure_voltage_waveforms
}

catch {
  set dff [get_lib_cell NangateOpenCellLibrary/DFF_X1]
  $dff ensure_voltage_waveforms
}

############################################################
# Liberty cell matching with regex patterns
############################################################
puts "--- liberty cell matching ---"
set ng_lib [lindex [get_libs NangateOpenCellLibrary] 0]
set inv_matches [$ng_lib find_liberty_cells_matching "INV_*" 0 0]
puts "INV_* matches = [llength $inv_matches]"

set dff_matches [$ng_lib find_liberty_cells_matching "DFF*" 0 0]
puts "DFF* matches = [llength $dff_matches]"

set all_matches [$ng_lib find_liberty_cells_matching "*" 0 0]
puts "* matches = [llength $all_matches]"

# Regex matching
set regex_matches [$ng_lib find_liberty_cells_matching {^INV_X[0-9]+$} 1 0]
puts "regex INV_X matches = [llength $regex_matches]"

# Port matching on a cell
set inv [get_lib_cell NangateOpenCellLibrary/INV_X1]
set port_matches [$inv find_liberty_ports_matching "*" 0 0]
puts "INV_X1 port * matches = [llength $port_matches]"

set port_matches [$inv find_liberty_ports_matching "A" 0 0]
puts "INV_X1 port A matches = [llength $port_matches]"
