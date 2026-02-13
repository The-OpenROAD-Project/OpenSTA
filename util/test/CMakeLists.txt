add_test(
  NAME tcl.util.msg_suppress
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_msg_suppress
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.msg_suppress PROPERTIES LABELS "tcl;module_util")

add_test(
  NAME tcl.util.commands
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_commands
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.commands PROPERTIES LABELS "tcl;module_util")

add_test(
  NAME tcl.util.report_redirect
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_report_redirect
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.report_redirect PROPERTIES LABELS "tcl;module_util")

add_test(
  NAME tcl.util.pattern_string
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_pattern_string
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.pattern_string PROPERTIES LABELS "tcl;module_util")

add_test(
  NAME tcl.util.report_debug
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_report_debug
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.report_debug PROPERTIES LABELS "tcl;module_util")

add_test(
  NAME tcl.util.parallel_misc
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_parallel_misc
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.parallel_misc PROPERTIES LABELS "tcl;module_util")

add_test(
  NAME tcl.util.report_format
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_report_format
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.report_format PROPERTIES LABELS "tcl;module_util")

add_test(
  NAME tcl.util.log_redirect
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_log_redirect
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.log_redirect PROPERTIES LABELS "tcl;module_util")

add_test(
  NAME tcl.util.report_string_log
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> util_report_string_log
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.util.report_string_log PROPERTIES LABELS "tcl;module_util")

add_subdirectory(cpp)
