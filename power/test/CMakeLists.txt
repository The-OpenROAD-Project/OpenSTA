sta_module_tests("power"
  TESTS
    detailed
    propagate
    report
    report_options
    saif
    saif_vcd
    vcd_detailed
    activity_cases
    mult16_correlation
    when_scenes
    enable_flop
    scene_clk
    tie_when
)

add_subdirectory(cpp)
