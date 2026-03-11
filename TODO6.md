# TODO6: Test Quality Review Against how_to_write_good_tests.md

## A. Files Over 5,000 Lines (Rule 4)

| File | Lines | Action |
|------|-------|--------|
| sdc/test/cpp/TestSdc.cc | 10,831 | Split into 2-3 files |
| search/test/cpp/TestSearchStaDesign.cc | 8,736 | Split into 2 files |
| liberty/test/cpp/TestLibertyStaBasics.cc | 6,747 | Split into 2 files |
| network/test/cpp/TestNetwork.cc | 5,857 | Split into 2 files |

## B. Weak Assertions: (void) Casts (Rule 3)

Replace `(void)result` with appropriate `EXPECT_*` assertions.

| File | Test Name(s) | Count |
|------|-------------|-------|
| liberty/test/cpp/TestLibertyStaBasics.cc | TimingArcSetSense, TimingArcSetArcTo, +20 more | ~22 |
| liberty/test/cpp/TestLibertyClasses.cc | DefaultScaleFactors, SetIsClockCell, SetLevelShifterType, SetSwitchCellType | 5 |
| search/test/cpp/TestSearchStaDesign.cc | TagOperations, PathEndCmp, WnsSlackLess, MaxSkewCheckAccessors, MinPeriodCheckAccessors, +100 more | ~100+ |
| search/test/cpp/TestSearchStaInitB.cc | SearchTagCount2, SearchTagGroupCount2, SearchClkInfoCount2, +15 more | ~18 |
| search/test/cpp/TestSearchStaInit.cc | SigmaFactor, SetArcDelayCalc, SetParasiticAnalysisPts, SetTimingDerateGlobal | 4 |
| sdc/test/cpp/TestSdc.cc | ExceptionPathLessComparator, ClockPairLessOp, ClockSetCompare, PinClockPairLessDesign, PinPairHashConstruct | 5 |
| network/test/cpp/TestNetwork.cc | PinIdLessConstructor, NetIdLessConstructor, InstanceIdLessConstructor, PortIdLessConstructor, CellIdLessConstructor, PinSetCompare, NetSetCompare, AdapterCellAttributeMap, AdapterInstanceAttributeMap, AdapterPinVertexId, AdapterBusName | 13 |
| parasitics/test/cpp/TestParasitics.cc | PoleResidueBaseSetPiModel, PoleResidueBaseSetElmore, +11 more | 13 |
| dcalc/test/cpp/TestDcalc.cc | FindRoot (void)root casts | 3 |
| dcalc/test/cpp/TestFindRoot.cc | (void)root cast | 1 |
| power/test/cpp/TestPower.cc | PinActivityQuery (void)density/duty | 1 |
| spice/test/cpp/TestSpice.cc | VertexArrivalForSpice (void)arr | 1 |
| util/test/cpp/TestUtil.cc | RedirectFileAppendBegin (void)bytes_read | 1 |

## C. Weak Assertions: SUCCEED() / EXPECT_TRUE(true) (Rule 9)

| File | Test Name(s) | Count |
|------|-------------|-------|
| dcalc/test/cpp/TestDcalc.cc | TimingDmpCeffElmore, TimingDmpCeffTwoPole, TimingLumpedCap, TimingArnoldi, TimingUnit, GraphDelayCalcFindDelays, TimingCcsCeff, TimingPrima, IncrementalDelayWithDesign, SwitchDelayCalcMidFlow, +8 DesignDcalcTest, +6 GraphDelayCalc | ~24 |
| sdc/test/cpp/TestSdc.cc | CycleAcctingHashAndEqual, ClkNameLessInstantiation, ClockNameLessInstantiation | 3 |
| sdf/test/cpp/TestSdf.cc | WriteThenReadSdf, ReadSdfUnescapedDividers, +13 more | ~15 |
| network/test/cpp/TestNetwork.cc | ConstantNetsAndClear, LibertyLibraryIterator | 2 |
| verilog/test/cpp/TestVerilog.cc | StmtDestructor, InstDestructor | 2 |
| util/test/cpp/TestUtil.cc | LogEndWithoutLog, RedirectFileEndWithoutRedirect, StringDeleteCheckNonTmp, PrintConsoleDirect, StatsConstructAndReport, FlushExplicit, PrintErrorConsole, StringDeleteCheckRegular, PrintErrorConsoleViaWarn | 9 |

## D. Load-only Tcl Tests (Rule 6)

| File | Issue |
|------|-------|
| sdc/test/sdc_exception_intersect.tcl | write_sdc only, no report_checks/diff_files, .ok empty |
| sdc/test/sdc_exception_thru_complex.tcl | write_sdc only, no report_checks/diff_files, .ok empty |
| sdc/test/sdc_exception_override_priority.tcl | write_sdc only, no report_checks/diff_files, .ok empty |
| sdc/test/sdc_write_roundtrip_full.tcl | write_sdc/read_sdc only, no verification |
| liberty/test/liberty_equiv_cells.tcl | equiv_cells results stored but never verified |
| liberty/test/liberty_multi_lib_equiv.tcl | equiv results stored but never verified |

## E. Orphan .ok Files (Rule 8)

| File | Issue |
|------|-------|
| test/delay_calc.ok | No matching .tcl |
| test/min_max_delays.ok | No matching .tcl |
| test/multi_corner.ok | No matching .tcl |
| test/power.ok | No matching .tcl |
| test/power_vcd.ok | No matching .tcl |
| test/sdf_delays.ok | No matching .tcl |
| test/spef_parasitics.ok | No matching .tcl |

## F. Inline Data File Creation (Rule 1) — Won't Fix

The following use inline data by design (testing specific parser constructs not covered by checked-in libraries):
- liberty/test/cpp/TestLibertyStaCallbacks.cc — R9_/R11_ tests for specific liberty parser callbacks
- sdf/test/cpp/TestSdf.cc — Testing specific SDF constructs
- spice/test/cpp/TestSpice.cc — Testing CSV/SPICE parser with specific data patterns
- spice/test/*.tcl (8 files) — SPICE tests need inline model/subckt data (no checked-in SPICE models exist)
