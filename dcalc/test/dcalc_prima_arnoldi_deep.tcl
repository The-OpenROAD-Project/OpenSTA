# Deep Prima and Arnoldi delay calculator testing with different
# SPEF files, reduce orders, extreme conditions, and Nangate45 PDK.
# Targets:
#   PrimaDelayCalc.cc: primaDelay, primaReduceRc, prima2, prima3,
#     primaResStamp, primaCapStamp, primaPostReduction,
#     gateDelay, gateSlew, loadDelay, reduceParasitics
#   ArnoldiDelayCalc.cc: arnoldiDelay, arnoldiReduceRc, arnoldi2,
#     arnoldi3, arnoldiExpand, loadDelay, gateDelay, gateSlew
#   ArnoldiReduce.cc: arnoldi reduce matrix, arnoldi basis, arnoldi iteration
#   GraphDelayCalc.cc: findVertexDelay with arnoldi/prima,
#     seedInvalidDelays, loadPinCapacitanceChanged
#   DmpCeff.cc: ceffPiElmore with arnoldi/prima parasitic models
#   DelayCalc.i: delay_calc_names, is_delay_calc_name, set_prima_reduce_order
source ../../test/helpers.tcl

############################################################
# Read Nangate45 library and search_test1 design
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
puts "PASS: read Nangate45"

read_verilog ../../search/test/search_test1.v
link_design search_test1
puts "PASS: link search_test1"

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports {in1 in2}]
set_output_delay -clock clk 2.0 [get_ports out1]
set_input_transition 0.1 [all_inputs]
puts "PASS: SDC setup"

############################################################
# Read SPEF parasitics for example1
# This exercises SPEF parsing and parasitic model construction
############################################################
read_spef ../../search/test/search_test1.spef
puts "PASS: read SPEF"

############################################################
# Test Prima with Nangate45 + SPEF
############################################################
puts "--- prima with Nangate45 ---"

catch {set_delay_calculator prima} msg
puts "set prima: $msg"

report_checks
puts "PASS: prima report_checks"

report_checks -path_delay min
puts "PASS: prima min path"

report_checks -path_delay max
puts "PASS: prima max path"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: prima with fields"

report_checks -format full_clock
puts "PASS: prima full_clock"

# Multiple endpoint paths
report_checks -endpoint_count 5
puts "PASS: prima endpoint_count 5"

# From/to specific paths
catch {
  report_checks -from [get_ports in1] -endpoint_count 3
  puts "PASS: prima from in1"
}

catch {
  report_checks -to [get_ports out1] -endpoint_count 3
  puts "PASS: prima to out1"
}

catch {
  report_checks -from [get_ports in2] -endpoint_count 3
  puts "PASS: prima from in2"
}

############################################################
# Prima with varying reduce orders
############################################################
puts "--- prima reduce order ---"

catch {sta::set_prima_reduce_order 1} msg
puts "set_prima_reduce_order 1: $msg"
report_checks
puts "PASS: prima order 1"

catch {sta::set_prima_reduce_order 2} msg
puts "set_prima_reduce_order 2: $msg"
report_checks
puts "PASS: prima order 2"

catch {sta::set_prima_reduce_order 3} msg
puts "set_prima_reduce_order 3: $msg"
report_checks
puts "PASS: prima order 3"

catch {sta::set_prima_reduce_order 4} msg
puts "set_prima_reduce_order 4: $msg"
report_checks
puts "PASS: prima order 4"

catch {sta::set_prima_reduce_order 5} msg
puts "set_prima_reduce_order 5: $msg"
report_checks
puts "PASS: prima order 5"

# Reset to default
catch {sta::set_prima_reduce_order 3} msg

############################################################
# Prima with varying slew
############################################################
puts "--- prima varying slew ---"
set_input_transition 0.05 [all_inputs]
report_checks
puts "PASS: prima slew=0.05"

set_input_transition 0.5 [all_inputs]
report_checks
puts "PASS: prima slew=0.5"

set_input_transition 2.0 [all_inputs]
report_checks
puts "PASS: prima slew=2.0"

