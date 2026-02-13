add_test(
  NAME tcl.verilog.roundtrip
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_roundtrip
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.roundtrip PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.write_options
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_write_options
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.write_options PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.read_asap7
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_read_asap7
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.read_asap7 PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.attributes
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_attributes
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.attributes PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.specify
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_specify
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.specify PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.writer_advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_writer_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.writer_advanced PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.bus
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_bus
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.bus PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.assign
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_assign
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.assign PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.complex_bus
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_complex_bus
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.complex_bus PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.write_types
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_write_types
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.write_types PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.hier_write
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_hier_write
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.hier_write PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.supply_tristate
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_supply_tristate
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.supply_tristate PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.const_concat
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_const_concat
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.const_concat PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.escaped_write
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_escaped_write
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.escaped_write PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.remove_cells
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_remove_cells
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.remove_cells PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.error_paths
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_error_paths
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.error_paths PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.preproc_param
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_preproc_param
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.preproc_param PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.gcd_writer
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_gcd_writer
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.gcd_writer PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.bus_partselect
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_bus_partselect
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.bus_partselect PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.gcd_large
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_gcd_large
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.gcd_large PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.multimodule_write
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_multimodule_write
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.multimodule_write PROPERTIES LABELS "tcl;module_verilog")

add_test(
  NAME tcl.verilog.coverage
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> verilog_coverage
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.verilog.coverage PROPERTIES LABELS "tcl;module_verilog")

add_subdirectory(cpp)
