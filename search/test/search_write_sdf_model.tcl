# Test write_sdf, write_timing_model with various options, and network edit operations.
# Targets: Sta.cc writeSdf, writeTimingModel, makeInstance, deleteInstance,
#          replaceCell, makeNet, connectPin, disconnectPin, makePortPin
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Run timing
report_checks -path_delay max > /dev/null

puts "--- write_sdf ---"
set sdf_file [make_result_file "search_test1.sdf"]
write_sdf $sdf_file

puts "--- write_sdf with options ---"
set sdf_file2 [make_result_file "search_test1_opts.sdf"]
write_sdf -divider . -no_timestamp -no_version $sdf_file2

puts "--- write_sdf with digits ---"
set sdf_file3 [make_result_file "search_test1_digits.sdf"]
write_sdf -digits 6 -no_timestamp $sdf_file3

puts "--- write_sdf with include_typ ---"
set sdf_file4 [make_result_file "search_test1_typ.sdf"]
write_sdf -include_typ -no_timestamp $sdf_file4

puts "--- write_timing_model ---"
set model_file [make_result_file "search_test1_model.lib"]
write_timing_model $model_file

puts "--- write_timing_model with cell_name ---"
set model_file2 [make_result_file "search_test1_model2.lib"]
write_timing_model -cell_name my_custom_cell $model_file2

puts "--- write_timing_model with library_name ---"
set model_file3 [make_result_file "search_test1_model3.lib"]
write_timing_model -library_name my_custom_lib -cell_name my_custom_cell2 $model_file3

puts "--- Network edit: make_instance ---"
catch {
  make_instance new_buf1 [get_lib_cells NangateOpenCellLibrary/BUF_X1]
  puts "make_instance new_buf1 done"
}

puts "--- Network edit: make_net ---"
catch {
  make_net new_net1
  puts "make_net new_net1 done"
}

puts "--- Network edit: connect_pin ---"
catch {
  connect_pin new_buf1 A [get_nets n1]
  puts "connect_pin done"
}

puts "--- Network edit: disconnect_pin ---"
catch {
  disconnect_pin new_buf1 A
  puts "disconnect_pin done"
}

puts "--- Network edit: delete_net ---"
catch {
  delete_net [get_nets new_net1]
  puts "delete_net done"
}

puts "--- Network edit: delete_instance ---"
catch {
  delete_instance [get_cells new_buf1]
  puts "delete_instance done"
}

puts "--- Network edit: replace_cell ---"
catch {
  replace_cell [get_cells buf1] [get_lib_cells NangateOpenCellLibrary/BUF_X2]
  report_checks -path_delay max
  puts "replace_cell done"
}

puts "--- report_checks after edits ---"
report_checks -path_delay max
report_checks -path_delay min

puts "--- write_timing_model after edits ---"
set model_file4 [make_result_file "search_test1_model_edited.lib"]
write_timing_model -library_name edited_lib -cell_name edited_cell $model_file4

puts "--- write_sdf after edits ---"
set sdf_file5 [make_result_file "search_test1_edited.sdf"]
write_sdf -no_timestamp -no_version $sdf_file5
