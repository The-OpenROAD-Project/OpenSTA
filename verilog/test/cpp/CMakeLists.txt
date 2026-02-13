add_executable(TestVerilog TestVerilog.cc)
target_link_libraries(TestVerilog
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestVerilog PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestVerilog
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp;module_verilog"
)
