# Test multi-corner library reading and timing analysis with Sky130HD.
source ../../test/helpers.tcl

############################################################
# Define corners and read Sky130HD libraries per corner
############################################################
define_corners fast slow

read_liberty -corner fast ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib
read_liberty -corner slow ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib

############################################################
# Read design and link
############################################################
read_verilog sky130_corners_test.v
link_design sky130_corners_test

############################################################
# Create constraints
############################################################
create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 2.0 [get_ports in1]
set_input_delay -clock clk 2.0 [get_ports in2]
set_output_delay -clock clk 3.0 [get_ports out1]
set_output_delay -clock clk 3.0 [get_ports out2]

############################################################
# Report timing per corner (shows different delays per corner)
############################################################
puts "--- Fast corner, max ---"
report_checks -corner fast -path_delay max

puts "--- Slow corner, max ---"
report_checks -corner slow -path_delay max

puts "--- Fast corner, min ---"
report_checks -corner fast -path_delay min

puts "--- Slow corner, min ---"
report_checks -corner slow -path_delay min

############################################################
# Comprehensive cell reports - fast corner library
############################################################
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
  sky130_fd_sc_hd__diode_2
}

foreach cell_name $sky130_cells_to_report {
  report_lib_cell sky130_fd_sc_hd__ff_n40C_1v95/$cell_name
}

############################################################
# Cell property queries - slow corner library
############################################################
foreach cell_name {sky130_fd_sc_hd__inv_1 sky130_fd_sc_hd__buf_1
                   sky130_fd_sc_hd__dfxtp_1 sky130_fd_sc_hd__dlxtp_1
                   sky130_fd_sc_hd__sdfxtp_1 sky130_fd_sc_hd__ebufn_1
                   sky130_fd_sc_hd__mux2_1 sky130_fd_sc_hd__fa_1} {
  set cell [get_lib_cell sky130_fd_sc_hd__ss_n40C_1v40/$cell_name]
  set area [get_property $cell area]
  set du [get_property $cell dont_use]
  puts "$cell_name: area=$area dont_use=$du"
}

############################################################
# Pin capacitance queries - fast corner library
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
  set pin [get_lib_pin sky130_fd_sc_hd__ff_n40C_1v95/$cell_name/$pin_name]
  set cap [get_property $pin capacitance]
  set dir [sta::liberty_port_direction $pin]
  puts "$cell_name/$pin_name: cap=$cap dir=$dir"
}

############################################################
# Write libraries to exercise writer paths
############################################################
set outfile1 [make_result_file liberty_sky130_hd_ff.lib]
sta::write_liberty sky130_fd_sc_hd__ff_n40C_1v95 $outfile1

set outfile2 [make_result_file liberty_sky130_hd_ss.lib]
sta::write_liberty sky130_fd_sc_hd__ss_n40C_1v40 $outfile2
