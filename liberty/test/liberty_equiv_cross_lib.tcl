# Test equivalent cells across multiple libraries from different PDKs,
# exercising mapEquivCells and cross-library equivalence hashing.
source ../../test/helpers.tcl

############################################################
# Read multiple ASAP7 Vt flavors for cross-library equiv
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_LVT_FF_nldm_220122.lib.gz

read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_FF_nldm_220122.lib.gz

############################################################
# Make equiv cells for RVT library
############################################################
set rvt_lib [lindex [get_libs asap7sc7p5t_INVBUF_RVT_FF_nldm_211120] 0]
sta::make_equiv_cells $rvt_lib

# Find equiv cells in ASAP7 RVT
foreach cell_prefix {INVx BUFx} {
  foreach size {1 2 3 4 5 8 11 13 16} {
    set cell_name "${cell_prefix}${size}_ASAP7_75t_R"
    set cell [get_lib_cell asap7sc7p5t_INVBUF_RVT_FF_nldm_211120/$cell_name]
    if {$cell != "NULL" && $cell ne ""} {
      set equivs [sta::find_equiv_cells $cell]
      if {$equivs != ""} {
        puts "$cell_name equiv count = [llength $equivs]"
      } else {
        puts "$cell_name equiv count = 0"
      }
    }
  }
}

# Find library buffers
set rvt_buffers [sta::find_library_buffers $rvt_lib]

############################################################
# Make equiv cells for LVT library
############################################################
set lvt_lib [lindex [get_libs asap7sc7p5t_INVBUF_LVT_FF_nldm_211120] 0]
sta::make_equiv_cells $lvt_lib

foreach cell_prefix {INVx BUFx} {
  foreach size {1 2 4 8} {
    set cell_name "${cell_prefix}${size}_ASAP7_75t_L"
    set cell [get_lib_cell asap7sc7p5t_INVBUF_LVT_FF_nldm_211120/$cell_name]
    if {$cell != "NULL" && $cell ne ""} {
      set equivs [sta::find_equiv_cells $cell]
      if {$equivs != ""} {
        puts "LVT $cell_name equiv count = [llength $equivs]"
      } else {
        puts "LVT $cell_name equiv count = 0"
      }
    }
  }
}

set lvt_buffers [sta::find_library_buffers $lvt_lib]

############################################################
# Make equiv cells for SLVT library
############################################################
set slvt_lib [lindex [get_libs asap7sc7p5t_INVBUF_SLVT_FF_nldm_211120] 0]
sta::make_equiv_cells $slvt_lib

set slvt_buffers [sta::find_library_buffers $slvt_lib]

############################################################
# Cross-Vt equiv_cells comparisons
############################################################
puts "--- cross-Vt equiv comparisons ---"

# RVT vs LVT (different cell name suffix -> not equiv)
  set rvt_inv [get_lib_cell asap7sc7p5t_INVBUF_RVT_FF_nldm_211120/INVx1_ASAP7_75t_R]
  set lvt_inv [get_lib_cell asap7sc7p5t_INVBUF_LVT_FF_nldm_211120/INVx1_ASAP7_75t_L]
  set result [sta::equiv_cells $rvt_inv $lvt_inv]
  puts "equiv RVT/LVT INVx1 = $result"
  set result [sta::equiv_cell_ports $rvt_inv $lvt_inv]
  puts "port_equiv RVT/LVT INVx1 = $result"
  set result [sta::equiv_cell_timing_arcs $rvt_inv $lvt_inv]
  puts "arc_equiv RVT/LVT INVx1 = $result"

############################################################
# Read ASAP7 SEQ libraries for sequential equiv
############################################################
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_LVT_FF_nldm_220123.lib

set seq_rvt_lib [lindex [get_libs asap7sc7p5t_SEQ_RVT_FF_nldm_220123] 0]
sta::make_equiv_cells $seq_rvt_lib

# Find equiv cells for DFF cells
set dff [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R]
if {$dff != "NULL" && $dff ne ""} {
  set equivs [sta::find_equiv_cells $dff]
  if {$equivs != ""} {
    puts "SEQ RVT DFFHQNx1 equiv count = [llength $equivs]"
    foreach eq $equivs {
      puts "  equiv: [$eq name]"
    }
  } else {
    puts "SEQ RVT DFFHQNx1 equiv count = 0"
  }
}

# ICG equiv cells
set icg [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/ICGx1_ASAP7_75t_R]
if {$icg != "NULL" && $icg ne ""} {
  set equivs [sta::find_equiv_cells $icg]
  if {$equivs != ""} {
    puts "SEQ RVT ICGx1 equiv count = [llength $equivs]"
  } else {
    puts "SEQ RVT ICGx1 equiv count = 0"
  }
}

# Latch equiv cells
set dll [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DLLx1_ASAP7_75t_R]
if {$dll != "NULL" && $dll ne ""} {
  set equivs [sta::find_equiv_cells $dll]
  if {$equivs != ""} {
    puts "SEQ RVT DLLx1 equiv count = [llength $equivs]"
  } else {
    puts "SEQ RVT DLLx1 equiv count = 0"
  }
}

# SDFF equiv cells
set sdff [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/SDFHQNx1_ASAP7_75t_R]
if {$sdff != "NULL" && $sdff ne ""} {
  set equivs [sta::find_equiv_cells $sdff]
  if {$equivs != ""} {
    puts "SEQ RVT SDFHQNx1 equiv count = [llength $equivs]"
  } else {
    puts "SEQ RVT SDFHQNx1 equiv count = 0"
  }
}

############################################################
# Cross-library comparisons of DFF cells
############################################################
  set rvt_dff [get_lib_cell asap7sc7p5t_SEQ_RVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_R]
  set lvt_dff [get_lib_cell asap7sc7p5t_SEQ_LVT_FF_nldm_220123/DFFHQNx1_ASAP7_75t_L]
  set result [sta::equiv_cells $rvt_dff $lvt_dff]
  puts "equiv SEQ RVT/LVT DFFHQNx1 = $result"
  set result [sta::equiv_cell_ports $rvt_dff $lvt_dff]
  puts "port_equiv SEQ RVT/LVT DFFHQNx1 = $result"

############################################################
# Read Sky130 and make equiv cells for a very different PDK
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

set sky_lib [lindex [get_libs sky130_fd_sc_hd__tt_025C_1v80] 0]
sta::make_equiv_cells $sky_lib

# Find equiv cells for Sky130 inverters
set sky_inv [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1]
if {$sky_inv != "NULL" && $sky_inv ne ""} {
  set equivs [sta::find_equiv_cells $sky_inv]
  if {$equivs != ""} {
    puts "Sky130 inv_1 equiv count = [llength $equivs]"
    foreach eq $equivs {
      puts "  equiv: [$eq name]"
    }
  } else {
    puts "Sky130 inv_1 equiv count = 0"
  }
}

# Find equiv for Sky130 DFF
set sky_dff [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfxtp_1]
if {$sky_dff != "NULL" && $sky_dff ne ""} {
  set equivs [sta::find_equiv_cells $sky_dff]
  if {$equivs != ""} {
    puts "Sky130 dfxtp_1 equiv count = [llength $equivs]"
    foreach eq $equivs {
      puts "  equiv: [$eq name]"
    }
  } else {
    puts "Sky130 dfxtp_1 equiv count = 0"
  }
}

set sky_buffers [sta::find_library_buffers $sky_lib]
