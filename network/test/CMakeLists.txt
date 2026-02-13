add_test(
  NAME tcl.network.query
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_query
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.query PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.traversal
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_traversal
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.traversal PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.modify
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_modify
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.modify PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.advanced
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_advanced
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.advanced PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.properties
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_properties
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.properties PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.connect_liberty
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_connect_liberty
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.connect_liberty PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.bus_parse
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_bus_parse
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.bus_parse PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.pattern_match
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_pattern_match
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.pattern_match PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.sorting
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_sorting
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.sorting PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.hierarchy
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_hierarchy
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.hierarchy PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.sdc_query
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_sdc_query
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.sdc_query PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.deep_modify
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_deep_modify
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.deep_modify PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.leaf_iter
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_leaf_iter
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.leaf_iter PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.escaped_names
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_escaped_names
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.escaped_names PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.multi_lib
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_multi_lib
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.multi_lib PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.namespace_escape
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_namespace_escape
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.namespace_escape PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.net_cap_query
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_net_cap_query
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.net_cap_query PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.fanin_fanout
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_fanin_fanout
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.fanin_fanout PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.connected_pins
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_connected_pins
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.connected_pins PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.hier_pin_query
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_hier_pin_query
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.hier_pin_query PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.cell_match_merge
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_cell_match_merge
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.cell_match_merge PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.sdc_adapt_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_sdc_adapt_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.sdc_adapt_deep PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.merge_bus_hier
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_merge_bus_hier
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.merge_bus_hier PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.sdc_pattern_deep
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_sdc_pattern_deep
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.sdc_pattern_deep PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.gcd_traversal
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_gcd_traversal
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.gcd_traversal PROPERTIES LABELS "tcl;module_network")

add_test(
  NAME tcl.network.find_cells_regex
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> network_find_cells_regex
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.network.find_cells_regex PROPERTIES LABELS "tcl;module_network")

add_subdirectory(cpp)
