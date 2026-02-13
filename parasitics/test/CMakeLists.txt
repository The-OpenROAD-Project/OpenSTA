add_test(
  NAME tcl.parasitics.spef
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_spef
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.spef PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.detailed
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_detailed
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.detailed PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.corners
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_corners
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.corners PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.manual
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_manual
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.manual PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.reduce
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_reduce
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.reduce PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.spef_namemap
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_spef_namemap
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.spef_namemap PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.coupling
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_coupling
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.coupling PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.reduce_dcalc
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_reduce_dcalc
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.reduce_dcalc PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.estimate_wirerc
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_estimate_wirerc
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.estimate_wirerc PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.gcd_spef
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_gcd_spef
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.gcd_spef PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.wireload
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_wireload
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.wireload PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.annotation_query
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_annotation_query
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.annotation_query PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.delete_network
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_delete_network
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.delete_network PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.coupling_reduce
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_coupling_reduce
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.coupling_reduce PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.pi_pole_residue
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_pi_pole_residue
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.pi_pole_residue PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.spef_formats
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_spef_formats
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.spef_formats PROPERTIES LABELS "tcl;module_parasitics")

add_test(
  NAME tcl.parasitics.gcd_reduce
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> parasitics_gcd_reduce
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.parasitics.gcd_reduce PROPERTIES LABELS "tcl;module_parasitics")

add_subdirectory(cpp)
