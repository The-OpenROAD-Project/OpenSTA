# Test Quality Improvement — Implementation Plan (Revised)

## Status Summary

| Comment | Issue | Status |
|---------|-------|--------|
| 1 | verilog_write_types.tcl quality | **Done** |
| 2 | Tcl→C++ conversion (dcalc, others) | **Done** |
| 3 | Unnecessary catch blocks | **Done** — removed where safe, kept where commands genuinely fail |
| 4 | "puts PASS" anti-pattern | **Done** |
| 5 | Incremental timing tests | **Done** — TestSearchIncremental.cc expanded: 8→36 tests |
| 6 | liberty_wireload.tcl missing report_checks | **Done** — report_checks after each set_wire_load_model |
| 7 | liberty_ccsn_ecsm.tcl misnaming | **Done** — renamed to liberty_ccsn.tcl |
| 8 | liberty_sky130_corners.tcl not testing corners | **Done** — define_corners + read_liberty -corner, actual multi-corner timing |
| 9 | Orphaned sdc_disable_case.ok | **Done** |
| 10 | C++ tests too short | **Done** — TestPower: 71→96, TestSpice: 98→126, TestSearchIncremental: 8→36 |
| 11 | Remove PASS prints | **Done** (same as Comment 4) |
| 12 | `test/ $ ./regression -R verilog_specify` broken | **Done** — special case for test/ directory |
| 13 | search/test/CMakeLists.txt messy | **Done** — sta_module_tests() function, all 12 modules refactored |

---

## Remaining Tasks

### Task 1: Remove remaining catch blocks (Comment 3)

**Problem:** 176 `catch` blocks remain in 30 Tcl test files, silently swallowing errors.

**Affected files (30):**

| Module | Files | Count |
|--------|-------|-------|
| search | 16 files | 84 |
| sdc | 6 files | 39 |
| liberty | 8 files | 35 |
| verilog | 4 files | 4 |

**Action for each file:**
1. Remove `catch {}` wrappers around commands that should succeed — keep the command bare
2. Keep `catch` only when **intentionally testing an error condition** (e.g., `if {[catch {...} msg]} { ... }`)
3. Regenerate `.ok` files by running the test

**Note:** Some catch blocks may have been left intentionally (e.g., `liberty_ccsn_ecsm.tcl` wrapping `report_lib_cell` calls that may fail on missing cells). Review each case carefully.

---

### Task 2: Fix verilog test quality issues (Comment 1)

**Problems remaining:**
1. Stale line-number comments in headers (e.g., `# writeInstBusPin (line 382, hit=0)`)
2. Useless `if { [file exists $out] && [file size $out] > 0 }` checks — 16 files
3. `verilog_write_types.tcl` is monolithic (174 lines, no .ok file)

**Affected files:**
- `verilog_write_types.tcl`, `verilog_assign.tcl`, `verilog_complex_bus.tcl`, `verilog_const_concat.tcl` — stale line# comments
- 15 verilog + 1 sdf test files — empty file-existence checks

**Action:**
1. Remove all `# Targets: ... (line NNN, hit=N)` comment blocks from headers
2. Remove the useless `if { [file exists ...] && [file size ...] > 0 } { }` blocks entirely (the `.ok` diff already validates output)
3. Either delete `verilog_write_types.tcl` (its .ok is already gone) or regenerate its .ok
4. Regenerate affected `.ok` files

---

### Task 3: Clean up dcalc Tcl tests (Comment 2)

**Problem:** 21 dcalc Tcl test files remain but ALL their `.ok` files were deleted. These Tcl tests are now effectively dead (cannot be regression-tested). New C++ tests have been added in `TestDcalc.cc` and `TestFindRoot.cc`.

**Action:**
1. Verify the new C++ tests cover what the Tcl tests covered
2. Delete the 21 orphaned Tcl files
3. Remove their entries from `dcalc/test/CMakeLists.txt`

