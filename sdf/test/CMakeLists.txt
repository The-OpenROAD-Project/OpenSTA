sta_module_tests("sdf"
  TESTS
    advanced
    annotation
    check_annotation
    cond_pathpulse
    device_cond
    edge_write
    read_write
    reread_cond
    timing_checks
    write_interconnect
)

add_subdirectory(cpp)
