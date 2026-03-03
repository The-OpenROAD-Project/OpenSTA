add_executable(TestSdcClasses TestSdcClasses.cc)
target_link_libraries(TestSdcClasses
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestSdcClasses PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestSdcClasses
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp\;module_sdc"
)
