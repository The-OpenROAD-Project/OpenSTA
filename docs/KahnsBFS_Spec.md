# Kahn's Algorithm BFS for OpenSTA

**Functional Specification**
*April 2026*

---

## 1. Motivation

OpenSTA's parallel BFS traversal (`visitParallel`) processes vertices one level at a time. All threads must finish the current level before any thread can start the next. If a level has only a handful of vertices, most threads sit idle waiting for them to finish. In real designs, level sizes vary widely — some levels have thousands of vertices and some have very few — making this wait-at-every-level approach a significant bottleneck for multi-threaded timing analysis.

Kahn's algorithm is a classical method for topological traversal of a directed acyclic graph. It tracks how many unprocessed predecessors each vertex has (its "in-degree"). A vertex becomes ready as soon as its in-degree reaches zero — meaning all the vertices it depends on have been processed. This is a natural fit for timing analysis: a vertex's arrival time depends only on its fanin, so it can be computed the moment all fanin arrivals are known, without waiting for unrelated vertices at the same level to finish.

## 2. Proposed Solution

Replace the per-level barrier model with Kahn's topological traversal. Instead of waiting for all vertices at level `L` to finish before starting level `L+1`, a vertex becomes eligible for processing as soon as every one of its predecessors has been processed. This allows vertices at different levels to execute concurrently, keeping threads busy.

The implementation is integrated into the existing `BfsIterator` class hierarchy as a runtime toggle, supporting both forward (arrival) and backward (required-time) propagation. The original level-based BFS remains the default and is always available as a fallback.

## 3. Algorithm

The timing graph is already a DAG within each `visitParallel()` call: flip-flop feedback is broken at D inputs, latch D-to-Q edges are excluded by search predicates, and combinational loops are broken by the Levelizer's disabled-loop edges. This satisfies Kahn's requirement for an acyclic graph.

When Kahn's is enabled, `visitParallel()` proceeds in two stages.

### Stage 1: Discovery and In-Degree Counting (single-threaded)

Starting from the seed vertices already in the BFS queue, a forward BFS discovers all reachable vertices following the same edge-filtering rules used by the original traversal. As each new vertex is discovered, its in-degree (number of active predecessors) is recorded in a flat array indexed by graph vertex ID. Seed vertices have in-degree zero.

### Stage 2: Parallel Traversal (multi-threaded)

The unit of scheduling is a single ready vertex. All zero-in-degree vertices are initially handed to workers in the `DispatchQueue` thread pool as "seed shards"; workers then expand the frontier themselves. Each worker task does three things for every vertex it processes:

1. Visit the vertex (computing arrivals or required times).
2. Atomically decrement the in-degree of each successor.
3. If any successor's in-degree reaches zero, that successor becomes ready to visit and is enqueued for processing.

A single `finishTasks()` call at the end waits for all work — including newly-ready successors produced during running tasks — to complete. There are no per-batch or per-level barriers. Any idle worker can pick up a ready vertex without waiting for unrelated tasks to finish. The `DispatchQueue` uses `condition_variable` internally, so idle threads block efficiently rather than spinning.

The initial implementation dispatched each newly-ready successor individually back through the `DispatchQueue`, which serialized on its internal mutex at high thread counts; Section 13 documents the per-worker local-batching fix that moved Stage 2 to a mostly lock-free scheduling model. The concurrency contract on the in-degree counter itself, described next, is independent of the scheduling choice.

### Stage 2 concurrency: atomic fetch_sub

Two workers can visit predecessors of the same successor simultaneously. Suppose vertex C has two active predecessors A and B, so `in_degree[C] == 2` after Stage 1. Worker W1 visits A, W2 visits B, and both reach the successor decrement concurrently:

```cpp
int prev = in_degree[C].fetch_sub(1, std::memory_order_acq_rel);
if (prev == 1)
  <enqueue C for visiting>
```

Three properties make this correct without locks.

**Atomic RMW prevents lost updates.** `fetch_sub` compiles to `LOCK XADD` on x86 (equivalent atomic RMW on other architectures); the hardware guarantees the read-decrement-write triple is indivisible. Two workers cannot both read the same value and each write the same decremented value. Cache coherence serializes the two attempts into a total order and returns each caller a distinct before-value.

**Unique-winner detection.** `fetch_sub` returns the old value. For `in_degree[C]` transitioning 2 → 1 → 0, exactly one worker sees `prev == 1` — the one whose decrement produced zero — and is uniquely responsible for enqueueing C. The other sees `prev == 2` and does nothing. No single worker "owns" C; ownership emerges from the atomic result. This makes the "who enqueues" question self-answering — no external coordination, no duplicate visits.

**Memory ordering: `acq_rel`.** W1's `visit(A)` writes arrival state that C's eventual `visit(C)` will later read. Without `acq_rel`, those writes could be reordered past the `fetch_sub`, so the worker that wins C could observe a stale predecessor state. Release on the store side makes everything W1 wrote before the decrement visible to anyone observing the decrement's effect; acquire on the load side guarantees the winning worker sees all earlier predecessor writes. This establishes the happens-before edge that makes "visit predecessors before successors" hold at the memory level, not just at the scheduling level.

No lock is required for the decrement or the ownership handoff itself. The shared `DispatchQueue` mutex is touched only at initial seeding and at spill-overflow; see Section 13.1.3.

## 4. Implementation Details

**Files modified:**

- `include/sta/Bfs.hh` — Added `kahnForEachSuccessor` pure virtual method (forward follows out-edges, backward follows in-edges), persistent `KahnState` storage, `use_kahns_` toggle, `kahn_pred_` pointer for the discovery edge filter, and `resetLevelBounds` helper.
- `search/Bfs.cc` — Defined `KahnState` struct holding persistent in-degree arrays (reused across calls to avoid re-allocation). Added a third branch to `visitParallel`: single-threaded / original-parallel / Kahn's-parallel. Implemented `kahnForEachSuccessor` for both `BfsFwdIterator` and `BfsBkwdIterator`.
- `include/sta/Search.hh` — Added `useKahnsBfs()` getter and `setUseKahnsBfs(bool)` setter on Search, forwarding to the arrival and required iterators.
- `search/Search.cc` — Two lines in the Search constructor wire the Kahn's edge filter (`SearchAdj`) onto the arrival and required iterators. Added `Search::useKahnsBfs()` and `Search::setUseKahnsBfs()` implementations.
- `include/sta/Sta.hh` — Added `useKahnsBfs()` / `setUseKahnsBfs()` declarations for the Tcl variable `sta_use_kahns_bfs`.
- `search/Sta.cc` — Implemented `Sta::useKahnsBfs()` and `Sta::setUseKahnsBfs()` as thin forwarders to Search.
- `search/Search.i` — Exposed `use_kahns_bfs` and `set_use_kahns_bfs` to SWIG for Tcl.
- `tcl/Variables.tcl` — Added the `sta_use_kahns_bfs` Tcl variable with a read/write trace that calls the underlying commands.

Enabling Kahn's at the iterator level requires two calls on a `BfsIterator`:

```cpp
iterator->setKahnPred(predicate);  // edge filter for discovery
iterator->setUseKahns(true);       // enable Kahn's
```

The edge filter is separate from the iterator's existing `search_pred_` because the original BFS never uses `search_pred_` directly for arrivals — the visitor provides its own filter at call time. Kahn's discovery runs before any visitor, so it needs the filter upfront. If the filter is null, `visitParallel` falls back to the original BFS.

