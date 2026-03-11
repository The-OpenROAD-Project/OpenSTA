# Test Review: All 13 Comment Violations

## Summary

Checked all `*/test/*.tcl` and `*/test/cpp/*.cc` files against all 13 review comments.

- Comment 1: Stale line numbers/coverage percentages, useless assertions, empty if-bodies
- Comment 2: Tcl tests that should be C++ (dcalc DONE; others checked)
- Comment 3: Unnecessary catch blocks
- Comment 4/11: Meaningless `puts "PASS: ..."` prints
- Comment 5: Missing incremental timing tests (project-wide)
- Comment 6: liberty_wireload.tcl needs report_checks per set_wire_load_model
- Comment 7: ECSM test loads NLDM only
- Comment 8: Load-only tests with no real verification
- Comment 9: Orphan .ok files
- Comment 10: C++ tests too short
- Comment 12: regression -R verilog_specify from test/
- Comment 13: search/test/CMakeLists.txt messiness

---

## graph/test/

DONE - graph/test/graph_incremental.tcl
  L2-5: Stale coverage percentages in comments (Comment 1)
DONE - graph/test/graph_delay_corners.tcl
  L2-6: Stale coverage percentages in comments (Comment 1)
DONE - graph/test/graph_advanced.tcl
DONE - graph/test/graph_bidirect.tcl
DONE - graph/test/graph_delete_modify.tcl
DONE - graph/test/graph_make_verify.tcl
DONE - graph/test/graph_modify.tcl
DONE - graph/test/graph_operations.tcl
DONE - graph/test/graph_timing_edges.tcl
DONE - graph/test/graph_vertex_edge_ops.tcl
DONE - graph/test/graph_wire_inst_edges.tcl
DONE - graph/test/cpp/TestGraph.cc

## liberty/test/

DONE - liberty/test/liberty_read_sky130.tcl
  L1-30: Existence-only test with no data verification (Comment 8)
DONE - liberty/test/liberty_read_nangate.tcl
  L26,49,71,222-238: Variables assigned but never inspected beyond existence (Comment 8)
DONE - liberty/test/liberty_read_ihp.tcl
  L27,123,182-186,200: Variables assigned but never inspected beyond existence (Comment 8)
DONE - liberty/test/liberty_read_asap7.tcl
  L44,62,76,90,104: 11 of 12 loaded libraries have zero verification beyond existence check (Comment 8)
DONE - liberty/test/liberty_arc_model_deep.tcl
DONE - liberty/test/liberty_busport_mem_iter.tcl
DONE - liberty/test/liberty_ccsn.tcl
DONE - liberty/test/liberty_cell_classify_pgpin.tcl
DONE - liberty/test/liberty_cell_deep.tcl
DONE - liberty/test/liberty_clkgate_lvlshift.tcl
DONE - liberty/test/liberty_ecsm.tcl
DONE - liberty/test/liberty_equiv_cells.tcl
DONE - liberty/test/liberty_equiv_cross_lib.tcl
DONE - liberty/test/liberty_equiv_deep.tcl
DONE - liberty/test/liberty_equiv_map_libs.tcl
DONE - liberty/test/liberty_func_expr.tcl
DONE - liberty/test/liberty_leakage_power_deep.tcl
DONE - liberty/test/liberty_multi_corner.tcl
DONE - liberty/test/liberty_multi_lib_equiv.tcl
DONE - liberty/test/liberty_opcond_scale.tcl
DONE - liberty/test/liberty_pgpin_voltage.tcl
DONE - liberty/test/liberty_power.tcl
DONE - liberty/test/liberty_properties.tcl
DONE - liberty/test/liberty_read_asap7.tcl
DONE - liberty/test/liberty_scan_signal_types.tcl
DONE - liberty/test/liberty_seq_scan_bus.tcl
DONE - liberty/test/liberty_sky130_corners.tcl
DONE - liberty/test/liberty_timing_models.tcl
DONE - liberty/test/liberty_timing_types_deep.tcl
DONE - liberty/test/liberty_wireload.tcl
DONE - liberty/test/liberty_write_roundtrip.tcl
DONE - liberty/test/liberty_writer.tcl
DONE - liberty/test/liberty_writer_roundtrip.tcl
DONE - liberty/test/cpp/TestLiberty.cc

