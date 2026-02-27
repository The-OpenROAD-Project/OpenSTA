add_executable(TestPower TestPower.cc)
target_link_libraries(TestPower
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestPower PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestPower
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp\;module_power"
)
