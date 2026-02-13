add_executable(TestParasitics TestParasitics.cc)
target_link_libraries(TestParasitics
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestParasitics PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestParasitics
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp;module_parasitics"
)
