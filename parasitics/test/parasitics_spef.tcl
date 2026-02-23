# Test SPEF read and parasitics reporting
# reg1_asap7 uses ASAP7 cells
read_liberty ../../test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz
read_liberty ../../test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz

read_verilog ../../test/reg1_asap7.v
link_design top

create_clock -name clk1 -period 10 [get_ports clk1]

# Read SPEF
read_spef ../../test/reg1_asap7.spef

set corner [sta::cmd_scene]
foreach net_name {r1q r2q u1z u2z} {
  set net [get_nets $net_name]
  set total_cap [$net capacitance $corner "max"]
  set pin_cap [$net pin_capacitance $corner "max"]
  set wire_cap [$net wire_capacitance $corner "max"]
  puts "$net_name total_cap=$total_cap pin_cap=$pin_cap wire_cap=$wire_cap"
  if {$total_cap <= 0.0} {
    error "expected positive capacitance on net $net_name after SPEF read"
  }
}

report_checks -fields {slew cap input_pins fanout}
