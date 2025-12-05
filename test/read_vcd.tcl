read_liberty asap7_small.lib.gz

# Read verilog file and link design
read_verilog read_vcd.v
link_design dut

# Create clock
set_units -time ps
set clock_period 1000
create_clock -name clk -period $clock_period

# Read VCD file
sta::set_debug read_vcd 1
read_vcd -scope dut read_vcd.vcd
