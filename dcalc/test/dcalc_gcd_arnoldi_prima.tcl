# Test Arnoldi and Prima delay calculators with a larger design (GCD sky130hd)
# having many more parasitic nodes, exercising deeper Arnoldi/Prima reduction
# paths, higher-order matrix operations, and multi-driver net handling.
# Targets:
#   ArnoldiDelayCalc.cc: gateDelay, gateDelaySlew, ar1_ceff_delay,
#     ra_rdelay_1, ra_get_r, ra_get_s, ra_solve_for_s, pr_solve1, pr_solve3,
#     delay_work_set_thresholds, reportGateDelay, finishDrvrPin,
#     reduceParasitic (arnoldi reduce from larger networks)
#   PrimaDelayCalc.cc: gateDelay, inputPortDelay, reduceParasitic,
#     primaReduceRc, stampR, stampC, prima matrix solve,
#     setPrimaReduceOrder, buildNodeMap, findParasitic
#   ArnoldiReduce.cc: arnoldi reduce matrix, arnoldi basis,
#     arnoldi iteration (more iterations for larger networks)
#   ReduceParasitics.cc: reduceToPiElmore, reduceToPiPoleResidue
#   GraphDelayCalc.cc: findVertexDelay with arnoldi/prima
source ../../test/helpers.tcl

############################################################
# Read Sky130 library and GCD design
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib

read_verilog ../../examples/gcd_sky130hd.v
link_design gcd

source ../../examples/gcd_sky130hd.sdc

# Read SPEF parasitics (large: ~19k lines, many parasitic nodes)
read_spef ../../examples/gcd_sky130hd.spef

############################################################
# Baseline with default delay calculator (dmp_ceff_elmore)
############################################################
puts "--- baseline dmp_ceff_elmore ---"
report_checks -endpoint_count 3

report_checks -path_delay min -endpoint_count 3

############################################################
# Arnoldi with large GCD design
# More parasitic nodes => deeper arnoldi reduction
############################################################
puts "--- arnoldi with gcd ---"
set_delay_calculator arnoldi
report_checks -endpoint_count 3

report_checks -path_delay min -endpoint_count 3

report_checks -fields {slew cap input_pins nets fanout}

report_checks -format full_clock

# Arnoldi report_dcalc on various cells in the design
puts "--- arnoldi report_dcalc ---"
set cell_count 0
foreach cell_obj [get_cells *] {
  set cname [get_name $cell_obj]
  set pins [get_pins $cname/*]
  set in_pins {}
  set out_pins {}
  foreach p $pins {
    set dir [get_property $p direction]
    if {$dir == "input"} {
      lappend in_pins $p
    } elseif {$dir == "output"} {
      lappend out_pins $p
    }
  }
  if {[llength $in_pins] > 0 && [llength $out_pins] > 0} {
    catch {
      report_dcalc -from [lindex $in_pins 0] -to [lindex $out_pins 0] -max
    }
    incr cell_count
    if {$cell_count >= 30} break
  }
}

# Arnoldi with varying input slews
puts "--- arnoldi varying slew ---"
foreach slew_val {0.01 0.05 0.1 0.5 1.0} {
  set_input_transition $slew_val [all_inputs]
  report_checks -endpoint_count 1
  puts "arnoldi slew=$slew_val done"
}
set_input_transition 0.1 [all_inputs]

# Arnoldi with varying output loads
puts "--- arnoldi varying loads ---"
foreach load_val {0.0001 0.001 0.01 0.05} {
  set_load $load_val [get_ports resp_msg*]
  report_checks -endpoint_count 1
  puts "arnoldi load=$load_val done"
}
set_load 0 [get_ports resp_msg*]

############################################################
# Prima with GCD design and varying reduce orders
############################################################
puts "--- prima with gcd ---"
catch {set_delay_calculator prima} msg
puts "set prima: $msg"

report_checks -endpoint_count 3

report_checks -path_delay min -endpoint_count 3

report_checks -fields {slew cap input_pins nets fanout}

# Prima with varying reduce orders
puts "--- prima reduce orders ---"
foreach order {1 2 3 4 5} {
  sta::set_prima_reduce_order $order
  report_checks -endpoint_count 1
  puts "prima order=$order done"
}
# Reset to default
sta::set_prima_reduce_order 3

# Prima report_dcalc
puts "--- prima report_dcalc ---"
set cell_count 0
foreach cell_obj [get_cells *] {
  set cname [get_name $cell_obj]
  set pins [get_pins $cname/*]
  set in_pins {}
  set out_pins {}
  foreach p $pins {
    set dir [get_property $p direction]
    if {$dir == "input"} {
      lappend in_pins $p
    } elseif {$dir == "output"} {
      lappend out_pins $p
    }
  }
  if {[llength $in_pins] > 0 && [llength $out_pins] > 0} {
    catch {
      report_dcalc -from [lindex $in_pins 0] -to [lindex $out_pins 0] -max
    }
    incr cell_count
    if {$cell_count >= 30} break
  }
}

# Prima varying slew
puts "--- prima varying slew ---"
foreach slew_val {0.01 0.1 0.5 2.0} {
  set_input_transition $slew_val [all_inputs]
  report_checks -endpoint_count 1
  puts "prima slew=$slew_val done"
}
set_input_transition 0.1 [all_inputs]

############################################################
# Rapid switching between calculators
# Exercises reinit, cleanup, and cache invalidation paths
############################################################
puts "--- rapid switching ---"
set_delay_calculator dmp_ceff_elmore
report_checks -endpoint_count 1

set_delay_calculator dmp_ceff_two_pole
report_checks -endpoint_count 1

set_delay_calculator lumped_cap
report_checks -endpoint_count 1

set_delay_calculator arnoldi
report_checks -endpoint_count 1

set_delay_calculator prima
report_checks -endpoint_count 1

set_delay_calculator dmp_ceff_elmore
report_checks -endpoint_count 1

############################################################
# delay_calc_names and is_delay_calc_name
############################################################
puts "--- delay calc name queries ---"
set names [sta::delay_calc_names]
puts "delay calc names: $names"

foreach name {dmp_ceff_elmore dmp_ceff_two_pole lumped_cap arnoldi prima} {
  set result [sta::is_delay_calc_name $name]
  puts "is_delay_calc_name $name = $result"
}
set result [sta::is_delay_calc_name nonexistent_calc]
puts "is_delay_calc_name nonexistent_calc = $result"
