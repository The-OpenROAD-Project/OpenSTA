add_executable(TestLiberty TestLiberty.cc)
target_link_libraries(TestLiberty
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestLiberty PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestLiberty
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp;module_liberty"
)
