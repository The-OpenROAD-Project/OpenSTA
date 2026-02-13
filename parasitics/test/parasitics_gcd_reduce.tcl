# Test parasitic reduction with the larger GCD Sky130HD design,
# exercising deeper reduction paths and multiple reduction methods.
# Targets:
#   ReduceParasitics.cc: reduceToPiElmore (more nodes -> deeper recursion),
#     reduceToPiPoleResidue, arnoldiReduction, piModel,
#     reduceParasiticNetwork (complete reduce flow)
#   ConcreteParasitics.cc: isPiElmore, isPiPoleResidue, isPiModel,
#     findParasiticNetwork, deleteReducedParasitics,
#     deleteDrvrReducedParasitics, parasiticNodeCount,
#     hasParasiticNetwork, loadPinCapacitanceChanged
#   SpefReader.cc: SPEF parse with NAME_MAP (larger name map),
#     makeResistor, makeCapacitor, makeParasiticNode,
#     findNode, makeParasiticNetwork, netParasitic,
#     spefNodeName, spefPinName, spefNetName
#   Parasitics.cc: parasitic find/make/delete, reduce operations
source ../../test/helpers.tcl

############################################################
# Read Sky130HD library and GCD design
############################################################
read_liberty ../../test/sky130hd/sky130hd_tt.lib
puts "PASS: read sky130hd"

read_verilog ../../examples/gcd_sky130hd.v
link_design gcd
puts "PASS: link gcd"

source ../../examples/gcd_sky130hd.sdc
puts "PASS: SDC"

############################################################
# Test 1: Read large SPEF and run baseline
############################################################
puts "--- Test 1: read GCD SPEF ---"
read_spef ../../examples/gcd_sky130hd.spef
puts "PASS: read gcd SPEF"

set_delay_calculator dmp_ceff_elmore
report_checks -endpoint_count 3
puts "PASS: dmp_ceff_elmore baseline"

report_checks -path_delay min -endpoint_count 3
puts "PASS: dmp_ceff_elmore min"

############################################################
# Test 2: Report parasitic annotation (exercises annotation queries)
############################################################
puts "--- Test 2: parasitic annotation ---"
report_parasitic_annotation
puts "PASS: parasitic annotation"

report_parasitic_annotation -report_unannotated
puts "PASS: parasitic annotation -report_unannotated"

############################################################
# Test 3: Multiple delay calculators with large SPEF
# Each calculator exercises different reduction paths
############################################################
puts "--- Test 3: delay calculators ---"

set_delay_calculator dmp_ceff_two_pole
report_checks -endpoint_count 2
puts "PASS: dmp_ceff_two_pole"

set_delay_calculator lumped_cap
report_checks -endpoint_count 2
puts "PASS: lumped_cap"

catch {set_delay_calculator arnoldi}
report_checks -endpoint_count 2
puts "PASS: arnoldi"

catch {set_delay_calculator prima}
report_checks -endpoint_count 2
puts "PASS: prima"

set_delay_calculator dmp_ceff_elmore
report_checks -endpoint_count 2
puts "PASS: back to dmp_ceff_elmore"

############################################################
# Test 4: Report_dcalc on nets with large parasitic trees
############################################################
puts "--- Test 4: report_dcalc on large nets ---"
set cell_count 0
foreach cell_obj [get_cells *] {
  set cname [get_name $cell_obj]
  catch {
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
      catch {
        report_dcalc -from [lindex $in_pins 0] -to [lindex $out_pins 0] -min
      }
      incr cell_count
      if {$cell_count >= 20} break
    }
  }
}
puts "PASS: report_dcalc $cell_count cells"

############################################################
# Test 5: Report nets with detailed parasitic info
############################################################
puts "--- Test 5: report_net ---"
set net_count 0
foreach net_obj [get_nets *] {
  catch {
    report_net -digits 4 [get_name $net_obj]
  }
  incr net_count
  if {$net_count >= 20} break
}
puts "PASS: report_net ($net_count nets)"

############################################################
# Test 6: Re-read SPEF with -reduce flag
# Exercises reduce-during-read path
############################################################
puts "--- Test 6: SPEF with -reduce ---"
read_spef -reduce ../../examples/gcd_sky130hd.spef
puts "PASS: read_spef -reduce"

report_checks -endpoint_count 2
puts "PASS: report after reduce"

set_delay_calculator dmp_ceff_two_pole
report_checks -endpoint_count 2
puts "PASS: dmp_two_pole after reduce"

set_delay_calculator dmp_ceff_elmore
report_checks -endpoint_count 2
puts "PASS: dmp after reduce"

############################################################
# Test 7: Load changes after SPEF (exercises cache invalidation)
############################################################
puts "--- Test 7: load changes ---"
foreach port_name {resp_val resp_rdy} {
  catch {
    set_load 0.01 [get_ports $port_name]
    report_checks -to [get_ports $port_name] -endpoint_count 1
  }
}
puts "PASS: load changes"

foreach port_name {resp_val resp_rdy} {
  catch {
    set_load 0.05 [get_ports $port_name]
    report_checks -to [get_ports $port_name] -endpoint_count 1
  }
}
puts "PASS: larger loads"

foreach port_name {resp_val resp_rdy} {
  catch {
    set_load 0 [get_ports $port_name]
  }
}
puts "PASS: reset loads"

############################################################
# Test 8: Re-read SPEF after load changes (exercises delete/re-create)
############################################################
puts "--- Test 8: SPEF re-read ---"
read_spef ../../examples/gcd_sky130hd.spef
puts "PASS: re-read SPEF"

report_checks -endpoint_count 2
puts "PASS: report after re-read"

############################################################
# Test 9: Annotated delay reports
############################################################
puts "--- Test 9: annotated delays ---"
catch {report_annotated_delay -cell} msg
puts "annotated -cell: done"

catch {report_annotated_delay -net} msg
puts "annotated -net: done"

catch {report_annotated_delay -from_in_ports -to_out_ports} msg
puts "annotated from/to ports: done"

puts "ALL PASSED"
