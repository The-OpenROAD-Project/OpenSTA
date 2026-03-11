
# DIRECTION

다음과 같은 review feedback을 받았어.

1. 전부 검토한 다음 수정 계획을 짜서 IMPL_PLAN.md 파일에 정리해줘
2. 이후 내가 계획을 승인하면 실행해줘.


# Review feedbacks


## Comment 1 

Arbitrarily picking https://github.com/The-OpenROAD-Project-private/OpenSTA/blob/secure-sta-test-by-opus/verilog/test/verilog_write_types.tcl:

Including line numbers in the test will quickly become stale, eg

# Targets: VerilogWriter.cc uncovered functions:
#   writeInstBusPin (line 382, hit=0), writeInstBusPinBit (line 416, hit=0)

The test is just that the file exists and is non-empty which is pretty useless:

DONE

# Write basic verilog
set out1 [make_result_file verilog_types_out1.v]
write_verilog $out1
puts "PASS: write_verilog with multiple types"

if { [file exists $out1] && [file size $out1] > 0 } {
  puts "PASS: output file 1 exists, size: [file size $out1]"
}

The file consists of many independent tests which will be annoying if you have to debug one (you have to edit the file or skip over all preceding tests).   

Also the .ok has warnings like:
Warning: ../../test/nangate45/Nangate45_typ.lib line 37, library NangateOpenCellLibrary already exists.
because it is loading the same .lib over in the different tests.

This is not a good quality test.


## Comment 2

I don't think we should write tcl tests for everything.  It is fine for sdc testing but core functions should be tested in c++ (eg dcalc).

[DIRECTION] Most of the Tcl tests in dcalc/test/*.tcl should be converted to C++ tests.  --> DONE
[DIRECTION] Check other module-level Tcl tests too, and do the same conversion if needed.


## Comment 3

Poking a bit more:

- Why is a catch used at graph/test/graph_bidirect.tcl#L78 ?  I see this pattern scatted about (randomly as far as I can tell).

DONE

## Comment 4
- Many tests use this pattern:
report_checks
puts "PASS: baseline"
I get that we are going to diff the output so that report_checks will be checked against the ok file.  Printing PASS is weird as it will always print and be confusing if the test actually failed.

## Comment 5
- jk: Where are tests that modify the netlist and do incremental timing?  That is a key use case and has been the source of various bugs.

## Comment 6
- liberty/test/liberty_wireload.tcl tests setting the wireload but nothing about computing the correct values.

jk: Add report_checks call for each set_wire_load_model

## Comment 7
- liberty/test/liberty_ccsn_ecsm.tcl claims to test ecsm but loads only libraries with nldm models.

jk: can make a new test for ecsm?

## Comment 8
- liberty/test/liberty_sky130_corners.tcl says it tests corners but fails use with read_liberty with -min or -max.  It doesn't really tests corners.  It has many "tests" like this that do little and have nothing to do with corners:
read_liberty ../../test/asap7/asap7sc7p5t_INVBUF_SLVT_TT_nldm_220122.lib.gz
puts "PASS: read ASAP7 INVBUF SLVT TT"

## Comment 9
- I see liberty/test/sdc_disable_case.ok but no corresponding test

## Comment 10
- C++ tests are too short. Need long sequence C++ unit tests for each major functionality of each module.
- Need to make too short tests long enough.

## Comment 11
- Tcl test result will be compared to the golden .ok files. So printing "PASS" is meaningless. Remove them.

## Comment 12

"sdc/test $ ./regression -R sdc_variables.tcl" works.
But "test $ ./regression -R verilog_specify" does not work. Make this work.


## Comment 13

search/test/CMakeLists.txt file is messy. Not easy to maintain.
Please enhance the file like /workspace/clean/OpenROAD-flow-scripts/tools/OpenROAD/src/rsz/test/CMakeLists.txt

