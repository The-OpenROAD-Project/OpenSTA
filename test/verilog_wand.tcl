# wand declarations error
read_liberty asap7_small.lib.gz
catch {read_verilog verilog_wand.v} msg
puts $msg
