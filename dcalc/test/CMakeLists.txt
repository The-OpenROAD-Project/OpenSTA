add_test(
  NAME tcl.dcalc.report
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_report
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.report PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.engines
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_engines
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.engines PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.corners
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_corners
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.corners PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.spef
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_spef
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.spef PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.advanced PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.prima
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_prima
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.prima PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.ccs_incremental
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_ccs_incremental
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.ccs_incremental PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.ccs_parasitics
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_ccs_parasitics
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.ccs_parasitics PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.graph_delay
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_graph_delay
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.graph_delay PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.dmp_ceff
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_dmp_ceff
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.dmp_ceff PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.arnoldi_prima
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_arnoldi_prima
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.arnoldi_prima PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.annotated_incremental
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_annotated_incremental
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.annotated_incremental PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.dmp_edge_cases
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_dmp_edge_cases
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.dmp_edge_cases PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.incremental_tolerance
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_incremental_tolerance
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.incremental_tolerance PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.multi_engine_spef
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_multi_engine_spef
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.multi_engine_spef PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.annotate_slew
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_annotate_slew
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.annotate_slew PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.arnoldi_spef
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_arnoldi_spef
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.arnoldi_spef PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.dmp_convergence
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_dmp_convergence
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.dmp_convergence PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.prima_arnoldi_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_prima_arnoldi_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.prima_arnoldi_deep PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.dmp_pi_model_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_dmp_pi_model_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.dmp_pi_model_deep PROPERTIES LABELS "tcl;module_dcalc")

add_test(
  NAME tcl.dcalc.gcd_arnoldi_prima
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> dcalc_gcd_arnoldi_prima
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.dcalc.gcd_arnoldi_prima PROPERTIES LABELS "tcl;module_dcalc")

add_subdirectory(cpp)
