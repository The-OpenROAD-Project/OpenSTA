# write_timing_model writes an output a constant drives as that
# constant, as a tie cell's output is. A parent timed against the model
# propagates it: the registers on blk's constant outputs hold the
# constant and are not unconstrained endpoints, and the register on q
# is timed through the model's arc. Only the netlist's constants: zte is
# constant while the block's SDC holds te at 0, a mode the parent need
# not be in, so the model keeps its arcs and the register on it is timed.
source ../../test/helpers.tcl
suppress_msg 1212

read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_verilog search_timing_model_constant.v
link_design blk
create_clock -name clk -period 1000 [get_ports clk]
set_input_delay 0 -clock clk [get_ports {d te}]
set_output_delay 0 -clock clk [get_ports {q z0 z1 zand zte}]
set_case_analysis 0 [get_ports te]
set model_file [make_result_file search_timing_model_constant.lib]
write_timing_model -library_name blk_lib -cell_name blk_model $model_file

# write_timing_model added blk_model to the libraries.
read_verilog search_timing_model_constant.v
link_design top
create_clock -name clk -period 1000 [get_ports clk]
set_input_delay 0 -clock clk [get_ports {d te}]

puts "--- constant outputs ---"
foreach pin {u/z0 u/z1 u/zand u/zte r0/D r1/D rz/D rte/D} {
  puts "$pin [sta::pin_sim_logic_value [get_pins $pin]]"
}
puts "--- check_setup -unconstrained_endpoints ---"
check_setup -verbose -unconstrained_endpoints
puts "--- rq/D ---"
report_checks -to rq/D -format end
puts "--- rte/D ---"
report_checks -to rte/D -format end