## network/test/

DONE - network/test/network_net_cap_query.tcl
  L237-238: Empty if-body for found_lib != NULL (Comment 1)
  L242-243: Empty if-body for inv_cell != NULL (Comment 1)
DONE - network/test/network_advanced.tcl
DONE - network/test/network_bus_parse.tcl
DONE - network/test/network_cell_match_merge.tcl
DONE - network/test/network_connect_liberty.tcl
DONE - network/test/network_connected_pins.tcl
DONE - network/test/network_deep_modify.tcl
DONE - network/test/network_escaped_names.tcl
DONE - network/test/network_fanin_fanout.tcl
DONE - network/test/network_find_cells_regex.tcl
DONE - network/test/network_gcd_traversal.tcl
DONE - network/test/network_hier_pin_query.tcl
DONE - network/test/network_hierarchy.tcl
DONE - network/test/network_leaf_iter.tcl
DONE - network/test/network_merge_bus_hier.tcl
DONE - network/test/network_modify.tcl
DONE - network/test/network_multi_lib.tcl
DONE - network/test/network_namespace_escape.tcl
DONE - network/test/network_pattern_match.tcl
DONE - network/test/network_properties.tcl
DONE - network/test/network_query.tcl
DONE - network/test/network_sdc_adapt_deep.tcl
DONE - network/test/network_sdc_pattern_deep.tcl
DONE - network/test/network_sdc_query.tcl
DONE - network/test/network_sorting.tcl
DONE - network/test/network_traversal.tcl
DONE - network/test/cpp/TestNetwork.cc

## parasitics/test/

DONE - parasitics/test/parasitics_spef.tcl
  L1-18: Minimal test - loads libs, reads SPEF, runs single report_checks with no parasitic value queries (Comment 8)
DONE - parasitics/test/parasitics_annotation_query.tcl
DONE - parasitics/test/parasitics_corners.tcl
DONE - parasitics/test/parasitics_coupling.tcl
DONE - parasitics/test/parasitics_coupling_reduce.tcl
DONE - parasitics/test/parasitics_delete_network.tcl
DONE - parasitics/test/parasitics_detailed.tcl
DONE - parasitics/test/parasitics_estimate_wirerc.tcl
DONE - parasitics/test/parasitics_gcd_reduce.tcl
DONE - parasitics/test/parasitics_gcd_spef.tcl
DONE - parasitics/test/parasitics_manual.tcl
DONE - parasitics/test/parasitics_pi_pole_residue.tcl
DONE - parasitics/test/parasitics_reduce.tcl
DONE - parasitics/test/parasitics_reduce_dcalc.tcl
DONE - parasitics/test/parasitics_spef_formats.tcl
DONE - parasitics/test/parasitics_spef_namemap.tcl
DONE - parasitics/test/parasitics_wireload.tcl
DONE - parasitics/test/cpp/TestParasitics.cc

## power/test/

DONE - power/test/power_report.tcl
  L1-10: Minimal test - loads lib, creates clock, runs report_power with no activity or options (Comment 8)
DONE - power/test/power_detailed.tcl
DONE - power/test/power_propagate.tcl
DONE - power/test/power_report_options.tcl
DONE - power/test/power_saif.tcl
DONE - power/test/power_saif_vcd.tcl
DONE - power/test/power_vcd_detailed.tcl
DONE - power/test/cpp/TestPower.cc

## sdc/test/

