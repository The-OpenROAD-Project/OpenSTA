# Register existing upstream Tcl tests for ctest coverage
set(STA_EXE $<TARGET_FILE:sta>)
set(TEST_DIR ${STA_HOME}/test)
set(EXAMPLES_DIR ${STA_HOME}/examples)

function(sta_tcl_test test_name)
  add_test(
    NAME tcl.${test_name}
    COMMAND ${STA_EXE} -no_init -no_splash -exit ${TEST_DIR}/${test_name}.tcl
    WORKING_DIRECTORY ${TEST_DIR}
  )
  set_tests_properties(tcl.${test_name} PROPERTIES LABELS "tcl")
endfunction()

function(sta_example_test test_name)
  add_test(
    NAME tcl.example.${test_name}
    COMMAND ${STA_EXE} -no_init -no_splash -exit ${EXAMPLES_DIR}/${test_name}.tcl
    WORKING_DIRECTORY ${EXAMPLES_DIR}
  )
  set_tests_properties(tcl.example.${test_name} PROPERTIES LABELS "tcl;example")
endfunction()

# Public regression tests
sta_tcl_test(disconnect_mcp_pin)
sta_tcl_test(get_filter)
sta_tcl_test(get_is_buffer)
sta_tcl_test(get_is_memory)
sta_tcl_test(get_lib_pins_of_objects)
sta_tcl_test(get_noargs)
sta_tcl_test(get_objrefs)
sta_tcl_test(liberty_arcs_one2one_1)
sta_tcl_test(liberty_arcs_one2one_2)
sta_tcl_test(liberty_backslash_eol)
sta_tcl_test(liberty_ccsn)
sta_tcl_test(liberty_float_as_str)
sta_tcl_test(liberty_latch3)
sta_tcl_test(package_require)
sta_tcl_test(path_group_names)
sta_tcl_test(power_json)
sta_tcl_test(prima3)
sta_tcl_test(report_checks_sorted)
sta_tcl_test(report_checks_src_attr)
sta_tcl_test(report_json1)
sta_tcl_test(report_json2)
sta_tcl_test(suppress_msg)
sta_tcl_test(verilog_attribute)
sta_tcl_test(verilog_specify)

# Example tests
sta_example_test(delay_calc)
sta_example_test(min_max_delays)
sta_example_test(multi_corner)
sta_example_test(power)
sta_example_test(power_vcd)
sta_example_test(sdf_delays)
sta_example_test(spef_parasitics)
