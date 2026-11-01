# verilog wor declarations error
read_liberty asap7_small.lib.gz
catch {read_verilog verilog_wor.v} msg
puts $msg