DONE - sdc/test/cpp/TestSdc.cc
  L475: EXPECT_TRUE(true) useless "compilation test" assertion (Comment 1/8)
  L568-574: ClkNameLess/ClockNameLess: EXPECT_TRUE(true) useless assertions (Comment 1/8)
  L2986-2987: CycleAcctingFunctorsCompile: EXPECT_TRUE(true) useless assertion (Comment 1/8)
  L3236,3238: ClkEdgeCmpLess: (void) cast on results instead of assertions (Comment 1)
  L3285: ExceptionPathLessComparator: (void) cast instead of assertion (Comment 1)
  L3322: ClockIndexLessComparator: (void) cast instead of assertion (Comment 1)
  L3359-3375: DeratingFactorsIsOneValue*: (void) casts on is_one/value (Comment 1)
  L3428-3429: DeratingFactorsCellIsOneValue: (void) casts (Comment 1)
  L3501,3506: CycleAcctingHashEqualLess: (void) casts (Comment 1)
  L3647,3664: PinPairLess/HashConstruct: ASSERT_NO_THROW with no real assertion (Comment 1/8)
  L3981: ClockDefaultPin: (void) cast instead of asserting nullptr (Comment 1)
DONE - sdc/test/sdc_advanced.tcl
DONE - sdc/test/sdc_capacitance_propagated.tcl
DONE - sdc/test/sdc_clock_groups_sense.tcl
DONE - sdc/test/sdc_clock_operations.tcl
DONE - sdc/test/sdc_clock_removal_cascade.tcl
DONE - sdc/test/sdc_clocks.tcl
DONE - sdc/test/sdc_constraints.tcl
DONE - sdc/test/sdc_cycle_acct_clk_relationships.tcl
DONE - sdc/test/sdc_cycle_acct_genclk.tcl
DONE - sdc/test/sdc_delay_borrow_group.tcl
DONE - sdc/test/sdc_derate_disable_deep.tcl
DONE - sdc/test/sdc_design_rules_limits.tcl
DONE - sdc/test/sdc_disable_case.tcl
DONE - sdc/test/sdc_drive_input_pvt.tcl
DONE - sdc/test/sdc_environment.tcl
DONE - sdc/test/sdc_exception_advanced.tcl
DONE - sdc/test/sdc_exception_intersect.tcl
DONE - sdc/test/sdc_exception_match_filter.tcl
DONE - sdc/test/sdc_exception_merge_priority.tcl
DONE - sdc/test/sdc_exception_override_priority.tcl
DONE - sdc/test/sdc_exception_rise_fall_transitions.tcl
DONE - sdc/test/sdc_exception_thru_complex.tcl
DONE - sdc/test/sdc_exception_thru_net.tcl
DONE - sdc/test/sdc_exception_thru_override.tcl
DONE - sdc/test/sdc_exceptions.tcl
DONE - sdc/test/sdc_filter_query.tcl
DONE - sdc/test/sdc_genclk_advanced.tcl
DONE - sdc/test/sdc_leaf_pin_filter_removal.tcl
DONE - sdc/test/sdc_net_wire_voltage.tcl
DONE - sdc/test/sdc_port_delay_advanced.tcl
DONE - sdc/test/sdc_removal_reset.tcl
DONE - sdc/test/sdc_remove_clock_gating.tcl
DONE - sdc/test/sdc_sense_unset_override.tcl
DONE - sdc/test/sdc_variables.tcl
DONE - sdc/test/sdc_write_comprehensive.tcl
DONE - sdc/test/sdc_write_disabled_groups.tcl
DONE - sdc/test/sdc_write_options.tcl
DONE - sdc/test/sdc_write_read.tcl
DONE - sdc/test/sdc_write_roundtrip.tcl
DONE - sdc/test/sdc_write_roundtrip_full.tcl

## sdf/test/

