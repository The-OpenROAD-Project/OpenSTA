# Test bus port member iteration, bundle ports, port functions,
# tristate enable, sequential queries, and diverse cell classification.
source ../../test/helpers.tcl

############################################################
# Read SRAM macro library (has bus ports)
############################################################
read_liberty ../../test/nangate45/fakeram45_64x7.lib

# Query bus port properties
set cell [get_lib_cell fakeram45_64x7/fakeram45_64x7]
puts "fakeram cell found"

set port_iter [$cell liberty_port_iterator]
while {[$port_iter has_next]} {
  set port [$port_iter next]
  set name [get_name $port]
  set dir [sta::liberty_port_direction $port]
  set is_bus [$port is_bus]
  set is_bit [$port is_bus_bit]
  set is_bundle [$port is_bundle]
  set is_bm [$port is_bundle_member]
  set has_mem [$port has_members]
  set func [$port function]
  set tri [$port tristate_enable]
  puts "  $name dir=$dir bus=$is_bus bit=$is_bit bundle=$is_bundle bm=$is_bm members=$has_mem func=\"$func\" tri=\"$tri\""
  if {$has_mem} {
    set mem_iter [$port member_iterator]
    set count 0
    while {[$mem_iter has_next]} {
      set mem [$mem_iter next]
      set mname [get_name $mem]
      set mdir [sta::liberty_port_direction $mem]
      set m_is_bit [$mem is_bus_bit]
      if {$count < 3} {
        puts "    member[$count]: $mname dir=$mdir bit=$m_is_bit"
      }
      incr count
    }
    $mem_iter finish
    puts "    total members=$count"
  }
}
$port_iter finish

############################################################
# Read other SRAM macros with different bus widths
############################################################
foreach lib_name {fakeram45_64x32 fakeram45_256x16 fakeram45_512x64
                  fakeram45_1024x32 fakeram45_64x96} {
  read_liberty ../../test/nangate45/${lib_name}.lib
  set cell [get_lib_cell ${lib_name}/${lib_name}]
  if {$cell != "NULL"} {
    set port_iter [$cell liberty_port_iterator]
    set bus_count 0
    set bit_count 0
    while {[$port_iter has_next]} {
      set port [$port_iter next]
      if {[$port is_bus]} {
        incr bus_count
        set mem_iter [$port member_iterator]
        while {[$mem_iter has_next]} {
          set mem [$mem_iter next]
          incr bit_count
        }
        $mem_iter finish
      }
    }
    $port_iter finish
    puts "$lib_name: bus_ports=$bus_count total_bits=$bit_count"
  }
}

############################################################
# Read SRAM macro from GF180MCU
############################################################
read_liberty ../../test/gf180mcu_sram.lib.gz

