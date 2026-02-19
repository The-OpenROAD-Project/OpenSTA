add_test(
  NAME tcl.liberty.read_sky130
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_read_sky130
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.read_sky130 PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.read_nangate
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_read_nangate
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.read_nangate PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.multi_corner
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_multi_corner
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.multi_corner PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.read_asap7
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_read_asap7
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.read_asap7 PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.read_ihp
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_read_ihp
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.read_ihp PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.equiv_cells
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_equiv_cells
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.equiv_cells PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.properties
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_properties
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.properties PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.writer
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_writer
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.writer PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.power
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_power
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.power PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.wireload
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_wireload
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.wireload PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.timing_models
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_timing_models
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.timing_models PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.func_expr
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_func_expr
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.func_expr PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.multi_lib_equiv
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_multi_lib_equiv
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.multi_lib_equiv PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.write_roundtrip
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_write_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.write_roundtrip PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.opcond_scale
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_opcond_scale
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.opcond_scale PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.ccsn
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_ccsn
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.ccsn PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.cell_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_cell_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.cell_deep PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.sky130_corners
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_sky130_corners
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.sky130_corners PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.arc_model_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_arc_model_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.arc_model_deep PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.writer_roundtrip2
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_writer_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.writer_roundtrip2 PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.equiv_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_equiv_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.equiv_deep PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.seq_scan_bus
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_seq_scan_bus
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.seq_scan_bus PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.clkgate_lvlshift
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_clkgate_lvlshift
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.clkgate_lvlshift PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.scan_signal_types
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_scan_signal_types
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.scan_signal_types PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.leakage_power_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_leakage_power_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.leakage_power_deep PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.timing_types_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_timing_types_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.timing_types_deep PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.pgpin_voltage
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_pgpin_voltage
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.pgpin_voltage PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.equiv_cross_lib
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_equiv_cross_lib
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.equiv_cross_lib PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.busport_mem_iter
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_busport_mem_iter
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.busport_mem_iter PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.equiv_map_libs
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_equiv_map_libs
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.equiv_map_libs PROPERTIES LABELS "tcl;module_liberty")

add_test(
  NAME tcl.liberty.cell_classify_pgpin
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> liberty_cell_classify_pgpin
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.liberty.cell_classify_pgpin PROPERTIES LABELS "tcl;module_liberty")

add_subdirectory(cpp)