DONE - sdf/test/sdf_advanced.tcl
DONE - sdf/test/sdf_annotation.tcl
DONE - sdf/test/sdf_check_annotation.tcl
DONE - sdf/test/sdf_cond_pathpulse.tcl
DONE - sdf/test/sdf_device_cond.tcl
DONE - sdf/test/sdf_edge_write.tcl
DONE - sdf/test/sdf_read_write.tcl
DONE - sdf/test/sdf_reread_cond.tcl
DONE - sdf/test/sdf_timing_checks.tcl
DONE - sdf/test/sdf_write_interconnect.tcl
DONE - sdf/test/cpp/TestSdf.cc

## search/test/

DONE - search/test/search_port_pin_properties.tcl
  L106-109: Unnecessary catch block for get_property nonexistent (Comment 3)
  L216-219: Unnecessary catch block for get_property nonexistent (Comment 3)
  L220-224: Unnecessary catch block for get_property nonexistent (Comment 3)
DONE - search/test/search_property_extra.tcl
  L106-109: Unnecessary catch block for get_property nonexistent (Comment 3)
DONE - search/test/search_property_libport_deep.tcl
  L222-231: 5 unnecessary catch blocks for get_property nonexistent (Comment 3)
DONE - search/test/search_analysis.tcl
DONE - search/test/search_annotated_write_verilog.tcl
DONE - search/test/search_assigned_delays.tcl
DONE - search/test/search_check_timing.tcl
DONE - search/test/search_check_types_deep.tcl
DONE - search/test/search_clk_skew_interclk.tcl
DONE - search/test/search_clk_skew_multiclock.tcl
DONE - search/test/search_corner_skew.tcl
DONE - search/test/search_crpr.tcl
DONE - search/test/search_crpr_data_checks.tcl
DONE - search/test/search_data_check_gated.tcl
DONE - search/test/search_exception_paths.tcl
DONE - search/test/search_fanin_fanout.tcl
DONE - search/test/search_fanin_fanout_deep.tcl
DONE - search/test/search_gated_clk.tcl
DONE - search/test/search_genclk.tcl
DONE - search/test/search_genclk_latch_deep.tcl
DONE - search/test/search_genclk_property_report.tcl
DONE - search/test/search_json_unconstrained.tcl
DONE - search/test/search_latch.tcl
DONE - search/test/search_latch_timing.tcl
DONE - search/test/search_levelize_loop_disabled.tcl
DONE - search/test/search_levelize_sim.tcl
DONE - search/test/search_limit_violations.tcl
DONE - search/test/search_limits_verbose.tcl
DONE - search/test/search_min_period_max_skew.tcl
DONE - search/test/search_min_period_short.tcl
DONE - search/test/search_multiclock.tcl
DONE - search/test/search_multicorner_analysis.tcl
DONE - search/test/search_network_edit_deep.tcl
DONE - search/test/search_network_edit_replace.tcl
DONE - search/test/search_network_sta_deep.tcl
DONE - search/test/search_path_delay_output.tcl
DONE - search/test/search_path_end_types.tcl
DONE - search/test/search_path_enum_deep.tcl
DONE - search/test/search_path_enum_groups.tcl
DONE - search/test/search_path_enum_nworst.tcl
DONE - search/test/search_power_activity.tcl
DONE - search/test/search_property.tcl
DONE - search/test/search_property_deep.tcl
DONE - search/test/search_property_inst_cell.tcl
DONE - search/test/search_pvt_analysis.tcl
DONE - search/test/search_register.tcl
DONE - search/test/search_register_deep.tcl
DONE - search/test/search_register_filter_combos.tcl
DONE - search/test/search_register_latch_sim.tcl
DONE - search/test/search_report_fields_formats.tcl
DONE - search/test/search_report_formats.tcl
DONE - search/test/search_report_gated_datacheck.tcl
DONE - search/test/search_report_json_formats.tcl
DONE - search/test/search_report_path_detail.tcl
DONE - search/test/search_report_path_expanded.tcl
DONE - search/test/search_report_path_latch_expanded.tcl
DONE - search/test/search_report_path_pvt_cap.tcl
DONE - search/test/search_report_path_types.tcl
DONE - search/test/search_sdc_advanced.tcl
DONE - search/test/search_search_arrival_required.tcl
DONE - search/test/search_sim_const_prop.tcl
DONE - search/test/search_sim_logic_clk_network.tcl
DONE - search/test/search_spef_parasitics.tcl
DONE - search/test/search_sta_bidirect_extcap.tcl
DONE - search/test/search_sta_cmds.tcl
DONE - search/test/search_sta_extra.tcl
DONE - search/test/search_tag_path_analysis.tcl
DONE - search/test/search_timing.tcl
DONE - search/test/search_timing_model.tcl
DONE - search/test/search_timing_model_clktree.tcl
DONE - search/test/search_timing_model_deep.tcl
DONE - search/test/search_timing_model_readback.tcl
DONE - search/test/search_worst_slack_sta.tcl
DONE - search/test/search_write_sdf_model.tcl
DONE - search/test/cpp/TestSearch.cc
DONE - search/test/cpp/TestSearchIncremental.cc
DONE - search/test/CMakeLists.txt

