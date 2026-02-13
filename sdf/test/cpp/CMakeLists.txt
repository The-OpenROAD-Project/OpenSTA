add_executable(TestSdf TestSdf.cc)
target_link_libraries(TestSdf
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestSdf PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestSdf
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp;module_sdf"
)
