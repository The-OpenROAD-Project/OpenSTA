add_test(
  NAME tcl.power.report
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> power_report
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.power.report PROPERTIES LABELS "tcl;module_power")

add_test(
  NAME tcl.power.detailed
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> power_detailed
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.power.detailed PROPERTIES LABELS "tcl;module_power")

add_test(
  NAME tcl.power.report_options
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> power_report_options
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.power.report_options PROPERTIES LABELS "tcl;module_power")

add_test(
  NAME tcl.power.saif
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> power_saif
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.power.saif PROPERTIES LABELS "tcl;module_power")

add_test(
  NAME tcl.power.saif_vcd
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> power_saif_vcd
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.power.saif_vcd PROPERTIES LABELS "tcl;module_power")

add_test(
  NAME tcl.power.vcd_detailed
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> power_vcd_detailed
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.power.vcd_detailed PROPERTIES LABELS "tcl;module_power")

add_test(
  NAME tcl.power.propagate
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> power_propagate
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.power.propagate PROPERTIES LABELS "tcl;module_power")

add_subdirectory(cpp)