## spice/test/

DONE - spice/test/spice_gate_cells.tcl
  L1-22: All spice-specific tests removed; now only loads lib and runs report_checks (Comment 8)
DONE - spice/test/spice_gate_advanced.tcl
DONE - spice/test/spice_gcd_gate.tcl
DONE - spice/test/spice_gcd_path.tcl
DONE - spice/test/spice_multipath.tcl
DONE - spice/test/spice_path_min.tcl
DONE - spice/test/spice_subckt_file.tcl
DONE - spice/test/spice_write.tcl
DONE - spice/test/spice_write_options.tcl
DONE - spice/test/cpp/TestSpice.cc

## util/test/

DONE - util/test/util_report_redirect.tcl
  L12-43: 5 empty if-bodies (true branch empty, only else has FAIL print) (Comment 1)
  L118: catch block without justification comment (Comment 3)
DONE - util/test/util_report_debug.tcl
  L3-8: Stale coverage percentages in comments (Comment 1)
DONE - util/test/util_pattern_string.tcl
  L2-9: Stale coverage percentages in comments (Comment 1)
DONE - util/test/util_msg_suppress.tcl
  L1-12: Calls suppress_msg/unsuppress_msg with no verification (Comment 8)
DONE - util/test/util_commands.tcl
DONE - util/test/util_log_redirect.tcl
DONE - util/test/util_parallel_misc.tcl
DONE - util/test/util_report_format.tcl
DONE - util/test/util_report_string_log.tcl
DONE - util/test/cpp/TestUtil.cc

## verilog/test/

DONE - verilog/test/verilog_attributes.tcl
  L2-5: Stale coverage percentages in comments (Comment 1)
DONE - verilog/test/verilog_escaped_write.ok
  Orphan .ok file: no corresponding verilog_escaped_write.tcl (Comment 9)
DONE - verilog/test/verilog_remove_cells.ok
  Orphan .ok file: no corresponding verilog_remove_cells.tcl (Comment 9)
DONE - verilog/test/verilog_writer_advanced.ok
  Orphan .ok file: no corresponding verilog_writer_advanced.tcl (Comment 9)
DONE - verilog/test/cpp/TestVerilog.cc
  L1828-1835: EmptyNames: EXPECT_TRUE(true) useless assertion (Comment 1)
  L2054-2070: WriteReadVerilogRoundTrip: claims roundtrip but never re-reads; SUCCEED() useless (Comment 1)
