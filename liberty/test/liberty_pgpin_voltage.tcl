# Test pg_pin, voltage_map, supply voltage, and power group parsing.
source ../../test/helpers.tcl

############################################################
# Read Sky130 library (has pg_pin, voltage_map extensively)
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

set sky_lib [sta::find_liberty sky130_fd_sc_hd__tt_025C_1v80]

############################################################
# Check supply voltage existence
############################################################
puts "--- supply voltage queries ---"

# sky130hd has VPWR and VGND supply names
set vpwr_exists [sta::liberty_supply_exists "VPWR"]
puts "VPWR exists = $vpwr_exists"

set vgnd_exists [sta::liberty_supply_exists "VGND"]
puts "VGND exists = $vgnd_exists"

set vbp_exists [sta::liberty_supply_exists "VPB"]
puts "VPB exists = $vbp_exists"

set vbn_exists [sta::liberty_supply_exists "VNB"]
puts "VNB exists = $vbn_exists"

set nonexist [sta::liberty_supply_exists "BOGUS_SUPPLY"]
puts "BOGUS_SUPPLY exists = $nonexist"

############################################################
# Query PG pin ports (power/ground pins)
############################################################
puts "--- pg pin port queries ---"

# Inverter has VPWR, VGND, VPB, VNB pg_pins
set inv_cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1]
set port_iter [$inv_cell liberty_port_iterator]
while {[$port_iter has_next]} {
  set port [$port_iter next]
  set is_pg [$port is_pwr_gnd]
  set dir [sta::liberty_port_direction $port]
  set name [get_name $port]
  if {$is_pg} {
    puts "inv_1/$name dir=$dir is_pg=$is_pg"
  }
}
$port_iter finish

# Query PG pins on various cell types
foreach cell_name {sky130_fd_sc_hd__buf_1 sky130_fd_sc_hd__nand2_1
                   sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dfrtp_1
                   sky130_fd_sc_hd__ebufn_1 sky130_fd_sc_hd__dlclkp_1
                   sky130_fd_sc_hd__mux2_1 sky130_fd_sc_hd__sdfxtp_1} {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  if {$cell != "NULL" && $cell ne ""} {
    set pg_count 0
    set port_iter [$cell liberty_port_iterator]
    while {[$port_iter has_next]} {
      set port [$port_iter next]
      if {[$port is_pwr_gnd]} {
        incr pg_count
      }
    }
    $port_iter finish
    puts "$cell_name pg_pin_count=$pg_count"
  }
}

############################################################
# Leakage power with when conditions per state
# Sky130 has per-state leakage_power groups
############################################################
puts "--- leakage power per-state queries ---"

foreach cell_name {sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__inv_2
                   sky130_fd_sc_hd__inv_4 sky130_fd_sc_hd__inv_8
                   sky130_fd_sc_hd__buf_1 sky130_fd_sc_hd__buf_2
                   sky130_fd_sc_hd__nand2_1 sky130_fd_sc_hd__nand3_1
                   sky130_fd_sc_hd__nor2_1 sky130_fd_sc_hd__nor3_1
                   sky130_fd_sc_hd__and2_1 sky130_fd_sc_hd__and3_1
                   sky130_fd_sc_hd__or2_1 sky130_fd_sc_hd__or3_1
                   sky130_fd_sc_hd__xor2_1 sky130_fd_sc_hd__xnor2_1
                   sky130_fd_sc_hd__a21o_1 sky130_fd_sc_hd__a21oi_1
                   sky130_fd_sc_hd__a22o_1 sky130_fd_sc_hd__a22oi_1
                   sky130_fd_sc_hd__o21a_1 sky130_fd_sc_hd__o21ai_0
                   sky130_fd_sc_hd__o22a_1 sky130_fd_sc_hd__o22ai_1
                   sky130_fd_sc_hd__mux2_1 sky130_fd_sc_hd__mux2i_1
                   sky130_fd_sc_hd__mux4_1 sky130_fd_sc_hd__ha_1
                   sky130_fd_sc_hd__fa_1} {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  if {$cell != "NULL" && $cell ne ""} {
    set area [get_property $cell area]
    puts "$cell_name area=$area"
  }
}

# Sequential cells
foreach cell_name {sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dfxtp_2
                   sky130_fd_sc_hd__dfxtp_4 sky130_fd_sc_hd__dfrtp_1
                   sky130_fd_sc_hd__dfrtp_2 sky130_fd_sc_hd__dfrtp_4
                   sky130_fd_sc_hd__dfstp_1 sky130_fd_sc_hd__dfstp_2
                   sky130_fd_sc_hd__dfstp_4 sky130_fd_sc_hd__dfbbp_1
                   sky130_fd_sc_hd__dfbbp_2 sky130_fd_sc_hd__dlxtp_1
                   sky130_fd_sc_hd__dlxtn_1 sky130_fd_sc_hd__dlxbn_1
                   sky130_fd_sc_hd__sdfxtp_1 sky130_fd_sc_hd__sdfxtp_2
                   sky130_fd_sc_hd__sdfrtp_1 sky130_fd_sc_hd__sdfstp_1
                   sky130_fd_sc_hd__sdlclkp_1 sky130_fd_sc_hd__dlclkp_1} {
  set cell [get_lib_cell sky130_fd_sc_hd__tt_025C_1v80/$cell_name]
  if {$cell != "NULL" && $cell ne ""} {
    set area [get_property $cell area]
    puts "$cell_name area=$area"
  }
}

############################################################
# Report cells to exercise detailed pg_pin/power writer paths
############################################################
puts "--- detailed cell reports with pg_pin ---"

report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__inv_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfxtp_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dfrtp_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__ebufn_1
report_lib_cell sky130_fd_sc_hd__tt_025C_1v80/sky130_fd_sc_hd__dlclkp_1

############################################################
# Read IHP library (different voltage/supply naming)
############################################################
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p20V_25C.lib

foreach cell_name {sg13g2_inv_1 sg13g2_buf_1 sg13g2_nand2_1
                   sg13g2_nor2_1 sg13g2_and2_1 sg13g2_or2_1
                   sg13g2_dfrbp_1 sg13g2_dlhq_1} {
  set cell [get_lib_cell sg13g2_stdcell_typ_1p20V_25C/$cell_name]
  if {$cell != "NULL" && $cell ne ""} {
    set area [get_property $cell area]
    puts "IHP $cell_name area=$area"
    # Query pg pins
    set pg_count 0
    set port_iter [$cell liberty_port_iterator]
    while {[$port_iter has_next]} {
      set port [$port_iter next]
      if {[$port is_pwr_gnd]} {
        incr pg_count
      }
    }
    $port_iter finish
    puts "IHP $cell_name pg_pins=$pg_count"
  }
}

############################################################
# Read IHP second corner
############################################################
read_liberty ../../test/ihp-sg13g2/sg13g2_stdcell_typ_1p50V_25C.lib

############################################################
# Link design and run power analysis
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../sdc/test/sdc_test2.v
link_design sdc_test2

create_clock -name clk1 -period 10 [get_ports clk1]
create_clock -name clk2 -period 20 [get_ports clk2]
set_input_delay -clock clk1 2.0 [all_inputs]
set_output_delay -clock clk1 3.0 [all_outputs]
set_input_transition 0.1 [all_inputs]

report_power

report_power -digits 6

############################################################
# Write liberty roundtrip for Sky130 (with pg_pin groups)
############################################################
set outfile [make_result_file liberty_pgpin_voltage_write.lib]
sta::write_liberty sky130_fd_sc_hd__tt_025C_1v80 $outfile

# Read back the written library to verify
read_liberty $outfile
