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
    PROPERTIES LABELS "cpp;module_liberty"
  )
endmacro()

sta_cpp_test(TestLibertyClasses)
sta_cpp_test(TestLibertyStaBasics)
sta_cpp_test(TestLibertyStaCallbacks)

# Compatibility aggregate target for legacy scripts that still build TestLiberty.
add_custom_target(TestLiberty
  DEPENDS
    TestLibertyClasses
    TestLibertyStaBasics
    TestLibertyStaCallbacks
)