DONE - verilog/test/verilog_assign.tcl
DONE - verilog/test/verilog_attributes.tcl
DONE - verilog/test/verilog_bus.tcl
DONE - verilog/test/verilog_bus_partselect.tcl
DONE - verilog/test/verilog_complex_bus.tcl
DONE - verilog/test/verilog_const_concat.tcl
DONE - verilog/test/verilog_coverage.tcl
DONE - verilog/test/verilog_error_paths.tcl
DONE - verilog/test/verilog_escaped_write_bus.tcl
DONE - verilog/test/verilog_escaped_write_complex.tcl
DONE - verilog/test/verilog_escaped_write_const.tcl
DONE - verilog/test/verilog_escaped_write_hier.tcl
DONE - verilog/test/verilog_escaped_write_supply.tcl
DONE - verilog/test/verilog_gcd_large.tcl
DONE - verilog/test/verilog_gcd_writer.tcl
DONE - verilog/test/verilog_hier_write.tcl
DONE - verilog/test/verilog_multimodule_write.tcl
DONE - verilog/test/verilog_preproc_param.tcl
DONE - verilog/test/verilog_read_asap7.tcl
DONE - verilog/test/verilog_remove_cells_basic.tcl
DONE - verilog/test/verilog_remove_cells_complex.tcl
DONE - verilog/test/verilog_remove_cells_hier.tcl
DONE - verilog/test/verilog_remove_cells_multigate.tcl
DONE - verilog/test/verilog_remove_cells_reread.tcl
DONE - verilog/test/verilog_remove_cells_supply.tcl
DONE - verilog/test/verilog_roundtrip.tcl
DONE - verilog/test/verilog_specify.tcl
DONE - verilog/test/verilog_supply_tristate.tcl
DONE - verilog/test/verilog_write_asap7.tcl
DONE - verilog/test/verilog_write_assign_types.tcl
DONE - verilog/test/verilog_write_bus_types.tcl
DONE - verilog/test/verilog_write_complex_bus_types.tcl
DONE - verilog/test/verilog_write_nangate.tcl
DONE - verilog/test/verilog_write_options.tcl
DONE - verilog/test/verilog_write_sky130.tcl
DONE - verilog/test/verilog_writer_asap7.tcl
DONE - verilog/test/verilog_writer_modify.tcl
DONE - verilog/test/verilog_writer_nangate.tcl
DONE - verilog/test/verilog_writer_sky130.tcl
DONE - verilog/test/cpp/TestVerilog.cc

## test/ (top-level)

DONE - test/liberty_backslash_eol.tcl
  L1-2: Only reads liberty file, .ok is empty - no verification (Comment 8)
DONE - test/liberty_ccsn.tcl
  L1-2: Only reads CCSN liberty file, .ok is empty - no verification (Comment 8)
DONE - test/liberty_latch3.tcl
  L1-2: Only reads liberty file, .ok is empty - no verification (Comment 8)
DONE - test/package_require.tcl
  L1-3: Only runs package require, .ok is empty - no verification (Comment 8)
DONE - test/verilog_specify.tcl
  L1-2: Only reads verilog file, .ok is empty - more thorough version exists at verilog/test/verilog_specify.tcl (Comment 8)
DONE - test/disconnect_mcp_pin.tcl
DONE - test/get_filter.tcl
DONE - test/get_is_buffer.tcl
DONE - test/get_is_memory.tcl
DONE - test/get_lib_pins_of_objects.tcl
DONE - test/get_noargs.tcl
DONE - test/get_objrefs.tcl
DONE - test/liberty_arcs_one2one_1.tcl
DONE - test/liberty_arcs_one2one_2.tcl
DONE - test/liberty_float_as_str.tcl
DONE - test/path_group_names.tcl
DONE - test/power_json.tcl
DONE - test/prima3.tcl
DONE - test/report_checks_sorted.tcl
DONE - test/report_checks_src_attr.tcl
DONE - test/report_json1.tcl
DONE - test/report_json2.tcl
DONE - test/suppress_msg.tcl
DONE - test/verilog_attribute.tcl

## Comment 12 status

`./regression -R verilog_specify` from test/ directory WORKS.
The script converts underscores to dots and runs `ctest -L "tcl" -R "verilog.specify"`,
which matches `tcl.verilog_specify`. Comment 12 is resolved.

## dcalc/test/

DONE - dcalc/test/cpp/TestDcalc.cc
DONE - dcalc/test/cpp/TestFindRoot.cc
