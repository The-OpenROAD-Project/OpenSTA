# jpeg_noise1.tcl sdc
create_clock -name clk -period 200

set_input_transition 10 in1
set_input_transition 50 in2
set_input_transition 100 in3
set_input_transition 30 {in6 in7 in8 in9 in9 in10}

# victim
set_input_delay -clock clk -min   0 in1
set_input_delay -clock clk -max 100 in1
set_input_delay -clock clk -min   0 in2
set_input_delay -clock clk -max 100 in2
set_input_delay -clock clk -min   0 in3
set_input_delay -clock clk -max 100 in3
# a1 aggressor
set_input_delay -clock clk -min 10 in6
set_input_delay -clock clk -max 20 in6
set_input_delay -clock clk -min 10 in7
set_input_delay -clock clk -max 20 in7
set_input_delay -clock clk -min 10 in8
set_input_delay -clock clk -max 20 in8
# a2 aggressor
set_input_delay -clock clk -min 10 in9
set_input_delay -clock clk -max 20 in9
set_input_delay -clock clk -min 10 in10
set_input_delay -clock clk -max 20 in10
