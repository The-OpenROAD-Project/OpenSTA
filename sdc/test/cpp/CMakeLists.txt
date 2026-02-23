macro(sta_cpp_test name)
  add_executable(${name} ${name}.cc)
  target_link_libraries(${name}
    OpenSTA
    GTest::gtest
    GTest::gtest_main
    ${TCL_LIBRARY}
  )
  target_include_directories(${name} PRIVATE
    ${STA_HOME}/include/sta
    ${STA_HOME}
    ${CMAKE_BINARY_DIR}/include/sta
  )
  gtest_discover_tests(${name}
    WORKING_DIRECTORY ${STA_HOME}
    PROPERTIES LABELS "cpp;module_sdc"
  )
endmacro()

sta_cpp_test(TestSdcClasses)
sta_cpp_test(TestSdcStaInit)
sta_cpp_test(TestSdcStaDesign)
