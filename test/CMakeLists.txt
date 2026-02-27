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

# Auto-detect public tests: register all *.ok files that have a matching *.tcl
file(GLOB ok_files "${TEST_DIR}/*.ok")
foreach(ok_file ${ok_files})
  get_filename_component(test_name ${ok_file} NAME_WE)
  if(EXISTS "${TEST_DIR}/${test_name}.tcl")
    sta_tcl_test(${test_name})
  endif()
endforeach()

# Auto-detect example tests: register all *.tcl files in examples/
file(GLOB example_tcl_files "${EXAMPLES_DIR}/*.tcl")
foreach(tcl_file ${example_tcl_files})
  get_filename_component(test_name ${tcl_file} NAME_WE)
  sta_example_test(${test_name})
endforeach()
