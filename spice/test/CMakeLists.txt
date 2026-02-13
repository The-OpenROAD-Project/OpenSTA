add_test(
  NAME tcl.spice.write
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_write
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.write PROPERTIES LABELS "tcl;module_spice")

add_test(
  NAME tcl.spice.write_options
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_write_options
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.write_options PROPERTIES LABELS "tcl;module_spice")

add_test(
  NAME tcl.spice.gate_advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_gate_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.gate_advanced PROPERTIES LABELS "tcl;module_spice")

add_test(
  NAME tcl.spice.path_min
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_path_min
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.path_min PROPERTIES LABELS "tcl;module_spice")

add_test(
  NAME tcl.spice.gate_cells
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_gate_cells
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.gate_cells PROPERTIES LABELS "tcl;module_spice")

add_test(
  NAME tcl.spice.multipath
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_multipath
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.multipath PROPERTIES LABELS "tcl;module_spice")

add_test(
  NAME tcl.spice.subckt_file
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_subckt_file
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.subckt_file PROPERTIES LABELS "tcl;module_spice")

add_test(
  NAME tcl.spice.gcd_path
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_gcd_path
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.gcd_path PROPERTIES LABELS "tcl;module_spice")

add_test(
  NAME tcl.spice.gcd_gate
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> spice_gcd_gate
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.spice.gcd_gate PROPERTIES LABELS "tcl;module_spice")

add_subdirectory(cpp)
