# Test Property.cc - get_property on various object types
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Force timing update so arrival/slew/slack are computed
report_checks -path_delay max > /dev/null

puts "--- Pin properties ---"
set pin [get_pins reg1/D]
puts "pin name: [get_property $pin name]"
puts "pin full_name: [get_property $pin full_name]"
puts "pin direction: [get_property $pin direction]"
puts "pin is_hierarchical: [get_property $pin is_hierarchical]"
puts "pin is_port: [get_property $pin is_port]"
puts "pin lib_pin_name: [get_property $pin lib_pin_name]"
puts "pin pin_direction: [get_property $pin pin_direction]"

puts "--- Pin clock properties ---"
set ck_pin [get_pins reg1/CK]
puts "ck is_clock: [get_property $ck_pin is_clock]"
puts "ck is_register_clock: [get_property $ck_pin is_register_clock]"
set ck_clocks [get_property $ck_pin clocks]
puts "ck clocks count: [llength $ck_clocks]"
set ck_domains [get_property $ck_pin clock_domains]
puts "ck clock_domains count: [llength $ck_domains]"

puts "--- Pin timing properties ---"
puts "pin arrival_max_rise: [get_property $pin arrival_max_rise]"
puts "pin arrival_max_fall: [get_property $pin arrival_max_fall]"
puts "pin arrival_min_rise: [get_property $pin arrival_min_rise]"
puts "pin arrival_min_fall: [get_property $pin arrival_min_fall]"
puts "pin slack_max: [get_property $pin slack_max]"
puts "pin slack_max_rise: [get_property $pin slack_max_rise]"
puts "pin slack_max_fall: [get_property $pin slack_max_fall]"
puts "pin slack_min: [get_property $pin slack_min]"
puts "pin slack_min_rise: [get_property $pin slack_min_rise]"
puts "pin slack_min_fall: [get_property $pin slack_min_fall]"
puts "pin slew_max: [get_property $pin slew_max]"
puts "pin slew_max_rise: [get_property $pin slew_max_rise]"
puts "pin slew_max_fall: [get_property $pin slew_max_fall]"
puts "pin slew_min: [get_property $pin slew_min]"
puts "pin slew_min_rise: [get_property $pin slew_min_rise]"
puts "pin slew_min_fall: [get_property $pin slew_min_fall]"
puts "pin activity: [get_property $pin activity]"

puts "--- Port properties ---"
set port [get_ports in1]
puts "port name: [get_property $port name]"
puts "port full_name: [get_property $port full_name]"
puts "port direction: [get_property $port direction]"

puts "--- Instance properties ---"
set inst [get_cells reg1]
puts "inst name: [get_property $inst name]"
puts "inst full_name: [get_property $inst full_name]"
puts "inst ref_name: [get_property $inst ref_name]"
set inst_cell [get_property $inst cell]
puts "inst cell: [get_name $inst_cell]"

puts "--- Net properties ---"
set net [get_nets n1]
puts "net name: [get_property $net name]"
puts "net full_name: [get_property $net full_name]"

puts "--- Clock properties ---"
set clk_obj [get_clocks clk]
puts "clock name: [get_property $clk_obj name]"
puts "clock full_name: [get_property $clk_obj full_name]"
puts "clock period: [get_property $clk_obj period]"
set clk_sources [get_property $clk_obj sources]
puts "clock sources count: [llength $clk_sources]"
puts "clock is_generated: [get_property $clk_obj is_generated]"
puts "clock is_virtual: [get_property $clk_obj is_virtual]"

puts "--- LibertyCell properties ---"
set lib_cell [get_lib_cells NangateOpenCellLibrary/DFF_X1]
puts "lib_cell name: [get_property $lib_cell name]"
puts "lib_cell full_name: [get_property $lib_cell full_name]"
puts "lib_cell base_name: [get_property $lib_cell base_name]"
puts "lib_cell filename: [get_property $lib_cell filename]"
set lib_ref [get_property $lib_cell library]
puts "lib_cell library: [get_name $lib_ref]"
puts "lib_cell is_buffer: [get_property $lib_cell is_buffer]"

puts "--- LibertyPort properties ---"
set lib_port [get_lib_pins NangateOpenCellLibrary/DFF_X1/D]
puts "lib_port name: [get_property $lib_port name]"
puts "lib_port full_name: [get_property $lib_port full_name]"
puts "lib_port direction: [get_property $lib_port direction]"

puts "--- Library properties ---"
set lib [get_libs NangateOpenCellLibrary]
puts "lib name: [get_property $lib name]"
puts "lib full_name: [get_property $lib full_name]"

puts "--- Edge properties ---"
set edges [get_timing_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]]
foreach edge $edges {
  puts "edge full_name: [get_property $edge full_name]"
  puts "edge delay_min_fall: [get_property $edge delay_min_fall]"
  puts "edge delay_max_fall: [get_property $edge delay_max_fall]"
  puts "edge delay_min_rise: [get_property $edge delay_min_rise]"
  puts "edge delay_max_rise: [get_property $edge delay_max_rise]"
  puts "edge sense: [get_property $edge sense]"
  set epin [get_property $edge from_pin]
  puts "edge from_pin: [get_full_name $epin]"
  break
}

puts "--- PathEnd properties ---"
set paths [find_timing_paths -path_delay max -endpoint_path_count 5]
foreach path_end $paths {
  set sp [get_property $path_end startpoint]
  puts "pathend startpoint: [get_full_name $sp]"
  set ep [get_property $path_end endpoint]
  puts "pathend endpoint: [get_full_name $ep]"
  catch {
    set sc [get_property $path_end startpoint_clock]
    puts "pathend startpoint_clock: [get_name $sc]"
  }
  catch {
    set ec [get_property $path_end endpoint_clock]
    puts "pathend endpoint_clock: [get_name $ec]"
  }
  if { [$path_end is_check] } {
    catch {
      set ecp [get_property $path_end endpoint_clock_pin]
      puts "pathend endpoint_clock_pin: [get_full_name $ecp]"
    }
  }
  puts "pathend slack: [get_property $path_end slack]"
  break
}

puts "--- Path properties ---"
set paths [find_timing_paths -path_delay max]
foreach path_end $paths {
  set p [$path_end path]
  set ppin [get_property $p pin]
  puts "path pin: [get_full_name $ppin]"
  puts "path arrival: [get_property $p arrival]"
  puts "path required: [get_property $p required]"
  puts "path slack: [get_property $p slack]"
  break
}

puts "--- Edge additional properties ---"
set edges [get_timing_edges -from [get_pins and1/A1] -to [get_pins and1/ZN]]
foreach edge $edges {
  set etp [get_property $edge to_pin]
  puts "edge to_pin: [get_full_name $etp]"
  break
}

puts "--- get_property with -object_type ---"
puts "by name cell: [get_property -object_type instance reg1 name]"
puts "by name pin: [get_property -object_type pin reg1/D name]"
puts "by name net: [get_property -object_type net n1 name]"
puts "by name port: [get_property -object_type port in1 name]"
puts "by name clock: [get_property -object_type clock clk name]"
puts "by name lib_cell: [get_property -object_type liberty_cell NangateOpenCellLibrary/DFF_X1 name]"
puts "by name lib_pin: [get_property -object_type liberty_port NangateOpenCellLibrary/DFF_X1/D name]"
puts "by name library: [get_property -object_type library NangateOpenCellLibrary name]"

puts "ALL PASSED"
