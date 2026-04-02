sta_module_tests("spice"
  TESTS
    gate_advanced
    gate_cells
    gcd_gate
    gcd_path
    multipath
    path_min
    subckt_file
    write
    write_options
)

add_subdirectory(cpp)
