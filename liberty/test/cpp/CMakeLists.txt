add_executable(TestLibertyClasses TestLibertyClasses.cc)
target_link_libraries(TestLibertyClasses
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestLibertyClasses PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestLibertyClasses
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp\;module_liberty"
)