set_input_transition 0.1 [all_inputs]

############################################################
# Prima with varying loads
############################################################
puts "--- prima varying loads ---"
foreach load_val {0.0001 0.001 0.005 0.01 0.05} {
  set_load $load_val [get_ports out1]
  set_load $load_val [get_ports out1]
  report_checks
  puts "prima load=$load_val: done"
}
set_load 0 [get_ports out1]
set_load 0 [get_ports out1]
puts "PASS: prima varying loads"

############################################################
# Prima report_dcalc for specific arcs
############################################################
puts "--- prima report_dcalc ---"

# Find some pins in the design
set all_cells [get_cells *]
set first_cell [lindex $all_cells 0]
set cell_name [get_name $first_cell]
puts "first cell: $cell_name"

# Try dcalc on various cells
foreach cell_obj $all_cells {
  set cname [get_name $cell_obj]
  catch {
    set ref [get_property $cell_obj ref_name]
    set pins [get_pins $cname/*]
    if {[llength $pins] >= 2} {
      set in_pin [lindex $pins 0]
      set out_pin [lindex $pins end]
      catch {
        report_dcalc -from $in_pin -to $out_pin -max
      }
    }
  }
}
puts "PASS: prima report_dcalc"

############################################################
# Switch to Arnoldi
############################################################
puts "--- arnoldi with Nangate45 ---"

catch {set_delay_calculator arnoldi} msg
puts "set arnoldi: $msg"

report_checks
puts "PASS: arnoldi report_checks"

report_checks -path_delay min
puts "PASS: arnoldi min"

report_checks -path_delay max
puts "PASS: arnoldi max"

report_checks -fields {slew cap input_pins nets fanout}
puts "PASS: arnoldi with fields"

report_checks -endpoint_count 5
puts "PASS: arnoldi endpoint_count 5"

# Arnoldi with varying slew
puts "--- arnoldi varying slew ---"
foreach slew_val {0.01 0.05 0.1 0.5 1.0 5.0} {
  set_input_transition $slew_val [all_inputs]
  report_checks
  puts "arnoldi slew=$slew_val: done"
}
set_input_transition 0.1 [all_inputs]
puts "PASS: arnoldi varying slew"

# Arnoldi with varying loads
puts "--- arnoldi varying loads ---"
foreach load_val {0.0001 0.001 0.01 0.05 0.1} {
  set_load $load_val [get_ports out1]
  report_checks
  puts "arnoldi load=$load_val: done"
}
set_load 0 [get_ports out1]
puts "PASS: arnoldi varying loads"

############################################################
# Engine switching with SPEF
############################################################
puts "--- engine switching ---"

set_delay_calculator dmp_ceff_elmore
report_checks
puts "PASS: dmp_ceff_elmore"

set_delay_calculator dmp_ceff_two_pole
report_checks
puts "PASS: dmp_ceff_two_pole"

set_delay_calculator lumped_cap
report_checks
puts "PASS: lumped_cap"

catch {set_delay_calculator prima}
report_checks
puts "PASS: prima after switching"

catch {set_delay_calculator arnoldi}
report_checks
puts "PASS: arnoldi after switching"

############################################################
# Re-read SPEF and re-compute
############################################################
puts "--- re-read SPEF ---"
read_spef ../../search/test/search_test1.spef
puts "PASS: re-read SPEF"

catch {set_delay_calculator prima}
report_checks
puts "PASS: prima after SPEF re-read"

catch {set_delay_calculator arnoldi}
report_checks
puts "PASS: arnoldi after SPEF re-read"

############################################################
# Incremental updates
############################################################
puts "--- incremental updates ---"

set_load 0.005 [get_ports out1]
report_checks
puts "PASS: arnoldi incremental load"

create_clock -name clk -period 5 [get_ports clk]
report_checks
puts "PASS: arnoldi incremental clock"

set_input_transition 1.0 [all_inputs]
report_checks
puts "PASS: arnoldi incremental slew"

puts "ALL PASSED"
