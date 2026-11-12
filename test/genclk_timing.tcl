read_liberty genclk_timing.lib
read_liberty asap7_seq.lib.gz

read_verilog genclk_timing.v
link_design generated_clock_timing
create_clock -name clk -period 10 [get_ports CLK_IN] 

report_clock_properties

# Set input delay relative to the base clock
set_input_delay -clock clk 0.5 [get_ports data_in]

# Set output delay relative to the generated clock
set_output_delay -clock clk_gen/CLK_OUT 0.5 [get_ports out]

# Launch (clk)  ->  Capture (gen)  ->  Slack
# -----------------------------------------
# 0ns           ->  3ns            ->  3.0 - 0.5 = 2.5ns
# 10ns          ->  20ns           ->  20 - 10.5 = 9.5ns
# 20ns          ->  37ns           ->  37 - 20.5 = 16.5ns
# 30ns          ->  37ns           ->  37 - 30.5 = 6.5ns
# 40ns          ->  54ns           ->  54 - 40.5 = 13.5ns
# 50ns          ->  54ns           ->  54 - 50.5 = 3.5ns
# 60ns          ->  71ns           ->  71 - 60.5 = 10.5ns
# 70ns          ->  71ns           ->  71 - 70.5 = 0.5ns, reported as worst condition
puts ""
report_checks
