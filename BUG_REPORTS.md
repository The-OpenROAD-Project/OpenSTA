# Bug Reports - Upstream STA Update

These bugs were found during test adaptation after the massive upstream STA update
(Corner→Scene, Mode architecture, etc.). Source code was NOT modified per policy;
only test files were edited. These bugs exist in the upstream source code.

---

## Bug 1: `report_clock_skew -clocks` broken - undefined variable `$clks`

**File:** `search/Search.tcl`, line 272
**Severity:** High - command completely broken
**Symptom:** `report_clock_skew -clock <name>` fails with Tcl error

**Description:**
The `report_clock_skew` proc has a debug `puts` statement where the variable
assignment should be. When the `-clocks` key is provided, `$clks` is never set,
causing the command to fail.

**Expected code (approximately):**
```tcl
set clks $keys(-clocks)
```

**Actual code:**
```tcl
puts "clocks $keys(-clocks)"   ;# debug print left in by mistake
```

**Impact:** Any use of `report_clock_skew` with `-clock`/`-clocks` argument will fail.

---

## Bug 2: `startpointPins()` declared but never implemented

**File:** `include/sta/Sta.hh` (declaration), no `.cc` implementation
**Severity:** Medium - linker error if called
**Symptom:** Symbol not found at link time

**Description:**
`Sta::startpointPins()` is declared in the header but has no implementation in
any `.cc` file. The old `Search::visitStartpoints(VertexPinCollector*)` was also
removed. There is no replacement API for enumerating timing startpoints.

**Impact:** Cannot enumerate startpoint pins programmatically.

---

## Bug 3: `CcsCeffDelayCalc::reportGateDelay` use-after-free

**File:** `dcalc/CcsCeffDelayCalc.cc`, line ~679
**Severity:** High - potential crash or wrong results
**Symptom:** SEGFAULT or corrupted data when calling `reportDelayCalc` after `updateTiming`

**Description:**
`CcsCeffDelayCalc::reportGateDelay` accesses the member variable `parasitics_`
which was set during a previous `gateDelay()` call (invoked by `updateTiming`).
After timing update completes, the parasitics pointer can become stale (freed
and reallocated for other objects). GDB inspection showed `parasitics_` pointing
to a `LibertyPort` vtable instead of `ConcreteParasitics`.

Other delay calculators (e.g., `DmpCeffDelayCalc::reportGateDelay`) refresh
their parasitics reference by calling `scene->parasitics(min_max)` inline.
`CcsCeffDelayCalc` should do the same.

**Suggested fix:**
In `CcsCeffDelayCalc::reportGateDelay`, fetch fresh parasitics via
`scene->parasitics(min_max)` instead of relying on the stale `parasitics_` member.

---

## Bug 4: `reduceToPiElmore()`/`reduceToPiPoleResidue2()` null Scene dereference

**File:** `parasitics/ReduceParasitics.cc`
**Severity:** Medium - crash when Scene is null
**Symptom:** SEGFAULT when calling reduce functions without a valid Scene

**Description:**
Both `reduceToPiElmore()` and `reduceToPiPoleResidue2()` unconditionally
dereference the `scene` parameter at `scene->parasitics(min_max)` without
checking for null. While callers should always pass a valid Scene, the lack
of a null check makes debugging difficult when called incorrectly.

**Suggested fix:**
Add a null check for `scene` parameter, or at minimum add an assertion:
```cpp
assert(scene != nullptr);  // or sta_assert
```

---

## Bug 5: SWIG interface gaps - functions removed without replacements

**Severity:** Low-Medium
**Symptom:** Tcl commands no longer available

The following functions were removed from the SWIG `.i` interface files without
providing equivalent replacements accessible from Tcl:

| Removed Function | Was In | Notes |
|---|---|---|
| `sta::startpoints` | Search.i | No replacement; `startpointPins()` declared but not implemented |
| `sta::is_ideal_clock` | Search.i | No replacement found |
| `sta::min_period_violations` | Search.i | No replacement; `report_check_types` only prints |
| `sta::min_period_check_slack` | Search.i | No replacement |
| `sta::min_pulse_width_violations` | Search.i | No replacement |
| `sta::min_pulse_width_check_slack` | Search.i | No replacement |
| `sta::max_skew_violations` | Search.i | No replacement |
| `sta::max_skew_check_slack` | Search.i | No replacement |
| `sta::check_slew_limits` | Search.i | No replacement |
| `sta::check_fanout_limits` | Search.i | No replacement |
| `sta::check_capacitance_limits` | Search.i | No replacement |
| `sta::remove_constraints` | Sdc.i | No replacement |
| `sta::pin_is_constrained` | Sdc.i | No replacement |
| `sta::instance_is_constrained` | Sdc.i | No replacement |
| `sta::net_is_constrained` | Sdc.i | No replacement |
| `Vertex::is_clock` | Graph.i | No replacement |
| `Vertex::has_downstream_clk_pin` | Graph.i | No replacement |
| `Edge::is_disabled_bidirect_net_path` | Graph.i | No replacement |

**Impact:** Scripts using these functions will fail. The check/violation counting
functions are particularly important for timing signoff scripts that need
programmatic access to violation counts (not just printed reports).

---

## Bug 6: `max_fanout_violation_count` / `max_capacitance_violation_count` crash

**Severity:** Medium - crash on valid input
**Symptom:** SEGFAULT (exit code 139) when calling without limit constraints set

**Description:**
Calling `max_fanout_violation_count` or `max_capacitance_violation_count` when
no corresponding limit constraints have been set results in a segmentation fault
instead of returning 0 or an error message. These functions should handle the
case where no limits are defined gracefully.
