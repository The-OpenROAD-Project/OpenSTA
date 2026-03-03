add_executable(TestSearchClasses TestSearchClasses.cc)
target_link_libraries(TestSearchClasses
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestSearchClasses PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestSearchClasses
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp\;module_search"
)