Kahn's is also bypassed — even when enabled — whenever the Tcl variable `sta_dynamic_loop_breaking` is set. That feature relies on arrival tags that only emerge during propagation to decide whether an otherwise-disabled loop edge can be traversed. Kahn's needs the active subgraph and in-degrees known before propagation begins, so it cannot consult those tags. To avoid silently missing vertices, `visitParallel` guards the Kahn's path with an explicit check on `variables_->dynamicLoopBreaking()` and falls back to the original level-based BFS whenever dynamic loop breaking is active. The toggle remains a no-op from the user's point of view; results stay correct.

For end users, Kahn's can be toggled from Tcl via a design-level variable:

```tcl
set sta_use_kahns_bfs 1      ;# enable Kahn's (default)
set sta_use_kahns_bfs 0      ;# fall back to original level-based BFS
puts $sta_use_kahns_bfs      ;# read current setting
```

Setting the variable calls `Sta::setUseKahnsBfs`, which applies the flag to both the arrival (forward) and required-time (backward) BFS iterators. No arrivals or requireds are invalidated on toggle — the two algorithms produce bit-identical results, so cached state remains valid. Scripts can flip the variable mid-session to compare the two paths without a full rerun.

The Tcl plumbing is registered at package load via `Variables.tcl` (`trace add variable`) and the underlying commands `use_kahns_bfs` / `set_use_kahns_bfs` are exposed through `Search.i` (SWIG) to the `sta` namespace.

Persistent state (`KahnState`) stores the in-degree arrays across calls. On the first call it allocates; on subsequent calls it resets only the entries touched previously, avoiding full re-initialization.

The Stage 2 task body is a `std::function` defined as a local variable in `visitParallel`. It captures itself by reference so that task code can recursively dispatch successors via the same function. Its lifetime is the duration of `visitParallel`; `finishTasks()` guarantees all dispatched tasks complete before the function returns, so the self-reference is always valid.

## 5. Incremental Timing Updates

OpenSTA supports incremental timing: when a cell is resized or an edge delay changes, only the affected vertices need to be re-evaluated instead of recomputing the whole graph. This is driven by `Search.cc`, which tracks dirty vertices in an "invalid arrivals" set and enqueues them as seeds before the next `findArrivals` call. Our implementation hooks into this existing mechanism without modification.

When Kahn's runs, the seed vertices in the BFS queue are exactly the dirty ones supplied by the incremental framework. The discovery stage walks forward from those seeds and finds the downstream subgraph that could be affected. Only that subgraph — not the whole graph — gets in-degrees computed and gets visited in Stage 2. For small updates (a few changed cells in a large design), the active set is a small fraction of the total graph, and the work is proportional to it.

There is one behavioral difference from the original BFS worth noting. The original stops propagating through a vertex whose arrivals did not change after re-evaluation; it skips the enqueue of its fanout. Our Kahn's implementation discovers the full reachable subgraph upfront and decrements in-degrees unconditionally, so every reachable vertex is visited.

The reason is fundamental to Kahn's algorithm: every active predecessor must decrement its successor's in-degree exactly once, otherwise the counter never reaches zero and the vertex stalls forever. If we skipped a decrement because "arrivals didn't change," a downstream vertex with multiple predecessors could be left waiting on a decrement that will never come — even if its other predecessors did change and genuinely need to propagate.

The practical cost is that vertices whose arrivals did not change are still visited, but the visitor detects no change and no downstream updates happen. This is correct but slightly more eager than the original. It has not caused test failures or measurable overhead in any regression so far.

## 6. Comparison with Alternate Implementation

An alternate implementation (`BfsFwdInDegreeIterator`) in a separate repository takes a standalone-class approach used only for delay calculation.

**Architecture.** The alternate creates a separate class. Ours integrates into the existing `BfsIterator` with a toggle, supporting both forward and backward BFS across all callers.

**Discovery cost.** The alternate scans every vertex and edge in the entire graph to compute in-degrees — O(V_total + E_total) where V_total is all vertices in the graph and E_total is all edges. Even if only a small portion needs re-timing, the full graph is walked. Ours starts from the dirty seed vertices and only walks the subgraph reachable from them — O(V_active + E_active) where V_active and E_active are only the vertices and edges that actually need processing. For loop breaking, the alternate uses a raw level comparison (`to_level >= from_level`) to decide which edges to skip. Ours uses the same `SearchAdj` filter that the Levelizer and the rest of the BFS already use, so the set of skipped edges (disabled loops, latch D-to-Q, timing checks) is guaranteed to be consistent.

**Thread safety.** The alternate uses a non-atomic visited flag from worker threads (data race risk) and maintains a per-edge mutex-locked set for deduplication (serialization bottleneck). Ours uses a read-only array for active-set checks and computes in-degrees upfront so edge tracking is unnecessary.

**Scheduling granularity.** The alternate uses batched dispatch — it dispatches a wavefront of ready vertices, waits for all to finish via `finishTasks()`, then dispatches the next wavefront. This re-introduces a barrier between wavefronts. Ours dispatches each ready vertex as a separate task and, when a running task makes a successor ready, dispatches that successor immediately via recursive dispatch into the same `DispatchQueue`. A single `finishTasks()` at the end waits for all work. This removes the per-wavefront barrier and keeps threads continuously fed.

**What we adopted from them.** The `DispatchQueue`-based execution model. Our initial implementation used custom spin-wait workers (`std::this_thread::yield`) which wasted CPU. Moving to `DispatchQueue` with condition_variable-based blocking cut overhead substantially.

## 7. Findings from Regressions

### Finding 1: Vertex IDs can exceed vertexCount() after deletions

The graph's `ObjectTable` stores vertices in blocks of 128. `graph->id(vertex)` returns `(block_index * 128 + slot)`, which can be much larger than `graph->vertexCount()` (the live count) after cells are deleted. Sizing the in-degree array to `vertexCount()+1` caused an out-of-bounds segfault during the `rmp.gcd_restructure` flow, which deletes cells during restructuring.

*Resolution:* The in-degree array now grows dynamically during discovery when any vertex ID exceeds current capacity. Worker threads include bounds checks. The alternate implementation has the same latent issue but has not encountered it because its code path does not trigger the deletion pattern.

### Finding 2: The arrival iterator has a null search predicate

The arrival BFS iterator is constructed with `search_pred = nullptr` because the original BFS never uses it — the visitor always provides the filter. Kahn's discovery used `search_pred` directly, causing a null-pointer crash during arrival propagation in the `rmp` flow.

*Resolution:* Introduced `kahn_pred`, a dedicated predicate for Kahn's discovery, wired to `SearchAdj` in the Search constructor. This keeps the original BFS path completely unchanged.

Both findings were caught by `rmp.gcd_restructure.tcl` and resolved without changing the original BFS behavior.

### Finding 3: Incompatibility with dynamic loop breaking

`sta_dynamic_loop_breaking` (a pre-existing Tcl variable, default off) enables on-the-fly re-activation of disabled-loop edges when arrival propagation produces loop tags that satisfy user-declared false-path exceptions. The check lives in `SearchAdj::searchThru`: a disabled-loop edge is traversable when `(dynamicLoopBreaking() && hasPendingLoopPaths(edge))` holds, where `hasPendingLoopPaths` consults the visitor's live `TagGroupBldr` to see which loop tags are currently propagating.

The `SearchAdj` instance we reuse as `kahn_pred_` (`search_adj_` in `Search.cc`) is constructed with `tag_bldr_ == nullptr`, so `hasPendingLoopPaths` always returns false for it — by design, since Kahn's discovery runs before any visitor is active and there are no live tags to consult. This means that when a user enables `sta_dynamic_loop_breaking` alongside `sta_use_kahns_bfs`, Kahn's discovery and successor decrement would systematically skip disabled-loop edges that the original `ArrivalVisitor` path (using its own tag-aware `adj_pred_`) can traverse. Vertices reachable only through those edges would never enter the active set, leaving their arrivals and slacks stale.

