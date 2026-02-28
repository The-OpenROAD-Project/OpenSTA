add_executable(TestSpice TestSpice.cc)
target_link_libraries(TestSpice
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestSpice PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${STA_HOME}/spice
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestSpice
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp\;module_spice"
)
