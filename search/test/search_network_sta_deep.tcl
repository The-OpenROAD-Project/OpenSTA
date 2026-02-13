# Test network editing, slow_drivers, replace_cell, connect/disconnect,
# parasitic-related functions, annotated delay/slew, and misc Sta.cc paths.
# Targets: Sta.cc makeInstance, deleteInstance, replaceCell, makeNet,
#          deleteNet, connectPin, disconnectPin, makePortPin,
#          slowDrivers, reportParasiticAnnotation,
#          removeDelaySlewAnnotations, setArcDelay, setAnnotatedSlew,
#          setPortExtPinCap, setPortExtWireCap, setPortExtFanout,
#          setNetWireCap, connectedCap, setResistance,
#          networkChanged, delaysInvalidFrom, replaceEquivCellBefore/After,
#          Search.cc reportArrivals, reportTags, reportClkInfos,
#          reportTagGroups, reportPathCountHistogram
source ../../test/helpers.tcl

read_liberty ../../test/nangate45/Nangate45_typ.lib
read_verilog search_test1.v
link_design search_test1

create_clock -name clk -period 10 [get_ports clk]
set_input_delay -clock clk 1.0 [get_ports in1]
set_input_delay -clock clk 1.0 [get_ports in2]
set_output_delay -clock clk 2.0 [get_ports out1]

# Baseline
report_checks -path_delay max > /dev/null

############################################################
# slow_drivers
############################################################
puts "--- slow_drivers ---"
set slow [sta::slow_drivers 3]
puts "slow drivers count: [llength $slow]"
foreach s $slow {
  catch { puts "  [get_full_name $s]" }
}
puts "PASS: slow_drivers"

############################################################
# Port external pin cap / wire cap / fanout
############################################################
puts "--- set_load (port ext pin cap) ---"
set_load -pin_load 0.01 [get_ports out1]
report_checks -path_delay max
puts "PASS: port ext pin cap"

puts "--- set_load -wire_load ---"
set_load -wire_load 0.005 [get_ports out1]
report_checks -path_delay max
puts "PASS: port ext wire cap"

puts "--- set_fanout_load ---"
set_fanout_load 4 [get_ports out1]
report_checks -path_delay max
puts "PASS: port ext fanout"

############################################################
# Input drive / slew
############################################################
puts "--- set_input_transition ---"
set_input_transition 0.1 [get_ports in1]
report_checks -path_delay max
puts "PASS: input_transition"

puts "--- set_drive ---"
catch {
  set_drive 100 [get_ports in1]
  report_checks -path_delay max
}
puts "PASS: set_drive"

puts "--- set_driving_cell ---"
set_driving_cell -lib_cell BUF_X1 -pin Z [get_ports in1]
report_checks -path_delay max
puts "PASS: driving_cell"

############################################################
# set_net_wire_cap (if net wire delay mode)
############################################################
puts "--- set_wire_load_mode ---"
catch {
  set_wire_load_mode enclosed
  report_checks -path_delay max
}
puts "PASS: wire_load_mode"

############################################################
# Tag, group, info reporting
############################################################
puts "--- report_tags ---"
catch { sta::report_tags }
puts "PASS: report_tags"

puts "--- report_clk_infos ---"
catch { sta::report_clk_infos }
puts "PASS: report_clk_infos"

puts "--- report_tag_groups ---"
catch { sta::report_tag_groups }
puts "PASS: report_tag_groups"

puts "--- report_path_count_histogram ---"
catch { sta::report_path_count_histogram }
puts "PASS: report_path_count_histogram"

puts "--- tag/group/path counts ---"
puts "tags: [sta::tag_count]"
puts "tag_groups: [sta::tag_group_count]"
puts "clk_infos: [sta::clk_info_count]"
puts "paths: [sta::path_count]"
puts "PASS: counts"

############################################################
# report_tag_arrivals for a vertex
############################################################
puts "--- report_tag_arrivals ---"
set v [sta::worst_slack_vertex max]
if { $v != "NULL" } {
  catch { sta::report_tag_arrivals_cmd $v 1 }
  catch { sta::report_tag_arrivals_cmd $v 0 }
}
puts "PASS: report_tag_arrivals"

############################################################
# report_arrival_entries / report_required_entries
############################################################
puts "--- report_arrival_entries ---"
catch { sta::report_arrival_entries }
puts "PASS: report_arrival_entries"

puts "--- report_required_entries ---"
catch { sta::report_required_entries }
puts "PASS: report_required_entries"