Neither OpenSTA's regression suite nor OpenROAD's standard flows set `sta_dynamic_loop_breaking`, so this never surfaced in testing. It was identified during code review.

*Resolution:* `visitParallel` now falls back to the original level-based BFS whenever `variables_->dynamicLoopBreaking()` is true, regardless of the Kahn's toggle. This is a defensive guard; the Tcl variable still reads and writes normally, but the traversal uses the original path when the two features would otherwise interact unsafely. The cost is one additional boolean check per `visitParallel` invocation.

A future enhancement could make Kahn's loop-breaking-aware by conservatively discovering through disabled-loop edges and adjusting in-degrees based on actual propagation, but that work is non-trivial and not worth pursuing until a concrete use case combines both features.

## 8. Performance

On the OpenSTA regression suite (6109 tests), Kahn's BFS runs at parity with the original level-based BFS (28s vs 27-30s). On small test designs the discovery stage overhead is negligible. On large designs with uneven level populations, barrier elimination should produce net speedups, particularly at high thread counts where the original BFS leaves threads idle.

## 9. Test Plan

Beyond the OpenSTA standalone regression suite and the OpenROAD full regression, a set of helper scripts is provided for A/B runtime benchmarking and validation across ORFS designs. These run the full ORFS flow for each design twice — once with Kahn's BFS disabled and once with Kahn's enabled — and collect per-step timing and design-size metrics for comparison.

All scripts live under `flow/util/` and are intended to be invoked from the `flow/` directory. They do not modify any design scripts or ORFS flow files; instead, a tiny binary wrapper injects the Tcl variable `sta_use_kahns_bfs` into every OpenROAD invocation.

### 9.1 Binary wrapper: `openroad_kahns_wrap.sh`

ORFS invokes openroad with `-no_init`, so `~/.openroad` is not sourced. To toggle `sta_use_kahns_bfs` across every invocation of every flow step without editing any Tcl, this wrapper sits in front of the real OpenROAD binary:

- Finds the `.tcl` cmd_file argument in the invocation.
- Creates a temporary Tcl that performs
  ```tcl
  set sta_use_kahns_bfs <mode>
  puts "kahns-wrap: requested=<mode>, effective=$::sta_use_kahns_bfs"
  source "<original.tcl>"
  ```
- Execs the real OpenROAD on the temporary file.

