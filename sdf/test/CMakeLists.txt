add_test(
  NAME tcl.sdf.read_write
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_read_write
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.read_write PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.annotation
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_annotation
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.annotation PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.advanced PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.edge_write
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_edge_write
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.edge_write PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.reread_cond
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_reread_cond
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.reread_cond PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.write_interconnect
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_write_interconnect
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.write_interconnect PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.check_annotation
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_check_annotation
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.check_annotation PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.timing_checks
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_timing_checks
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.timing_checks PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.device_cond
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_device_cond
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.device_cond PROPERTIES LABELS "tcl;module_sdf")

add_test(
  NAME tcl.sdf.cond_pathpulse
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> sdf_cond_pathpulse
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.sdf.cond_pathpulse PROPERTIES LABELS "tcl;module_sdf")

add_subdirectory(cpp)
