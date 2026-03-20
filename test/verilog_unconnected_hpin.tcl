read_liberty asap7_small.lib.gz
read_verilog verilog_unconnected_hpin.v
link_design top 
puts "Find b1/out2: [get_full_name [get_pins b1/out2]]"
puts "Find b2/out2: [get_full_name [get_pins b2/out2]]"
# Check if net is connected to "b2/u3/Y" that was the b2/out2 in parent block
set iterm [sta::find_pin "b2/u3/Y"]
set net [get_net -of_object [get_pin $iterm]]
report_net [get_full_name $net]
