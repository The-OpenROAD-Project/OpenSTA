add_executable(TestSdc TestSdc.cc)
target_link_libraries(TestSdc
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestSdc PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestSdc
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp;module_sdc"
)
