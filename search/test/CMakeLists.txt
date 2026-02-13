add_test(
  NAME tcl.search.timing
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_timing
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.timing PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_formats
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_formats
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_formats PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.analysis
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_analysis
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.analysis PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.sdc_advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_sdc_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.sdc_advanced PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.fanin_fanout
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_fanin_fanout
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.fanin_fanout PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.genclk
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_genclk
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.genclk PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.property
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_property
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.property PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.latch
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_latch
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.latch PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.crpr
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_crpr
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.crpr PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.register
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_register
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.register PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.check_timing
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_check_timing
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.check_timing PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_path_detail
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_path_detail
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_path_detail PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.timing_model
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_timing_model
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.timing_model PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.sta_cmds
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_sta_cmds
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.sta_cmds PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.min_period_max_skew
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_min_period_max_skew
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.min_period_max_skew PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.corner_skew
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_corner_skew
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.corner_skew PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.path_end_types
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_path_end_types
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.path_end_types PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.levelize_sim
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_levelize_sim
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.levelize_sim PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.multiclock
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_multiclock
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.multiclock PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.gated_clk
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_gated_clk
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.gated_clk PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.property_extra
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_property_extra
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.property_extra PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.sta_extra
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_sta_extra
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.sta_extra PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.json_unconstrained
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_json_unconstrained
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.json_unconstrained PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.port_pin_properties
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_port_pin_properties
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.port_pin_properties PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.min_period_short
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_min_period_short
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.min_period_short PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.worst_slack_sta
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_worst_slack_sta
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.worst_slack_sta PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.crpr_data_checks
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_crpr_data_checks
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.crpr_data_checks PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.multicorner_analysis
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_multicorner_analysis
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.multicorner_analysis PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.write_sdf_model
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_write_sdf_model
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.write_sdf_model PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.power_activity
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_power_activity
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.power_activity PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.path_enum_groups
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_path_enum_groups
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.path_enum_groups PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.latch_timing
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_latch_timing
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.latch_timing PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_path_expanded
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_path_expanded
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_path_expanded PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.limit_violations
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_limit_violations
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.limit_violations PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.data_check_gated
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_data_check_gated
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.data_check_gated PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.fanin_fanout_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_fanin_fanout_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.fanin_fanout_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.network_edit_replace
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_network_edit_replace
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.network_edit_replace PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.annotated_write_verilog
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_annotated_write_verilog
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.annotated_write_verilog PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_path_pvt_cap
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_path_pvt_cap
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_path_pvt_cap PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.sim_logic_clk_network
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_sim_logic_clk_network
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.sim_logic_clk_network PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.exception_paths
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_exception_paths
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.exception_paths PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.timing_model_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_timing_model_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.timing_model_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.property_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_property_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.property_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.sim_const_prop
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_sim_const_prop
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.sim_const_prop PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.register_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_register_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.register_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.network_sta_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_network_sta_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.network_sta_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.genclk_latch_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_genclk_latch_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.genclk_latch_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.limits_verbose
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_limits_verbose
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.limits_verbose PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.pvt_analysis
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_pvt_analysis
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.pvt_analysis PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.spef_parasitics
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_spef_parasitics
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.spef_parasitics PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.assigned_delays
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_assigned_delays
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.assigned_delays PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_json_formats
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_json_formats
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_json_formats PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.property_libport_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_property_libport_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.property_libport_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.check_types_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_check_types_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.check_types_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.property_inst_cell
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_property_inst_cell
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.property_inst_cell PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_path_latch_expanded
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_path_latch_expanded
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_path_latch_expanded PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.network_edit_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_network_edit_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.network_edit_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_gated_datacheck
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_gated_datacheck
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_gated_datacheck PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.search_arrival_required
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_search_arrival_required
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.search_arrival_required PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_fields_formats
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_fields_formats
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_fields_formats PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.sta_bidirect_extcap
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_sta_bidirect_extcap
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.sta_bidirect_extcap PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.path_delay_output
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_path_delay_output
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.path_delay_output PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.genclk_property_report
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_genclk_property_report
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.genclk_property_report PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.path_enum_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_path_enum_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.path_enum_deep PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.timing_model_readback
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_timing_model_readback
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.timing_model_readback PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.clk_skew_multiclock
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_clk_skew_multiclock
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.clk_skew_multiclock PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.register_latch_sim
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_register_latch_sim
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.register_latch_sim PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.timing_model_clktree
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_timing_model_clktree
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.timing_model_clktree PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.path_enum_nworst
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_path_enum_nworst
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.path_enum_nworst PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.clk_skew_interclk
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_clk_skew_interclk
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.clk_skew_interclk PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.register_filter_combos
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_register_filter_combos
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.register_filter_combos PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.levelize_loop_disabled
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_levelize_loop_disabled
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.levelize_loop_disabled PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.tag_path_analysis
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_tag_path_analysis
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.tag_path_analysis PROPERTIES LABELS "tcl;module_search")

add_test(
  NAME tcl.search.report_path_types
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_report_path_types
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.report_path_types PROPERTIES LABELS "tcl;module_search")

add_subdirectory(cpp)
