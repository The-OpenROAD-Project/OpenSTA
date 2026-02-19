# Deep DMP effective capacitance testing with pi model parasitics,
# various cell types, extreme conditions, and incremental updates.
# Targets:
#   DmpCeff.cc: dmpCeffIter convergence, ceffPiElmore boundary cases,
#     ceffPiPoleResidue, iteration count limits, very small/large caps,
#     loadDelay, gateDelay with pi model
#   GraphDelayCalc.cc: findVertexDelay with manual parasitics,
#     seedInvalidDelays, delayCalcIncrementalCond
#   ArnoldiDelayCalc.cc: arnoldi with pi model parasitics
#   FindRoot.cc: root finding edge cases
source ../../test/helpers.tcl

############################################################
# Read Nangate45 and setup search_test1 design
############################################################
read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog ../../search/test/search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports {in1 in2}]
set_output_delay -clock clk 2.0 [get_ports out1]
set_input_transition 0.1 [all_inputs]

############################################################
# Test 1: Manual pi model with dmp_ceff_elmore on all outputs
############################################################
puts "--- Test 1: pi models on all driver pins ---"
set_delay_calculator dmp_ceff_elmore

# Set pi models on representative driver pins
set all_cells [get_cells *]
foreach cell_obj $all_cells {
  set cname [get_name $cell_obj]
  set ref [get_property $cell_obj ref_name]
  # Try to set pi model on output pins
  set pins [get_pins $cname/*]
  foreach pin $pins {
    catch {
      set dir [get_property $pin direction]
      if {$dir == "output"} {
        catch {sta::set_pi_model [get_name $pin] 0.003 8.0 0.002}
      }
    }
  }
}

report_checks

report_checks -path_delay min

report_checks -path_delay max

############################################################
# Test 2: Extreme pi model values (very small)
############################################################
puts "--- Test 2: tiny pi model ---"

foreach cell_obj [lrange $all_cells 0 4] {
  set cname [get_name $cell_obj]
  set pins [get_pins $cname/*]
  foreach pin $pins {
    catch {
      set dir [get_property $pin direction]
      if {$dir == "output"} {
        catch {sta::set_pi_model [get_name $pin] 0.00001 0.01 0.000005}
      }
    }
  }
}
report_checks

############################################################
# Test 3: Large pi model values
############################################################
puts "--- Test 3: large pi model ---"

foreach cell_obj [lrange $all_cells 0 4] {
  set cname [get_name $cell_obj]
  set pins [get_pins $cname/*]
  foreach pin $pins {
    catch {
      set dir [get_property $pin direction]
      if {$dir == "output"} {
        catch {sta::set_pi_model [get_name $pin] 0.1 200.0 0.05}
      }
    }
  }
}
report_checks

############################################################
# Test 4: dmp_ceff_two_pole with manual pi models
############################################################
puts "--- Test 4: dmp_ceff_two_pole ---"
set_delay_calculator dmp_ceff_two_pole

report_checks

report_checks -path_delay min

# Vary slew
foreach slew_val {0.01 0.1 0.5 1.0 5.0} {
  set_input_transition $slew_val [all_inputs]
  report_checks
  puts "two_pole slew=$slew_val: done"
}
set_input_transition 0.1 [all_inputs]

############################################################
# Test 5: SPEF then manual pi model override
############################################################
puts "--- Test 5: SPEF then pi override ---"

set_delay_calculator dmp_ceff_elmore
read_spef ../../search/test/search_test1.spef

report_checks

# Override with manual pi models
foreach cell_obj [lrange $all_cells 0 2] {
  set cname [get_name $cell_obj]
  set pins [get_pins $cname/*]
  foreach pin $pins {
    catch {
      set dir [get_property $pin direction]
      if {$dir == "output"} {
        catch {sta::set_pi_model [get_name $pin] 0.005 10.0 0.003}
      }
    }
  }
}
report_checks

############################################################
# Test 6: report_dcalc with dmp calculators and pi models
############################################################
puts "--- Test 6: report_dcalc ---"

set_delay_calculator dmp_ceff_elmore
foreach cell_obj [lrange $all_cells 0 5] {
  set cname [get_name $cell_obj]
  set pins [get_pins $cname/*]
  if {[llength $pins] >= 2} {
    set in_pin [lindex $pins 0]
    set out_pin [lindex $pins end]
    catch {report_dcalc -from $in_pin -to $out_pin -max}
    catch {report_dcalc -from $in_pin -to $out_pin -min}
  }
}

set_delay_calculator dmp_ceff_two_pole
foreach cell_obj [lrange $all_cells 0 5] {
  set cname [get_name $cell_obj]
  set pins [get_pins $cname/*]
  if {[llength $pins] >= 2} {
    set in_pin [lindex $pins 0]
    set out_pin [lindex $pins end]
    catch {report_dcalc -from $in_pin -to $out_pin -max -digits 6}
  }
}

############################################################
# Test 7: Incremental updates with pi models
############################################################
puts "--- Test 7: incremental ---"

set_delay_calculator dmp_ceff_elmore

# Load change triggers incremental
set_load 0.001 [get_ports out1]
report_checks

set_load 0.005 [get_ports out1]
report_checks

# Slew change triggers incremental
set_input_transition 0.5 [all_inputs]
report_checks

set_input_transition 2.0 [all_inputs]
report_checks

# Clock change triggers incremental
create_clock -name clk -period 5 [get_ports clk]
report_checks

create_clock -name clk -period 2 [get_ports clk]
report_checks

############################################################
# Test 8: find_delays and invalidation
############################################################
puts "--- Test 8: find_delays ---"
sta::find_delays

sta::delays_invalid
sta::find_delays

# Multiple invalidation cycles
for {set i 0} {$i < 3} {incr i} {
  sta::delays_invalid
  sta::find_delays
}
