# Tests whether get_lib_pins -of_objects command works correctly

# Read in design and libraries
read_liberty asap7_small.lib.gz
read_verilog reg1_asap7.v
link_design top

# Get the pins of the library cells
puts {[get_lib_pins -of_objects [get_lib_cells asap7_small/AND2x2_ASAP7_75t_R]]}
report_object_full_names [get_lib_pins -of_objects [get_lib_cells asap7_small/AND2x2_ASAP7_75t_R]]
puts {[get_lib_pins -of_objects [get_lib_cells *]]}
report_object_full_names [get_lib_pins -of_objects [get_lib_cells *]]