**Files to delete:**
```
dcalc/test/dcalc_advanced.tcl       dcalc/test/dcalc_multi_engine_spef.tcl
dcalc/test/dcalc_annotate_slew.tcl  dcalc/test/dcalc_prima.tcl
dcalc/test/dcalc_annotated_incremental.tcl  dcalc/test/dcalc_prima_arnoldi_deep.tcl
dcalc/test/dcalc_arnoldi_prima.tcl  dcalc/test/dcalc_report.tcl
dcalc/test/dcalc_arnoldi_spef.tcl   dcalc/test/dcalc_spef.tcl
dcalc/test/dcalc_ccs_incremental.tcl
dcalc/test/dcalc_ccs_parasitics.tcl
dcalc/test/dcalc_corners.tcl
dcalc/test/dcalc_dmp_ceff.tcl
dcalc/test/dcalc_dmp_convergence.tcl
dcalc/test/dcalc_dmp_edge_cases.tcl
dcalc/test/dcalc_dmp_pi_model_deep.tcl
dcalc/test/dcalc_engines.tcl
dcalc/test/dcalc_gcd_arnoldi_prima.tcl
dcalc/test/dcalc_graph_delay.tcl
dcalc/test/dcalc_incremental_tolerance.tcl
```

---

### Task 4: Fix liberty_wireload.tcl (Comment 6)

**Problem:** Sets 9 different wireload models but only calls `report_checks` once at the end. Reviewer says: "Add report_checks call for each set_wire_load_model."

**File:** `liberty/test/liberty_wireload.tcl`

**Action:**
1. Add `report_checks -from [get_ports in1] -to [get_ports out1]` after **each** `set_wire_load_model` call (lines 23-39)
2. This produces output that shows timing changes per wireload model — captured in the `.ok` golden file
3. Regenerate `liberty_wireload.ok`

---

### Task 5: Fix liberty_ccsn_ecsm.tcl (Comment 7)

**Problem:** Test claims to test ECSM but only loads NLDM libraries. No ECSM libraries exist in the test data.

**File:** `liberty/test/liberty_ccsn_ecsm.tcl`

**Action:**
1. Rename to `liberty_ccsn.tcl` — remove "ecsm" from the name since no ECSM library is available
2. Update comments to remove ECSM claims
3. Also fix the 13 remaining catch blocks in this file (Task 1 overlap)
4. Update CMakeLists.txt registration
5. Rename `.ok` file accordingly
6. Regenerate `.ok`

---

### Task 6: Fix liberty_sky130_corners.tcl (Comment 8)

**Problem:**
1. Claims to test corners but never uses `read_liberty -min` or `-max`
2. Contains unrelated ASAP7 library reads (lines 125-153) that have nothing to do with corners

**File:** `liberty/test/liberty_sky130_corners.tcl`

**Action:**
1. Use `define_corners` + `read_liberty -corner` to actually test multi-corner:
   ```tcl
   define_corners fast slow
   read_liberty -corner fast ../../test/sky130hd/sky130_fd_sc_hd__ff_n40C_1v95.lib
   read_liberty -corner slow ../../test/sky130hd/sky130_fd_sc_hd__ss_n40C_1v40.lib
   ```
2. Add a design + constraints, then `report_checks` per corner to verify different timing
3. Remove unrelated ASAP7 library reads (already covered by other tests)
4. Fix remaining catch blocks (Task 1 overlap)
5. Regenerate `.ok`

---

### Task 7: Fix `test/ $ ./regression -R verilog_specify` (Comment 12)

**Problem:** The `test/regression` script derives the module name from the current directory:
```bash
module=${script_path#${sta_home}/}   # → "test"
module=${module%%/*}                  # → "test"
```
Then runs `ctest -L "module_test"`. But tests in `test/CMakeLists.txt` are labeled `"tcl"`, not `"module_test"`.

**Root cause:** `test/CMakeLists.txt` uses `sta_tcl_test()` which sets label `"tcl"`, while module CMakeLists.txt files set label `"module_<name>"`.

**Fix options (choose one):**
- **Option A (recommended):** Update `test/regression` to handle the `test/` directory as a special case — when module is "test", use label "tcl" instead of "module_test"
- **Option B:** Add `"module_test"` label to `sta_tcl_test()` function

I recommend **Option A** because it's minimal and doesn't change the labeling semantics.

**Action:**
1. Edit `test/regression` to add special case for `test/` directory
2. Verify: `cd test && ./regression -R verilog_specify` passes

---

### Task 8: Clean up module CMakeLists.txt files (Comment 13)