set gf_cells [get_lib_cells gf180mcu_fd_ip_sram__sram256x8m8wm1/*]
puts "gf180mcu cells: [llength $gf_cells]"
foreach cell_obj $gf_cells {
  set cname [get_full_name $cell_obj]
  catch {
    set cell [get_lib_cell $cname]
    set port_iter [$cell liberty_port_iterator]
    set bus_count 0
    while {[$port_iter has_next]} {
      set port [$port_iter next]
      if {[$port is_bus] || [$port has_members]} {
        incr bus_count
      }
    }
    $port_iter finish
    puts "  [get_name $cell_obj]: bus_ports=$bus_count"
  }
}

############################################################
# Read Nangate for cell classification queries
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib

# Cell classification
foreach cell_name {INV_X1 INV_X2 BUF_X1 BUF_X2 CLKBUF_X1
                   NAND2_X1 NOR2_X1 AND2_X1 OR2_X1 XOR2_X1
                   MUX2_X1 AOI21_X1 OAI21_X1 AOI22_X1 OAI22_X1
                   DFF_X1 DFF_X2 DFFR_X1 DFFS_X1 DFFRS_X1
                   SDFF_X1 SDFFR_X1 SDFFRS_X1 TLAT_X1
                   TINV_X1 CLKGATETST_X1 HA_X1 FA_X1
                   ANTENNA_X1 FILLCELL_X1 FILLCELL_X2 LOGIC0_X1 LOGIC1_X1} {
  set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
  if {$cell != "NULL"} {
    set is_leaf [$cell is_leaf]
    set is_buf [$cell is_buffer]
    set is_inv [$cell is_inverter]
    set area [get_property $cell area]
    set du [get_property $cell dont_use]
    set arc_sets [$cell timing_arc_sets]
    set arc_count [llength $arc_sets]
    puts "$cell_name leaf=$is_leaf buf=$is_buf inv=$is_inv area=$area du=$du arcs=$arc_count"
  }
}

############################################################
# Test cell and scan signal type queries
############################################################
puts "--- test_cell / scan queries ---"

# SDFF has test_cell
set sdff [get_lib_cell NangateOpenCellLibrary/SDFF_X1]
set tc [$sdff test_cell]
if {$tc != "NULL"} {
  puts "SDFF_X1 has test_cell"
} else {
  puts "SDFF_X1 test_cell is null"
}

set sdffr [get_lib_cell NangateOpenCellLibrary/SDFFR_X1]
set tc [$sdffr test_cell]
if {$tc != "NULL"} {
  puts "SDFFR_X1 has test_cell"
} else {
  puts "SDFFR_X1 test_cell is null"
}

set sdffrs [get_lib_cell NangateOpenCellLibrary/SDFFRS_X1]
set tc [$sdffrs test_cell]
if {$tc != "NULL"} {
  puts "SDFFRS_X1 has test_cell"
} else {
  puts "SDFFRS_X1 test_cell is null"
}

# Regular DFF should NOT have test_cell
set dff [get_lib_cell NangateOpenCellLibrary/DFF_X1]
set tc [$dff test_cell]
if {$tc != "NULL"} {
  puts "DFF_X1 has test_cell (unexpected)"
} else {
  puts "DFF_X1 has no test_cell (expected)"
}

############################################################
# Port function and tristate enable queries
############################################################
puts "--- function and tristate queries ---"

# Tristate inverter
set tinv [get_lib_cell NangateOpenCellLibrary/TINV_X1]
set port_iter [$tinv liberty_port_iterator]
while {[$port_iter has_next]} {
  set port [$port_iter next]
  set name [get_name $port]
  set dir [sta::liberty_port_direction $port]
  set func [$port function]
  set tri [$port tristate_enable]
  puts "TINV_X1/$name dir=$dir func=\"$func\" tri=\"$tri\""
}
$port_iter finish

# Clock gate tester
set clkgt [get_lib_cell NangateOpenCellLibrary/CLKGATETST_X1]
set port_iter [$clkgt liberty_port_iterator]
while {[$port_iter has_next]} {
  set port [$port_iter next]
  set name [get_name $port]
  set dir [sta::liberty_port_direction $port]
  set func [$port function]
  puts "CLKGATETST_X1/$name dir=$dir func=\"$func\""
}
$port_iter finish

# Output functions for various logic cells
foreach cell_name {INV_X1 BUF_X1 NAND2_X1 NOR2_X1 AND2_X1 OR2_X1
                   XOR2_X1 XNOR2_X1 AOI21_X1 OAI21_X1 MUX2_X1
                   HA_X1 FA_X1} {
  set cell [get_lib_cell NangateOpenCellLibrary/$cell_name]
  set port_iter [$cell liberty_port_iterator]
  while {[$port_iter has_next]} {
    set port [$port_iter next]
    set dir [sta::liberty_port_direction $port]
    if {$dir == "output"} {
      set func [$port function]
      if {$func != ""} {
        puts "$cell_name/[get_name $port] func=$func"
      }
    }
  }
  $port_iter finish
}

############################################################
# Read Sky130 for tristate and latch port queries
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

# Tristate buffer port queries
foreach cell_name {sky130_fd_sc_hd__ebufn_1 sky130_fd_sc_hd__ebufn_2} {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  set port_iter [$cell liberty_port_iterator]
  while {[$port_iter has_next]} {
    set port [$port_iter next]
    set name [get_name $port]
    set dir [sta::liberty_port_direction $port]
    set func [$port function]
    set tri [$port tristate_enable]
    set is_pg [$port is_pwr_gnd]
    if {!$is_pg} {
      puts "$cell_name/$name dir=$dir func=\"$func\" tri=\"$tri\""
    }
  }
  $port_iter finish
}

############################################################
# Read fake_macros library for memory/macro classification
############################################################
read_liberty ../../test/nangate45/fake_macros.lib

############################################################
# Write roundtrip with bus ports
############################################################
set outfile [make_result_file liberty_busport_mem_iter_write.lib]
sta::write_liberty fakeram45_64x7 $outfile

# Read back
catch {
  read_liberty $outfile
} msg
if {[string match "Error*" $msg]} {
  puts "INFO: roundtrip issue: [string range $msg 0 80]"
}
