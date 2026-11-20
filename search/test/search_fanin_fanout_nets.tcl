# get_fanin/get_fanout with nets that connect to more than one pin.
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_fanin_fanout_nets.v
link_design search_fanin_fanout_nets

proc print_names { objs } {
  set names {}
  foreach obj $objs {
    lappend names [get_full_name $obj]
  }
  return [lsort $names]
}

puts "--- get_fanout -from net n1 ---"
puts [print_names [get_fanout -from [get_nets n1]]]

puts "--- get_fanout -from net n1 -flat -endpoints_only ---"
puts [print_names [get_fanout -from [get_nets n1] -flat -endpoints_only]]

puts "--- get_fanout -from nets {n1 n3} -flat -endpoints_only ---"
puts [print_names [get_fanout -from [get_nets {n1 n3}] -flat -endpoints_only]]

puts "--- get_fanout -from net n1 -only_cells ---"
puts [print_names [get_fanout -from [get_nets n1] -only_cells]]

puts "--- get_fanin -to net n1 ---"
puts [print_names [get_fanin -to [get_nets n1]]]

puts "--- get_fanin -to nets {n2 n3} -flat -startpoints_only ---"
puts [print_names [get_fanin -to [get_nets {n2 n3}] -flat -startpoints_only]]

puts "--- get_fanout without -from ---"
catch {get_fanout -flat} msg
puts $msg
