add_executable(TestSearch TestSearch.cc)
target_link_libraries(TestSearch
  OpenSTA
  GTest::gtest
  GTest::gtest_main
  ${TCL_LIBRARY}
)
target_include_directories(TestSearch PRIVATE
  ${STA_HOME}/include/sta
  ${STA_HOME}
  ${CMAKE_BINARY_DIR}/include/sta
)
gtest_discover_tests(TestSearch
  WORKING_DIRECTORY ${STA_HOME}
  PROPERTIES LABELS "cpp;module_search"
)