**Problem:** `search/test/CMakeLists.txt` is 519 lines of repetitive boilerplate:
```cmake
add_test(
  NAME tcl.search.timing
  COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> search_timing
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
set_tests_properties(tcl.search.timing PROPERTIES LABELS "tcl;module_search")
```
This 6-line block is repeated ~85 times. The reference (`rsz/test/CMakeLists.txt`) uses a declarative list:
```cmake
or_integration_tests("rsz" TESTS test1 test2 ...)
```

**Action:**
1. Create a `sta_module_tests()` CMake function (analogous to `or_integration_tests()`) in the top-level CMake or a shared include:
   ```cmake
   function(sta_module_tests module_name)
     cmake_parse_arguments(ARG "" "" "TESTS" ${ARGN})
     foreach(test_name ${ARG_TESTS})
       add_test(
         NAME tcl.${module_name}.${test_name}
         COMMAND bash ${STA_HOME}/test/regression.sh $<TARGET_FILE:sta> ${module_name}_${test_name}
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
       )
       set_tests_properties(tcl.${module_name}.${test_name} PROPERTIES LABELS "tcl;module_${module_name}")
     endforeach()
   endfunction()
   ```
2. Convert **all** module CMakeLists.txt files to use this function:
   ```cmake
   sta_module_tests("search"
     TESTS
       timing
       report_formats
       analysis
       ...
   )
   add_subdirectory(cpp)
   ```
3. Affected files (all using verbose pattern):
   - `search/test/CMakeLists.txt` (519 lines → ~85 lines)
   - `sdc/test/CMakeLists.txt` (281 lines → ~50 lines)
   - `liberty/test/CMakeLists.txt` (218 lines → ~40 lines)
   - `verilog/test/CMakeLists.txt` (190 lines → ~35 lines)
   - `network/test/CMakeLists.txt` (183 lines → ~30 lines)
   - `dcalc/test/CMakeLists.txt` (148 lines → ~25 lines, after Task 3 cleanup)
   - `parasitics/test/CMakeLists.txt` (120 lines → ~20 lines)
   - `graph/test/CMakeLists.txt` (78 lines → ~15 lines)
   - `sdf/test/CMakeLists.txt` (71 lines → ~15 lines)
   - `spice/test/CMakeLists.txt` (64 lines → ~12 lines)
   - `util/test/CMakeLists.txt` (64 lines → ~12 lines)
   - `power/test/CMakeLists.txt` (50 lines → ~10 lines)
4. Verify all tests still pass via `ctest`

---

### Task 9: Expand short C++ tests (Comment 10) — DEFERRED

**Problem:** Some C++ test files are too short:
- `TestSearchIncremental.cc` — 460 lines, 8 tests
- `TestFindRoot.cc` — 667 lines, 46 tests
- `TestPower.cc` — 914 lines, 71 tests
- `TestSpice.cc` — 1,370 lines, 98 tests

**Action:** This is a substantial effort. Focus on higher-priority items (Tasks 1-8) first. Expansion can be done incrementally in follow-up PRs.

---

### Task 10: Add incremental timing C++ tests (Comment 5) — DEFERRED

**Problem:** Reviewer asked for tests that "modify the netlist and do incremental timing." Tcl tests exist (`search_network_edit_replace.tcl`, `search_network_edit_deep.tcl`, `dcalc_annotated_incremental.tcl`) but C++ coverage is thin (`TestSearchIncremental.cc` has only 8 tests).

**Action:** Expand `TestSearchIncremental.cc` with comprehensive C++ tests. Deferred — same rationale as Task 9.

---

## Execution Order

```
Task 1: Remove remaining catch blocks         (Comment 3)
Task 2: Fix verilog test quality               (Comment 1)
Task 3: Clean up dead dcalc Tcl tests          (Comment 2)
Task 4: Fix liberty_wireload.tcl               (Comment 6)
Task 5: Rename liberty_ccsn_ecsm.tcl           (Comment 7)
Task 6: Fix liberty_sky130_corners.tcl          (Comment 8)
Task 7: Fix test/regression for verilog_specify (Comment 12)
Task 8: Clean up all module CMakeLists.txt      (Comment 13)
---
Task 9:  Expand short C++ tests                (Comment 10) — DEFERRED
Task 10: Add incremental timing C++ tests      (Comment 5)  — DEFERRED
```

Tasks 1-3 can be done in parallel (independent modules).
Tasks 4-6 can be done in parallel (independent liberty test files).
Task 7 is independent.
Task 8 depends on Tasks 1-6 (CMakeLists.txt should reflect final test list).
