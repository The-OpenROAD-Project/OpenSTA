add_executable(TestNetwork TestNetwork.cc)
target_link_libraries(TestNetwork
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestNetwork PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestNetwork
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp\;module_network"
)
