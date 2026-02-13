add_executable(TestGraph TestGraph.cc)
target_link_libraries(TestGraph
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestGraph PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestGraph
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp;module_graph"
)