The wrapper reads `KAHNS_BFS` from the environment (`0` = original BFS, `1` = Kahn's). The breadcrumb `puts` line lands in every step log, so a single grep confirms the flag was in effect and never overridden by a downstream script.

### 9.2 Benchmark driver: `kahns_benchmark.sh`

Runs an A/B sweep across one or more designs. For each design:

1. `make clean_all`
2. Run target (default: `finish`) with `KAHNS_BFS=0`; time with `date +%s.%N`.
3. Save `elapsed-all.txt` and copy `logs/<pdk>/<design>/<variant>/` before the next clean.
4. `make clean_all`
5. Run target with `KAHNS_BFS=1`; time.
6. Save `elapsed-all.txt` and the logs tree again.

Output directory layout:

```
<bench_dir>/
  summary.csv                                         wall-time totals, CSV
  <design>_kahns_off.log                              full stdout, OFF run
  <design>_kahns_on.log                               full stdout, ON run
  <design>_kahns_off_artifacts/elapsed-all.txt        per-step seconds, OFF
  <design>_kahns_off_artifacts/logs/                  raw step logs and JSON metrics, OFF
  <design>_kahns_on_artifacts/elapsed-all.txt         per-step seconds, ON
  <design>_kahns_on_artifacts/logs/                   raw step logs and JSON metrics, ON
```

Usage (from `flow/`):

```
util/kahns_benchmark.sh [-t target] [-o outdir] [design-configs...]
```

Target defaults to `finish`. For STA-focused benchmarking, `-t route` covers all STA-heavy steps (place, repair_timing_post_place, cts, global_route, repair_timing_post_global_route, detail_route) without the downstream fill / final_report overhead.

### 9.3 Per-step runtime comparison: `kahns_compare.sh`

Reads the `elapsed-all.txt` files from a benchmark directory and produces a per-step comparison table with OFF seconds, ON seconds, delta, and ratio (`ON/OFF`). Positive deltas mean Kahn's was slower for that step; ratios below `1.00x` mean Kahn's was faster.

Usage (from `flow/`):

```
util/kahns_compare.sh <bench_dir> [design_tag]
```

Without `design_tag`, every design that has both OFF and ON artifacts is compared in a single run.

Typical reading pattern for a given design:

- Non-STA steps (yosys, floorplan_macro, pdn, fillcell): ratio ~1.00x.
- STA-heavy steps (`3_3_place_gp`, `4_1_cts`, `5_1_grt`, `5_2_route`): where any real speed-up or slowdown appears.
- Small designs: slight positive delta from Kahn's discovery-pass overhead.
- Large designs with uneven level populations: expected speed-up from barrier elimination.

### 9.4 Design-size view and correctness check: `kahns_size.sh`

Extracts design-size metrics (instance count, net count, IO count, cell area) at each major stage from the step-level JSON metrics files (`<step>.json`). Provides three modes:

- **Default (combined view)** — `util/kahns_size.sh <bench_dir> [design_tag]`
  Prints one table per design with the OFF-run values and a match column that flags any stage where ON disagreed. Ideal for spotting correctness regressions at a glance: every row must show `ok`.
- **Verbose (`-v`)** — `util/kahns_size.sh -v <bench_dir> [design_tag]`
  Prints the two separate OFF and ON tables side-by-side so the actual disagreeing values can be read.
- **Validation sweep (`-c`, `--check`)** — `util/kahns_size.sh --check <bench_dir>`
  Iterates every design in the benchmark directory and emits one line per design: `OK`, `FAIL` (with the stage and metrics that disagreed), or `SKIP` (missing artifacts). Exits non-zero if any design fails, which makes it CI-friendly. Any `FAIL` is a real correctness bug — Kahn's must produce the same netlist as the original BFS.

### 9.5 Operational checklist

Running a full sweep across several designs:

1. Build OpenROAD with Kahn's: the flag `sta_use_kahns_bfs` defaults to 1.
2. From `flow/`, choose the target, the output directory, and the design list. For example, pick one or more `config.mk` paths from a platform of interest (under `flow/designs/<platform>/`) and pass them on the command line:
   ```
   util/kahns_benchmark.sh -t finish -o <bench_dir> <config1> <config2> ...
   ```
3. While it runs, tail the most recent per-design stdout log to follow progress and verify the wrapper breadcrumb:
   ```
   tail -f "$(ls -t <bench_dir>/*.log | head -1)" | grep -i "kahns-wrap\|error"
   ```
4. Validate correctness once designs finish:
   ```
   util/kahns_size.sh --check <bench_dir>
   ```
   Address any `FAIL` before trusting the runtime numbers.
5. Compare per-step runtimes:
   ```
   util/kahns_compare.sh <bench_dir>
   ```
   Interpret in the context of design size:
   ```
   util/kahns_size.sh <bench_dir>
   ```

### 9.6 Additional conventions

- Always run `KAHNS_BFS=0` first, then `KAHNS_BFS=1`. The OFF pass is the baseline; running OFF first avoids any chance that a bug in the ON path could corrupt shared state and affect a subsequent OFF run.
- Target choice: `-t route` is usually enough for STA-feature benchmarking. `-t finish` adds fillers / final report which do not exercise Kahn's much.
- Parallelism: ORFS exports `NUM_CORES` to OpenROAD's `-threads` flag. Kahn's and the original BFS both respect this. A fair comparison must use identical thread counts.
- Disk usage: each artifact directory copies the per-design logs tree. Budget a few hundred MB per design for a finish sweep.
- Clean up between sweeps: `kahns_benchmark.sh` always runs `make clean_all` before each design's first iteration. No manual cleanup is required.

## 10. Test Results

**OpenSTA standalone:** 6109/6109 tests PASS with Kahn's enabled.

**OpenROAD full regression:** All tests PASS, including `rmp.gcd_restructure` (the test that surfaced both findings in Section 7), `rsz` (incremental netlist modification), and `cts` (buffer insertion with re-timing).

**ORFS A/B runtime benchmarks (Section 9):** in progress. An initial sweep across several platform/design combinations is running using `util/kahns_benchmark.sh`. Completed designs to date show Kahn's at parity or slightly slower on small designs (for a representative small design the measured overhead was roughly `+3%` / `+12s` over a `~375s` baseline), consistent with the Section 8 prediction that the discovery-pass overhead dominates when the active graph is small. Larger designs are still pending; this section will be updated with their numbers and the per-step breakdown as each finishes. Correctness (netlist-size match between OFF and ON) is verified after each design via `util/kahns_size.sh --check`.

## 11. Limitations of the Current Approach

The current implementation is correct and matches the original BFS at parity on small designs, but several limitations remain.

### Addressed

- **Per-call `active_vertices` allocation.** (Addressed in Section 13.) The `active_vertices`, `seeds`, and per-worker `local_ready` vectors used to be rebuilt every call. They now live in `KahnState` alongside `in_degree_init` and `in_degree` — cleared per call, capacity retained — so incremental flows with many small `visitParallel` invocations no longer pay a re-allocation on every call.
- **Recursive dispatch cost for small workloads.** (Addressed in Section 13.) The original recursive-dispatch scheme called `DispatchQueue::dispatch` per ready successor, which contended heavily on the shared mutex at high thread counts. This has been replaced with per-worker local batches (Section 13) that drain in-line and spill only when deep. The steady-state dispatch count dropped from O(visited_vertices) to O(thread_count + spills).
- **Ref-pin-delay vertex orphaning in cleanup.** (Addressed in Section 13.2.) `ArrivalVisitor::enqueueRefPinInputDelays` pushes vertices into `queue_` during Stage 2 that are outside the precomputed Kahn active set. The post-Stage-2 cleanup used to call `queue_[level].clear()`, which dropped the pointer while leaving `bfsInQueue=true` stuck on the vertex and silently suppressing all future `enqueue()` calls for it. Cleanup now filters, keeping `bfsInQueue=true` entries for the next call.
- **Eager Kahn's walk past reg CK under `clks_only`.** (Addressed in Section 13.3.) Kahn's Stage 1 discovery used to follow graph successors unconditionally. Under `findClkArrivals` (`ArrivalVisitor` with `clks_only=true`) the original BFS stops at register CK via `postponeClkFanouts`; Kahn's now mirrors that stop via `VertexVisitor::stopDiscoveryAtRegClk()` so the discovered active set is narrowed identically. Kahn's itself still runs; only the reg-CK fanout is excluded.

### Open

- **Eager visits in Kahn's traversal.** Beyond the `clks_only` case fixed above, the general "every vertex in the active set is visited regardless of whether its arrival changes" limitation remains. This is a fundamental consequence of the in-degree counting model — each predecessor must decrement each successor's counter exactly once, so skipping decrements is not allowed. The original BFS short-circuits via an "arrivals unchanged" check at the visitor level and avoids enqueuing downstream when no change occurred. We do not. For most designs the cost is small because the visitor itself detects no change and terminates quickly, but in deep-pipeline designs with many cascaded unchanged vertices the extra visits add up. Section 12 item 1 (visit-level change short-circuit) is the roadmap fix; an opt-in prototype of that optimization exists behind `sta_kahn_visit_skip`.
- **Full forward sweep for slack queries.** Slack at pin P is computed as `required(P) − arrival(P)`. The required-time backward BFS is already scoped to P's level. But the forward arrival BFS is not scoped to P's fanin cone — it propagates from all dirty seeds to all endpoints they can reach. For a single-point slack query on a design with large independent cones, most of the forward work is spent on endpoints the query does not care about.
- **Over-invalidation in the dirty set.** The incremental framework's `invalid_arrivals_` and `invalid_requireds_` sets are tracked conservatively. Some edge-delay or pin changes invalidate more vertices than strictly necessary; the visitor detects no change and does no further propagation, but we still paid for the visit. A more precise validity analysis could prune the seed set before the BFS starts.
- **No Kahn's when dynamic loop breaking is enabled.** `sta_dynamic_loop_breaking` decides whether a disabled-loop edge is traversable based on arrival tags that only appear during propagation, which Kahn's upfront-discovery model cannot consult. `visitParallel` therefore falls back to the original level-based BFS whenever `dynamicLoopBreaking()` is true. The Tcl toggle `sta_use_kahns_bfs` still reads normally, but the traversal uses the original path. See Section 7, Finding 3 for details.

## 12. Future Roadmap

The following enhancements extend the current Kahn's-based incremental timing implementation. They address known limitations in the existing approach and are orthogonal to Kahn's itself — each can be layered on top of the existing implementation independently. Items are listed in rough order of payoff relative to effort.

### 1. Visit-level change short-circuit

**Objective:** Restore the "arrivals unchanged" optimization within Kahn's propagation by skipping the visit body when no predecessor's arrival has actually changed.

**Approach:** Track a per-vertex "changed" flag during Kahn's traversal. When a vertex is popped from the queue, check whether any of its incoming arrivals differ from the previously recorded values before performing the full visit.

**Benefit:** Addresses the eager-visit limitation in the current implementation, eliminating redundant work when dirty vertices do not actually propagate value changes downstream.

**Risk and effort:** Low risk, localized change. An opt-in prototype is available today behind the `sta_kahn_visit_skip` toggle (default off); effectiveness depends on selective-invalidation flows like `rsz` / `cts`.

### 2. Validity-boundary seeding

**Objective:** Address over-invalidation by narrowing the set of vertices that require re-propagation.

**Approach:** Before seeding the BFS, perform a pre-pass that walks the dirty-reachable subgraph to identify the minimal boundary — the last layer of vertices whose arrivals remain known-valid but whose fanout begins the invalid region. Seed the BFS from those valid boundary vertices so that propagation walks forward into the invalid region with known-correct source arrivals. The same pattern applies in reverse for required times: find the boundary between known-valid and known-invalid requireds, seed from the valid side, and propagate backward into the invalid region.

**Benefit:** If the invalidation is actually narrower than the dirty set suggests, the boundary pass prunes work the current approach would still perform. Starting from vertices with known-correct arrivals also gives the forward pass a clean reference point, which may catch cases where a dirty vertex's arrival does not actually change.

**Risk and effort:** Medium effort, refinement of the current dirty-set mechanism rather than an architectural change. Can be added in `Search.cc` at the seeding step (`seedInvalidArrivals` / `seedInvalidRequireds`) without modifying the BFS iterator.

### 3. Demand-driven forward propagation for single-point queries

**Objective:** For single slack queries at a given pin, restrict forward propagation to only the vertices that actually influence that pin.

**Approach:** For a slack query at pin P:

- Walk backward from P to compute its fanin cone.
- Intersect the cone with the dirty set — only those dirty vertices actually affect P.
- Propagate forward only from those vertices, restricted to within the cone.

OpenSTA already applies this pattern in the backward required-time pass (which stops at the query pin's level) and for the endpoint-with-no-fanout shortcut (`seedRequired`, no BFS at all). It does not currently do this for the forward arrival pass in slack queries — `findAllArrivals` always goes to `maxLevel`.

**Implementation requirements:**

- A query context passed from the Tcl command down to `findArrivals`.
- A backward cone computation, or a lazy cone-membership check.
- Predicate filtering during Kahn's discovery to skip vertices outside the cone.

Kahn's traversal still applies to the forward pass within the cone — it remains a DAG traversal and retains the barrier elimination benefit. The two optimizations stack rather than compete.

**Benefit:** The largest win for narrow single-point queries on large designs.

**Risk and effort:** Larger architectural change spanning `Search.cc` and the Tcl query entry points. Scope is architectural, not inside the BFS iterator.

### 4. Hybrid scheduling

**Objective:** Address the small-workload overhead of the current scheduling model.

**Approach:** Introduce an adaptive threshold that switches between batched dispatch (suited for small active sets) and recursive dispatch (suited for large active sets), based on the size of the active set at scheduling time.

**Benefit:** Eliminates overhead for small incremental updates while preserving throughput for large ones.

### 5. Multi-query cone batching

**Objective:** Amortize cone-computation cost when multiple slack queries are issued in sequence.

**Approach:** When several slack queries arrive together, compute the union of their backward cones once and perform a single scoped forward sweep across the combined cone, rather than repeating the cone computation and forward traversal per query.

**Benefit:** Reduces redundant work in reporting flows that issue many related queries, such as full endpoint slack reports or path-group summaries.

## 13. Post-Integration Findings and Fixes

After the initial Kahn's implementation shipped, three follow-up issues were identified: a performance regression on large designs (per-vertex dispatch overhead), a latent correctness bug with reference-pin input delays, and a performance regression under the `clks_only` (`findClkArrivals`) path. This section documents each, the fix, and the shared diagnostic harness used to catch future regressions.

### 13.1 Parallel-dispatch overhead

#### 13.1.1 Observed symptom

On a large SoC-scale A/B sweep with 32 threads, the clock tree synthesis step ran ~24% slower with Kahn's enabled than with Kahn's disabled. Other STA-heavy steps showed similar regressions in the ~20-25% range. Parity on the OpenSTA standalone regression suite (Section 8) and on small ORFS designs was undisturbed; the slowdown was specific to designs whose active subgraph during arrival propagation is large.

#### 13.1.2 Root cause

Two pieces of evidence pinned the issue to `DispatchQueue` contention rather than algorithmic cost:

- Isolated STA measurement (harness in Section 13.4.5) showed the same ~25% gap on a full arrival sweep (`report_checks -path_delay max`) when loading a post-CTS database directly, outside of any ORFS step. The effect was not CTS-specific; it was the STA engine.
- A `dispatch()` counter added to `DispatchQueue` revealed that Kahn's issued roughly 5-7× more `dispatch()` calls than the original level-based BFS on identical workloads. Each `dispatch()` call acquires `DispatchQueue::lock_` (a `std::mutex`) and signals a `condition_variable`; at 32 threads, the hundreds of thousands of mutex acquisitions produced severe contention.

The algorithmic atomics in the hot path (`in_degree[sid].fetch_sub(1, memory_order_acq_rel)`) are lock-free and do not loop — they map to a single `LOCK XADD` on x86 — so the contention was not in the algorithm. It was in the work-queue: the level-based BFS dispatches O(thread_count × heavy_levels) tasks, while the original Kahn's dispatched O(visited_vertices) tasks. On a ~2k-cell reference design this was a 5.2× ratio; on designs where the active subgraph is hundreds of thousands of vertices, the ratio and the wall-clock impact grow accordingly.

#### 13.1.3 Fix: per-worker local batches with spill

The recursive-dispatch worker lambda in `visitParallel` was replaced with a per-worker local ready-batch model:

- Each `DispatchQueue` worker runs with a stable thread-id `tid`. The Kahn's branch allocates a stack-local `std::vector<Vertex *> local_ready[tid]`, one slot per worker, and the worker drains its own slot in-line with a while-loop rather than calling `dispatch()`.
- When a successor's in-degree transitions to zero, it is pushed to the current worker's batch instead of dispatched. The same worker pops it on the next drain iteration.
- When the local batch exceeds a spill threshold (`kKahnBatchSpillThreshold = 64`), the older half is handed back to `DispatchQueue` via **one `dispatch()` call per spilled vertex** so idle workers can each grab a different one in parallel. This keeps the worker's most-recent frontier hot locally while preventing starvation in the presence of fan-out spikes. Per-vertex dispatch (rather than a single chunk task carrying all spilled vertices) is deliberate: chunking would let only one idle worker claim the whole spill, whereas one-task-per-vertex lets up to `thread_count` idle workers steal simultaneously. The cost is one mutex acquire on `DispatchQueue::lock_` per spilled vertex; measurements (Section 13.1.4) show that cost is dominated by the surrounding visit work and does not regress wall-clock.
- Initial seeding no longer dispatches one task per seed. Seeds are sharded round-robin across `min(thread_count, seeds.size())` dispatches; each dispatched task pre-loads its shard into the worker's local batch and then enters the drain loop.

Per-tid exclusivity is guaranteed because `DispatchQueue` workers are fixed-id (each worker thread is constructed with its index `i` in `dispatch_thread_handler`). Two concurrent tasks never share a `tid`, so each `local_ready[tid]` slot is touched only by its owning worker — no locking on the vector is needed. The in-degree `fetch_sub` retains its `memory_order_acq_rel` ordering; it is the happens-before edge that establishes visitor-writes-before-successor-reads, and switching between local-push and shared-dispatch does not change that ordering because the same thread that wins the decrement is the thread that subsequently owns the successor's visit. The `setBfsInQueue(bfs_index_, false)` call still fires exactly once per vertex, at the top of each drain-loop iteration.

#### 13.1.4 Measured outcome

- **Dispatch-count regression:** the ON/OFF ratio on the reference regression (Section 13.4.2) dropped from `5.22×` to `0.27×`. Kahn's now dispatches fewer tasks than the level-based BFS, because level-BFS still pays `thread_count` dispatches per heavy level while Kahn's pays only shard-seeds plus a small number of spills.
- **Wall-clock regression on the large-design STA benchmark** (Section 13.4.5): the ~25% slowdown on full arrival propagation at 32 threads disappeared. On an isolated run, ON matched OFF within measurement noise (~2%).

Kahn's is not yet faster than level-BFS on the isolated post-CTS sweep, which was expected — the algorithmic advantage of Kahn's (no per-level barrier) only materializes when barriers dominate, which is not the case at this thread count on designs whose `visit()` body is the cost center. What the fix delivers is elimination of the overhead regression: Kahn's is safe to leave enabled by default.

### 13.2 Ref-pin-delay vertex orphaning in cleanup

#### 13.2.1 Observed symptom

No runtime symptom in any ORFS flow since no shipped regression exercises `set_input_delay -reference_pin`. Flagged by external review. Manifests as missed required-time propagation on any subsequent `invalidate + report_checks` after a Kahn's run that triggered `enqueueRefPinInputDelays`.

#### 13.2.2 Root cause

`ArrivalVisitor::visit()` on a clock-like vertex calls `enqueueRefPinInputDelays(ref_pin, sdc)`, which iterates `sdc->refPinInputDelays(ref_pin)` and calls `arrival_iter_->enqueue(target_vertex)` for each. That enqueue sets `bfsInQueue=true` on the target vertex and appends a pointer into `queue_[level]`. If the target was outside Kahn's precomputed active set (`in_deg[target_vid]` stays `-1`), the Stage 2 worker never visits it and never clears `bfsInQueue`.

The original post-Stage-2 cleanup was `queue_[level].clear()`. This dropped the pointer but left `bfsInQueue` stuck at `true` on the vertex. Every subsequent `enqueue()` on the same vertex short-circuited via the "already queued" check in `BfsIterator::enqueue`, silently suppressing its future propagation. A test that did `arrivals_invalid` and then `report_checks` produced reports missing paths through the ref-pin target.

#### 13.2.3 Fix: preserve-queued cleanup

`search/Bfs.cc` now has a helper `BfsIterator::dropProcessedEntries(first, last, to_level)` that walks each level's `queue_` entries, keeps those where `v->bfsInQueue(bfs_index_)` is still `true`, and drops the rest via erase-remove. It is called from both the `active_count==0` early-return and the post-Stage-2 cleanup. The invariant enforced is "after `visitParallel`, `queue_` contains exactly the vertices the caller can legitimately treat as queued" — matching the enqueue/dequeue contract the rest of `BfsIterator` relies on.

Ref-pin targets enqueued during Stage 2 with `bfsInQueue=true` now survive past cleanup and become seeds on the next call, so missed propagation is impossible.

#### 13.2.4 Verification

See `search/test/search_kahns_bfs_refpin.tcl` and Section 13.4.3.

### 13.3 Narrowed Kahn's discovery under clks_only

#### 13.3.1 Observed symptom

Runtime regression on the `findClkArrivals` path (exercised by `report_clock_skew`, `report_clock_latency`, and any command routed through `Sta::ensureClkArrivals`). Flagged by external review. Level-BFS stops at reg CK via `postponeClkFanouts` (see `ArrivalVisitor::visit` at `Search.cc:1179`: `if clks_only_ && vertex->isRegClk()` then postpone, else enqueue fanout). Kahn's Stage 1 discovery walked graph successors unconditionally, so it pre-discovered the entire data fanout past every register CK even on a clocks-only pass. Stage 2 then visited all of it, turning a narrow clock-arrival pass into broad propagation work.

#### 13.3.2 Fix: visitor-directed stop at reg CK

Added `VertexVisitor::stopDiscoveryAtRegClk()` (`include/sta/VertexVisitor.hh`), returning `false` by default. `ArrivalVisitor` overrides it to return `clks_only_`. `search/Bfs.cc` queries this once at the top of the Kahn's branch and captures it as `stop_at_reg_clk`. Two gates respect the flag:

- **Stage 1 discovery** (the `disc_idx` loop in `visitParallel`): if `stop_at_reg_clk && vertex->isRegClk()`, the loop skips the `kahnForEachSuccessor` recursion. The reg CK itself is still in `active_vertices` (it was added when its predecessor discovered it); only the successor walk is pruned.
- **Stage 2 worker lambda** (the `process` drain loop): same check before the successor-decrement `kahnForEachSuccessor` call. Redundant with the `in_deg[sid] >= 0` guard for correctness — successors past CK were never added, so every decrement would short-circuit — but it avoids iterating the edge list at all.

Kahn's still runs under `clks_only`; only the reg-CK fanout is excluded from discovery, matching the level-BFS active set exactly. Postpone semantics are unchanged: the reg CK vertex is visited, `ArrivalVisitor::visit` still calls `postponeClkFanouts`, and the next non-`clks_only` pass consumes `pending_clk_endpoints_` via `enqueuePendingClkFanouts`.

#### 13.3.3 Verification

See `search/test/search_kahns_bfs_clks_only.tcl` and Section 13.4.4.

### 13.4 Diagnostic harness

A small harness was added so both correctness and performance of this path can be checked quickly and deterministically. The pieces are independent and can be used in isolation.

#### 13.4.1 Tcl accessor: `sta::dispatch_call_count`

`DispatchQueue` carries a `std::atomic<uint64_t>` counter that increments on every `dispatch()` call. The counter is exposed via a Sta forwarder and a SWIG inline in `util/Util.i`:

```tcl
set before [sta::dispatch_call_count]
# ... some STA work ...
set dispatches [expr {[sta::dispatch_call_count] - $before}]
```

The counter is monotonic since `Sta` construction. There is no reset command — callers compute deltas between two reads. The counter is wall-clock-independent and therefore suitable for golden-file regressions in CI, unlike raw elapsed times.

**Accessor forwarding:**

- `Sta::dispatchCallCount()` (`search/Sta.cc`) → `DispatchQueue::dispatchCallCount()`
- Tcl command `sta::dispatch_call_count` (`util/Util.i`)

Header visibility is confined to `DispatchQueue.hh`; neither `StaState.hh` nor `Util.i` reaches directly into `DispatchQueue` internals.

A companion `Sta::arrivalVisitCount()` accessor exposes `BfsFwdIterator::visitCountCumulative()` as `sta::arrival_visit_count`, tracking how many vertices the arrival BFS has visited. It is the correct observable for Section 13.4.4 (where dispatch count is blind to the regression).

#### 13.4.2 Regression: `search/test/search_kahns_bfs_dispatch.tcl`

Captures dispatch count during a `report_checks` (full propagation) sweep on the bundled `gcd_sky130hd` example, once with `sta_use_kahns_bfs=0` and once with `=1`. `thread_count=2` is fixed so the golden is portable. Prints `off_dispatches`, `on_dispatches`, and `on_to_off_ratio`. A future change to either BFS path shifts these numbers; the failing diff forces conscious review. Typical healthy values under the current fix are ON << OFF.

Invocation from `sta/search/test/`:

```
./regression search_kahns_bfs_dispatch
```

#### 13.4.3 Regression: `search/test/search_kahns_bfs_refpin.tcl`

Covers the ref-pin-delay orphaning fix (Section 13.2). Adds `set_input_delay -reference_pin [get_ports clk]` on a test design so that clock propagation fires `enqueueRefPinInputDelays` during Stage 2. Runs `report_checks` OFF (baseline), then Kahn's ON (triggers the cleanup-preservation path), then `arrivals_invalid + report_checks` OFF (the observation point). Asserts the two OFF reports are byte-identical and prints `refpin_preservation=ok`. A regression that reintroduces the blind `queue_[level].clear()` makes the second report miss the ref-pin path and the diff fires.

Invocation:

```
./regression search_kahns_bfs_refpin
```

#### 13.4.4 Regression: `search/test/search_kahns_bfs_clks_only.tcl`

Covers the narrowed-discovery fix (Section 13.3). Runs `report_clock_skew` (which routes through `clkSkewPreamble` → `ensureClkArrivals` → `findClkArrivals` with `clks_only=true`) on `gcd_sky130hd`, captures the arrival-BFS cumulative visit count with Kahn's OFF and ON, and prints `clks_only_off_visits`, `clks_only_on_visits`, `clks_only_on_to_off_ratio`.

Visit count — not dispatch count — is the correct observable for this regression: the wider Kahn's walk fills per-worker batches but rarely crosses the 64-vertex spill threshold on small designs, so the dispatch counter is blind to it. Every Kahn's-discovered vertex is visited exactly once in Stage 2, so the cumulative visit count tracks active-set size one-to-one.

Under the fix the ratio is close to parity (Kahn's discovers the same narrowed set as level-BFS). A regression that removes the stop makes Kahn's visit every data-path vertex downstream of reg CK and the ON count jumps sharply.

Invocation:

```
./regression search_kahns_bfs_clks_only
```

#### 13.4.5 Isolated STA wall-clock harness: `flow/util/kahns_sta_isolated.tcl`

For pinpointing STA-level wall-clock regressions without running ORFS steps around them, a generic Tcl harness loads a post-step ORFS database, applies the liberty set (globbed from platform/objects dirs), and times two `report_checks` passes — full cold arrival+required, then incremental required only — under a caller-supplied Kahn's mode. Required Tcl vars are `kahn_mode` and `kahn_design_path`; `kahn_step` defaults to `4_cts`. A bash wrapper `flow/util/kahns_sta_repeat.sh` runs N iterations per mode, parses the `STA_MS full/incr` lines, and reports median/min/max/stddev so small deltas can be distinguished from measurement noise. Modes `0`, `1`, or `both` are selectable via `-m`.

Invocation from `flow/`:

```
util/kahns_sta_repeat.sh -d PLATFORM/DESIGN/VARIANT [-s step] [-m mode] [-n N] [-t threads]
```

Two separate openroad invocations are used per iteration, deliberately, to avoid any stale-arrival or process-cached state from contaminating the comparison.

**Per-run output.** Each invocation prints the following lines so that timing, dispatch count, and visit count can be compared across OFF and ON without opening another tool:

```
=== design=... step=... kahn_mode=N (sta_use_kahns_bfs=N) visit_skip=N threads=N libs=N ===
STA_MS full:   <ms>
DISPATCH full: <DispatchQueue::dispatch() delta during -path_delay max>
VISITS full:   <arrival BFS visit delta during -path_delay max>
STA_MS incr:   <ms>
DISPATCH incr: <dispatch delta during -path_delay min>
VISITS incr:   <visit delta during -path_delay min>
```

`DISPATCH` and `VISITS` come from the Section 13.4.1 counters.

#### 13.4.6 VTune hotspot harness: `flow/util/kahns_vtune.sh`

For attributing time to specific C++ functions, a wrapper runs VTune hotspot collection twice — once with Kahn's OFF, once with ON — against the same isolated Tcl harness (13.4.5). It uses VTune's `-start-paused` option together with the collector's `-command resume/pause` to gate sampling to only the `report_checks` windows, so liberty parsing, `read_db`, and `link_design` do not pollute the profile. Both result directories (`off/` and `on/`) are written under a timestamped parent so `vtune -report hotspots` can directly compare the two algorithms on the same propagation workload.

Invocation from `flow/`:

```
util/kahns_vtune.sh -d PLATFORM/DESIGN/VARIANT [-s step] [-m mode] [-t threads] [-o outdir]
```

**Output layout:**

```
<outdir>/off/     vtune result dir, Kahn's OFF
<outdir>/off.log  full openroad stdout/stderr (with STA_MS / DISPATCH / VISITS lines)
<outdir>/off.tcl  generated wrapper Tcl (reproducible)
<outdir>/on/      vtune result dir, Kahn's ON
<outdir>/on.log
<outdir>/on.tcl
```

**Resume/pause mechanism.** `kahns_sta_isolated.tcl` reads the env variables `VTUNE_RESULT_DIR` and `VTUNE` that `kahns_vtune.sh` sets. When they are present, Tcl procs `vtune_resume` / `vtune_pause` shell out `vtune -command resume -r $VTUNE_RESULT_DIR` and `vtune -command pause -r ...`. These bracket each `report_checks` call so the captured profile contains only STA propagation work. When `VTUNE_RESULT_DIR` is empty (direct Tcl invocation, or the `kahns_sta_repeat.sh` path), the procs no-op and the harness still runs standalone. No change to openroad's build is required; the gating uses VTune's own CLI to signal the collector from inside the profiled process.

**Comparing results:**

```
vtune -report hotspots -result-dir <outdir>/off -format=csv > off.csv
vtune -report hotspots -result-dir <outdir>/on  -format=csv > on.csv
```

For a side-by-side of the top functions:

```
paste <(head -21 off.csv) <(head -21 on.csv) | column -t -s $'\t' | less -S
```

Filtering out VTune's own symbol-resolution overhead (`libdwarf`, `libpindwarf`, `libc-dynamic`) makes the STA-relevant hotspots stand out:

```
awk -F, '$6 !~ /dwarf|pindwarf|libc-dynamic/' off.csv | head -20 > off.stripped
awk -F, '$6 !~ /dwarf|pindwarf|libc-dynamic/' on.csv  | head -20 > on.stripped
paste off.stripped on.stripped | column -t -s $'\t' | less -S
```

**Expected signatures under the current fix.** `pthread_mutex_lock/unlock` CPU time drops measurably in ON — this is the dispatch-contention fix visible at the profiler level. `VertexOutEdgeIterator::next` and `VertexInEdgeIterator::next` increase slightly in ON — Kahn's walks each edge twice (Stage 1 discovery plus Stage 2 decrement) while level-BFS walks each edge once. These two shifts roughly cancel on designs where neither barrier nor dispatch dominates; see Section 13.1.4.

**Permission note.** VTune's SQLite layer wants write access to `<outdir>/off/sqlite-db` and `on/sqlite-db` even in `-report` mode. If the collection ran under a different uid (e.g. inside a container) and the current user sees "Insufficient permissions" on report, either `chown -R` the result dir to the current user, or `cp -a` it into `/tmp/` before running `-report`.

#### 13.4.7 Opt-in visit-skip regression: `search/test/search_kahns_bfs_visit_skip.tcl`

Correctness regression for the predecessor-based visit-skip optimization (Section 12 item 1, available today via `sta_kahn_visit_skip`). Runs `report_checks` under Kahn's with `sta_kahn_visit_skip=0` and `=1` and asserts the reports are byte-identical — catches any bug in the skip mechanism that changes arrivals, slacks, or path ordering.

Effectiveness measurement (visit-count drop) requires selective invalidation, which is only reachable from real ORFS flows (`rsz`, `cts`) or a C++ unit test, not from the `sta::arrivals_invalid` Tcl command. For those measurements the isolated STA harness accepts a `kahn_visit_skip` Tcl variable so wrapper scripts can A/B it alongside `kahn_mode`.

Invocation:

```
./regression search_kahns_bfs_visit_skip
```

### 13.5 Opt-in predecessor-based visit-skip

A prototype of Section 12 item 1 (visit-level change short-circuit) is available today behind the Tcl variable `sta_kahn_visit_skip` (default off). When on, Kahn's Stage 2 skips the visitor body on any vertex whose active-set predecessors all reported `arrivals_changed=false` via `VertexVisitor::lastVisitChanged()`. The decrement walk over successors still runs so Kahn's in-degree invariant holds; only the expensive visitor call (arrival/tag computation) is elided.

#### 13.5.1 Mechanism

A new atomic byte array lives in `KahnState` alongside `in_degree`:

```cpp
std::unique_ptr<std::atomic<uint8_t>[]> pred_changed;
size_t pred_changed_size = 0;
```

It is allocated only when `visit_skip` is on — zero-cost when the flag is off. Initialization at the top of Stage 2 sets `pred_changed[vid] = 1` for every seed (in-degree 0) and `0` for every other active vertex. Seeds are always visited; non-seeds start skippable and flip to non-skippable as soon as any predecessor reports a change.

The worker lambda's drain loop consults the flag once per pop and propagates on the successor side:

```cpp
// at pop time
bool changed = true;
const bool skip = visit_skip
    && pred_changed[graph_->id(vertex)]
           .load(std::memory_order_acquire) == 0;
if (skip) {
  changed = false;
} else {
  visitors[tid]->visit(vertex);
  total_visited.fetch_add(1, std::memory_order_relaxed);
  if (visit_skip)
    changed = visitors[tid]->lastVisitChanged();
}
// ... in kahnForEachSuccessor callback:
if (visit_skip && changed
    && pred_changed[sid].load(std::memory_order_relaxed) == 0)
  pred_changed[sid].store(1, std::memory_order_release);
int prev = in_degree[sid].fetch_sub(1, std::memory_order_acq_rel);
```

The relaxed-load guard before the release-store elides the redundant writes that high-fan-in vertices would otherwise cause — when N predecessors all see `changed=true`, only the first actually writes `pred_changed[sid]` to 1, the rest see it already set and skip the store. This avoids cache-line bouncing on the shared byte.

#### 13.5.2 Correctness argument

**Invariant preserved.** The decrement of `in_degree[sid]` always runs, regardless of whether we skipped the current vertex's visitor. Every active predecessor therefore decrements its successors' counters exactly once, so every active vertex reaches `in_degree == 0` and enters the batch. Kahn's contract is intact; the skip only short-circuits the *visitor call*, not the scheduling step.

**Sufficient but not necessary predicate.** Skipping is safe whenever no predecessor's arrival actually changed, because STA arrivals depend monotonically on predecessor arrivals plus (unchanged) edge delays. The converse is not guaranteed — we may visit a vertex whose arrivals wouldn't have changed, because the predecessor-changed flag is a conservative over-approximation. Worst case is identical to today; the optimization only *adds* skips, never incorrect visits.

**Memory ordering.** The `pred_changed[sid].store(1, release)` happens before the same thread's `in_degree[sid].fetch_sub(1, acq_rel)`. The decrementer-to-zero observes its fetch_sub as the last in the total modification order, so its acquire synchronizes with every prior thread's release — including the pred_changed store. The subsequent `pred_changed[vid].load(acquire)` at pop time therefore sees all predecessors' signals. Explicit release/acquire on pred_changed is slightly stronger than strictly necessary (the adjacent `fetch_sub(acq_rel)` on in_degree carries the piggyback synchronization), but it makes intent self-documenting and costs nothing measurable.

**Seed handling.** Seeds initialize to `pred_changed = 1` so they always run through the visitor. This is load-bearing: if seeds were skippable, we'd never verify whether the cached arrival on a dirty seed is still correct, and the whole downstream would silently skip with a stale cache.

#### 13.5.3 Why opt-in and not default

`Search::arrivalsChanged(vertex, tag_bldr)` compares the newly-computed tag_bldr against the vertex's existing `paths()`. `sta::arrivals_invalid` calls `Search::arrivalsInvalid()` which `deletePaths()`s everything — the comparison then takes the `paths == nullptr` branch and always returns true. From pure Tcl this means the skip never fires and the optimization is pure overhead.

The real win is on **selective invalidation**: `Search::arrivalInvalid(Vertex*)` (singular) adds a vertex to `invalid_arrivals_` without wiping paths. This is what OpenROAD's `rsz`, `cts`, and other incremental flows trigger when they resize or insert cells. There, cached arrivals survive, `arrivalsChanged` can legitimately return false, and visit-skip short-circuits the redundant visitor work.

Until the optimization has been soak-tested in those real flows, default-off is safer. Users who want to A/B it can flip the Tcl variable per session; the plumbing and regression tests are in place to support that without risking existing flows.

#### 13.5.4 Files touched

The implementation mirrors the existing `sta_use_kahns_bfs` plumbing across eleven files:

- `include/sta/VertexVisitor.hh` — new virtual `lastVisitChanged()` default `true`.
- `include/sta/Search.hh` — `ArrivalVisitor` override; new member `last_arrivals_changed_`.
- `search/Search.cc` — one line in `ArrivalVisitor::visit` storing the existing `arrivals_changed` result.
- `include/sta/Variables.hh` — `use_kahns_visit_skip_{false}` + getter + setter decl.
- `sdc/Variables.cc` — setter definition.
- `include/sta/Sta.hh` — `Sta::useKahnsVisitSkip/setUseKahnsVisitSkip` decls.
- `search/Sta.cc` — forwarder definitions.
- `search/Search.i` — SWIG inlines `use_kahns_visit_skip` / `set_use_kahns_visit_skip`.
- `tcl/Variables.tcl` — Tcl variable trace for `sta_kahn_visit_skip`.
- `search/Bfs.cc` — `KahnState` gains `pred_changed` + `ensurePredChangedSize`; `visitParallel` allocates and initializes on the opt-in path; worker lambda reads/writes the flag as above.
- `search/test/CMakeLists.txt` — registers the new regression below.

#### 13.5.5 Verification

`search/test/search_kahns_bfs_visit_skip.tcl` is a **correctness** regression: it runs the same `report_checks` under Kahn's with `sta_kahn_visit_skip=0` and `=1` and asserts the reports are byte-identical. Any bug in the pred_changed plumbing that changes arrivals, slacks, or path ordering will make the two reports diverge.

The test deliberately cannot exercise *effectiveness* — `sta::arrivals_invalid` wipes the path cache, so the skip never fires in this harness. That's fine: the test's job is to guard the shipping-critical invariant (the skip never corrupts output), not to measure wins on incremental flows. Effectiveness measurement lives outside Tcl:

- Run a real ORFS flow (`rsz`, `cts`) with `set sta_kahn_visit_skip 1` in the injected Tcl and compare `DISPATCH` / `VISITS` counters against the skip-off baseline.
- Or write a C++ unit test that calls `Search::arrivalInvalid(vertex)` on a chosen subset of a small graph and verifies the skip fires on the expected vertices.

#### 13.5.6 How to A/B on a real design

The isolated STA harness accepts a `kahn_visit_skip` Tcl variable that maps directly to `sta_kahn_visit_skip`. Set it alongside `kahn_mode` when sourcing the harness. See the wrappers under `flow/util/` for the command-line interface.

### 13.6 Summary of diagnostic coverage

| Section | Tool | Catches |
| --- | --- | --- |
| 13.4.1 | `sta::dispatch_call_count`, `sta::arrival_visit_count` | Raw counters for ad-hoc scripts |
| 13.4.2 | `search_kahns_bfs_dispatch.tcl` | Full-propagation dispatch count — general overhead regression |
| 13.4.3 | `search_kahns_bfs_refpin.tcl` | Ref-pin-delay cleanup invariant |
| 13.4.4 | `search_kahns_bfs_clks_only.tcl` | Narrowed-discovery invariant |
| 13.4.5 | `kahns_sta_repeat.sh` | Wall-clock on real designs at full thread counts |
| 13.4.6 | `kahns_vtune.sh` | Function-level attribution of residual cost structure |
| 13.4.7 | `search_kahns_bfs_visit_skip.tcl` | Correctness of the opt-in visit-skip optimization |

### 13.7 What remains

Kahn's at 32 threads on a large design is now at parity with level-BFS on the STA-isolated benchmark. Real algorithmic wins from barrier elimination are still expected on designs where level populations are highly uneven — the profile that originally motivated Kahn's — but measuring them requires designs larger than the reference corpus here. Future work in that direction belongs under Section 12, particularly items 1 (visit-level change short-circuit, prototyped behind `sta_kahn_visit_skip` and documented in Section 13.5) and 3 (demand-driven forward propagation), which reduce the `visit()` cost that currently dominates on the designs tested.
