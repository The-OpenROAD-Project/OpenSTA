# SWIG Seq* double-free if %typemap(freearg) and C++ both delete.
# Successful get_* -filter, get_fanin/fanout, write_verilog -remove_cells,
# and report_path_ends all allocate a Seq in the in typemap.
# Run under -fsanitize=address or ./regression -valgrind.
source helpers.tcl
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
create_clock -name vclk -period 1000
set_input_delay -clock clk 0 {in1 in2}

puts {[get_cells -filter liberty_cell==BUFx2_ASAP7_75t_R *]}
report_object_full_names [get_cells -filter liberty_cell==BUFx2_ASAP7_75t_R *]
puts {[get_pins -filter direction==input r1/*]}
report_object_full_names [get_pins -filter direction==input r1/*]
puts {[get_ports -filter direction==output *]}
report_object_full_names [get_ports -filter direction==output *]
puts {[get_nets -filter name=~*q *]}
report_object_full_names [get_nets -filter name=~*q *]
puts {[get_clocks -filter is_virtual *]}
report_object_full_names [get_clocks -filter is_virtual *]
puts {[get_lib_cells -filter is_buffer *]}
report_object_full_names [get_lib_cells -filter is_buffer *]
puts {[get_lib_pins -filter direction==output BUFx2_ASAP7_75t_R/*]}
report_object_full_names [get_lib_pins -filter direction==output BUFx2_ASAP7_75t_R/*]
puts {[get_libs -filter name==asap7_small *]}
report_object_full_names [get_libs -filter name==asap7_small *]

puts {[get_timing_edges -of_objects u1 -filter {from_pin =~ *}]}
puts [llength [get_timing_edges -of_objects u1 -filter {from_pin =~ *}]]

puts {[get_fanin -to out]}
report_object_full_names [get_fanin -to out]
puts {[get_fanout -from in1]}
report_object_full_names [get_fanout -from in1]
puts {[get_fanin -only_cells -to r3/D]}
report_object_full_names [get_fanin -only_cells -to r3/D]

# PathEndSeq* input to report_path_ends.
with_output_to_variable ignored {
  sta::report_path_ends [find_timing_paths -group_path_count 1]
}
puts report_path_ends

# CellSeq* input to write_verilog_cmd.
set verilog_file [make_result_file "swig_seq_double_free.v"]
write_verilog -remove_cells BUFx2_ASAP7_75t_R $verilog_file
report_file $verilog_file

# ExceptionThruSeq* is stored by set_false_path; freearg would use-after-free.
set_false_path -through u1
with_output_to_variable ignored {
  report_checks -format end -group_path_count 1
}
puts false_path
