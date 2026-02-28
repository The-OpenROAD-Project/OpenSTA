add_executable(TestFindRoot TestFindRoot.cc)
target_link_libraries(TestFindRoot
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestFindRoot PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestFindRoot
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp\;module_dcalc"
)
