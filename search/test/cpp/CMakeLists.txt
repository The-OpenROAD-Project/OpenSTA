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
    PROPERTIES LABELS "cpp;module_search"
  )
endmacro()

sta_cpp_test(TestSearchClasses)
sta_cpp_test(TestSearchStaInit)
sta_cpp_test(TestSearchStaInitB)
sta_cpp_test(TestSearchStaDesign)
sta_cpp_test(TestSearchStaDesignB)
sta_cpp_test(TestSearchIncremental)

# Compatibility aggregate target for legacy scripts that still build TestSearch.
add_custom_target(TestSearch
  DEPENDS
    TestSearchClasses
    TestSearchStaInit
    TestSearchStaInitB
    TestSearchStaDesign
    TestSearchStaDesignB
    TestSearchIncremental
)
