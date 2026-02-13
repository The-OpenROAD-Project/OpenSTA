add_test(
  NAME tcl.graph.make_verify
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_make_verify
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.make_verify PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.timing_edges
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_timing_edges
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.timing_edges PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.advanced PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.incremental
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_incremental
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.incremental PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.delay_corners
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_delay_corners
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.delay_corners PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.operations
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_operations
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.operations PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.modify
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_modify
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.modify PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.bidirect
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_bidirect
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.bidirect PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.wire_inst_edges
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_wire_inst_edges
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.wire_inst_edges PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.delete_modify
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_delete_modify
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.delete_modify PROPERTIES LABELS "tcl;module_graph")

add_test(
  NAME tcl.graph.vertex_edge_ops
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> graph_vertex_edge_ops
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.graph.vertex_edge_ops PROPERTIES LABELS "tcl;module_graph")

add_subdirectory(cpp)
