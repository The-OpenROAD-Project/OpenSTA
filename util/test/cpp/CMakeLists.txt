add_executable(TestUtil TestUtil.cc)
target_link_libraries(TestUtil
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestUtil PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestUtil
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp;module_util"
)
