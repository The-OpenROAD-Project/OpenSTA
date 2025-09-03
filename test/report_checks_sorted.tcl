# report_checks with sorted path ends
read_liberty asap7_small.lib.gz
read_verilog report_checks_sorted.v
link_design top

create_clock -name clk -period 500 {clk}
set_input_delay -clock clk 0 {in}

group_path -name custom -to {r3}
group_path -name long -to {r4}

report_checks -group_path_count 1 -sort_by_slack
