add_test(
  NAME tcl.sdc.constraints
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_constraints
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.constraints PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.clocks
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_clocks
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.clocks PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exceptions
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exceptions
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exceptions PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.environment
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_environment
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.environment PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.write_read
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_write_read
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.write_read PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.write_comprehensive
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_write_comprehensive
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.write_comprehensive PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.variables
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_variables
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.variables PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.advanced PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.write_options
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_write_options
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.write_options PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.genclk_advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_genclk_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.genclk_advanced PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_advanced PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.port_delay_advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_port_delay_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.port_delay_advanced PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.clock_groups_sense
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_clock_groups_sense
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.clock_groups_sense PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.disable_case
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_disable_case
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.disable_case PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.removal_reset
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_removal_reset
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.removal_reset PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.write_roundtrip
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_write_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.write_roundtrip PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.net_wire_voltage
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_net_wire_voltage
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.net_wire_voltage PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_merge_priority
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_merge_priority
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_merge_priority PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.clock_operations
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_clock_operations
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.clock_operations PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.design_rules_limits
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_design_rules_limits
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.design_rules_limits PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.drive_input_pvt
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_drive_input_pvt
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.drive_input_pvt PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.filter_query
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_filter_query
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.filter_query PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.derate_disable_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_derate_disable_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.derate_disable_deep PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_thru_net
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_thru_net
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_thru_net PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.delay_borrow_group
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_delay_borrow_group
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.delay_borrow_group PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.sense_unset_override
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_sense_unset_override
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.sense_unset_override PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_intersect
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_intersect
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_intersect PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.remove_clock_gating
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_remove_clock_gating
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.remove_clock_gating PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_override_priority
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_override_priority
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_override_priority PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.write_disabled_groups
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_write_disabled_groups
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.write_disabled_groups PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.clock_removal_cascade
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_clock_removal_cascade
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.clock_removal_cascade PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_thru_complex
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_thru_complex
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_thru_complex PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.capacitance_propagated
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_capacitance_propagated
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.capacitance_propagated PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.write_roundtrip_full
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_write_roundtrip_full
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.write_roundtrip_full PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.cycle_acct_clk_relationships
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_cycle_acct_clk_relationships
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.cycle_acct_clk_relationships PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.leaf_pin_filter_removal
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_leaf_pin_filter_removal
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.leaf_pin_filter_removal PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_rise_fall_transitions
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_rise_fall_transitions
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_rise_fall_transitions PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.cycle_acct_genclk
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_cycle_acct_genclk
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.cycle_acct_genclk PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_thru_override
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_thru_override
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_thru_override PROPERTIES LABELS "tcl;module_sdc")

add_test(
  NAME tcl.sdc.exception_match_filter
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdc_exception_match_filter
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdc.exception_match_filter PROPERTIES LABELS "tcl;module_sdc")

add_subdirectory(cpp)
