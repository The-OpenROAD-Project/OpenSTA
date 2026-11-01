# makeParasiticNetwork on a net created after parasitics were pre-built
# (set_wire_load_model) must not crash. Regression for upstream 47f91f28,
# fixed in 6d8f13f2.
read_liberty asap7_small.lib.gz
read_liberty gf180mcu_sram.lib.gz
read_verilog reg1_asap7.v
link_design top
create_clock -name clk -period 500 {clk1 clk2 clk3}
set_delay_calculator arnoldi
set_wire_load_model -name Estimate -library [get_name [lindex [get_libs gf180*] 0]]
report_checks -to r3/D
puts "--- insert buffer u3 on new net n_new"
make_instance u3 asap7_small/BUFx2_ASAP7_75t_R
disconnect_pin u2z r3/D
make_net n_new
connect_pin u2z u3/A
connect_pin n_new u3/Y
connect_pin n_new r3/D
report_checks -to r3/D
