create_clock -name m1_clk -period 1000 {clk1 clk2 clk3}
set_input_delay -clock m1_clk 100 {in1 in2}