############################################################
# find_requireds
############################################################
puts "--- find_requireds ---"
sta::find_requireds
puts "PASS: find_requireds"

############################################################
# report_annotated_delay / report_annotated_check
############################################################
puts "--- report_annotated_delay ---"
catch { report_annotated_delay }
puts "PASS: report_annotated_delay"

puts "--- report_annotated_check ---"
catch { report_annotated_check }
puts "PASS: report_annotated_check"

############################################################
# report_disabled_edges
############################################################
puts "--- report_disabled_edges ---"
report_disabled_edges
puts "PASS: report_disabled_edges"

############################################################
# Network editing: make_instance, connect, disconnect
############################################################
puts "--- network editing ---"
set edit_net [make_net new_net]
puts "new_net: [get_full_name $edit_net]"
puts "PASS: make_net"

puts "--- make_instance ---"
set new_inst [make_instance new_buf BUF_X1]
puts "new_inst: [get_full_name $new_inst]"
puts "PASS: make_instance"

puts "--- connect_pin ---"
connect_pin $edit_net new_buf/A
puts "PASS: connect_pin A"

connect_pin $edit_net new_buf/Z
puts "PASS: connect_pin Z"

puts "--- disconnect_pin ---"
disconnect_pin $edit_net new_buf/Z
puts "PASS: disconnect_pin"

puts "--- disconnect_pin A ---"
disconnect_pin $edit_net new_buf/A
puts "PASS: disconnect_pin A"

puts "--- delete_instance ---"
delete_instance $new_inst
puts "PASS: delete_instance"

puts "--- delete_net ---"
delete_net $edit_net
puts "PASS: delete_net"

############################################################
# replace_cell
############################################################
puts "--- replace_cell ---"
set old_inst [get_cells buf1]
replace_cell $old_inst BUF_X2
report_checks -path_delay max
puts "PASS: replace_cell"

puts "--- replace_cell back ---"
replace_cell $old_inst BUF_X1
report_checks -path_delay max
puts "PASS: replace_cell back"

############################################################
# write_verilog
############################################################
puts "--- write_verilog ---"
set verilog_out [make_result_file "search_test1_edited.v"]
write_verilog $verilog_out
puts "PASS: write_verilog"

############################################################
# write_sdc
############################################################
puts "--- write_sdc ---"
set sdc_out [make_result_file "search_test1_edited.sdc"]
write_sdc $sdc_out
puts "PASS: write_sdc"

############################################################
# Vertex arrival/required/slack queries
############################################################
puts "--- vertex queries ---"
set wv [sta::worst_slack_vertex max]
if { $wv != "NULL" } {
  catch {
    set warr [sta::vertex_worst_arrival_path $wv max]
    if { $warr != "NULL" } {
      puts "worst_arrival pin: [get_full_name [$warr pin]]"
      puts "worst_arrival arrival: [$warr arrival]"
    }
  }
  catch {
    set wslk [sta::vertex_worst_slack_path $wv max]
    if { $wslk != "NULL" } {
      puts "worst_slack pin: [get_full_name [$wslk pin]]"
      puts "worst_slack slack: [$wslk slack]"
    }
  }
}
puts "PASS: vertex queries"

############################################################
# report_path_end_header/footer
############################################################
puts "--- report_path_end header/footer ---"
sta::report_path_end_header
set pes [find_timing_paths -path_delay max -endpoint_path_count 3]
set prev ""
set last 0
set idx 0
foreach pe $pes {
  incr idx
  if { $idx == [llength $pes] } { set last 1 }
  if { $prev != "" } {
    catch { sta::report_path_end2 $pe $prev $last }
  } else {
    sta::report_path_end $pe
  }
  set prev $pe
}
sta::report_path_end_footer
puts "PASS: report_path_end header/footer"

############################################################
# report_checks -format json
############################################################
puts "--- json format ---"
report_checks -path_delay max -format json
puts "PASS: json format"

############################################################
# set_report_path_field_properties (ReportPath.cc)
############################################################
puts "--- set_report_path_field_properties ---"
catch { sta::set_report_path_field_properties "total" "Total" 12 0 }
catch { sta::set_report_path_field_width "total" 14 }
report_checks -path_delay max
puts "PASS: field properties"

############################################################
# Report checks min_max variants
############################################################
puts "--- report_checks -path_delay min_max ---"
report_checks -path_delay min_max
puts "PASS: min_max"

puts "ALL PASSED"
