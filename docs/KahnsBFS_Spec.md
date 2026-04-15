# Kahn's Algorithm BFS for OpenSTA: Functional Specification

## 1. Motivation

OpenSTA's `BfsIterator::visitParallel()` processes the timing graph level by level, inserting a thread barrier (`dispatch_queue_->finishTasks()`) between every level. When a level contains few vertices, most threads sit idle at the barrier while waiting for the rest of the level to complete. This is the dominant source of parallel inefficiency in timing analysis for real designs with uneven level populations.

Kahn's algorithm removes the per-level barrier by processing each vertex as soon as all its predecessors are complete, allowing vertices at different levels to execute concurrently.

## 2. Functional Specification

### 2.1 Toggle and Predicate Setup

Kahn's is controlled by two settings on each `BfsIterator`:

```cpp
// 1. Provide the edge filter Kahn's uses for discovery.
//    This is separate from search_pred_ used by the original BFS.
iterator->setKahnPred(search_adj);

// 2. Enable Kahn's for visitParallel.
iterator->setUseKahns(true);
```

Both are required. If `kahn_pred_` is null when `use_kahns_` is true, the iterator falls back to the original level-based BFS silently.

Default for both is off/null (original behavior). The toggle affects only `visitParallel()`; the sequential `visit()`, `hasNext()`/`next()`, and `enqueue()` APIs are unchanged.

### 2.2 Why Kahn's Needs Its Own Predicate

In the original BFS, edge filtering happens **inside the visitor** at call time:

```
visitor->visit(vertex)
  └─ enqueueAdjacentVertices(vertex, adj_pred_)   ← visitor provides the filter
```

The BFS iterator itself never decides which edges to follow -- the visitor does, one vertex at a time.

Kahn's algorithm cannot work this way. It must discover the **entire active subgraph upfront** (before any visitor runs) to compute in-degrees. This discovery needs an edge filter to know which edges to follow. The iterator's own `search_pred_` is often null (the arrival iterator is constructed with `nullptr` because the visitor was always expected to provide the filter).

`kahn_pred_` solves this by giving Kahn's its own dedicated filter, set once during construction, without changing how `search_pred_` or the visitor works.

In practice, `Search.cc` wires it up at construction:

```cpp
// Search constructor:
arrival_iter_->setKahnPred(search_adj_);
required_iter_->setKahnPred(search_adj_);
```

### 2.3 Behavioral Contract

When enabled, `visitParallel(to_level, visitor)` must:

1. Visit exactly the same set of vertices as the original level-based BFS.
2. Visit each vertex exactly once.
3. Visit every vertex only after all its predecessors (in the BFS-direction DAG) have been visited and their results are visible.
4. Call `visitor->visit(vertex)` in a thread-safe manner (one thread per vertex, thread-local visitor copies).
5. Call `visitor->levelFinished()` once after all vertices are processed.
6. Leave the BFS queue in a consistent state (processed levels cleared, remaining levels tracked).
7. Respect the `to_level` bound -- vertices beyond it remain queued for future calls.

### 2.4 Scope

The implementation is integrated into the existing `BfsIterator` class hierarchy:

- `BfsFwdIterator` -- forward arrival propagation (out-edges)
- `BfsBkwdIterator` -- backward required-time propagation (in-edges)

Both directions are supported through the polymorphic `kahnForEachSuccessor()` virtual method.

## 3. Why the Graph Is a DAG

Kahn's algorithm requires a directed acyclic graph. Within a single `visitParallel()` call, the active graph is guaranteed acyclic because:

| Cycle Source | How It's Broken | Where |
|---|---|---|
| Flip-flop feedback (Q -> ... -> D) | D inputs are timing endpoints; clk-to-Q starts new propagation. `SearchAdj` skips `latchDtoQ` and timing-check edges. | `Search.cc:127-131`, `Search.cc:178-186` |
| Latch D-to-Q | Explicitly excluded by `SearchThru::searchThru()` and `SearchAdj::searchThru()`. Convergence handled by multi-pass outer loop in `Search::findAllArrivals()`. | `Search.cc:130`, `Search.cc:1004-1012` |
| Combinational loops | Levelizer DFS detects back edges and marks them `isDisabledLoop()`. All BFS predicates skip disabled-loop edges. | `Levelize.cc:232-330`, `Levelize.cc:428-446` |

## 4. Algorithm

### 4.1 Overview

```
visitParallel(to_level, visitor):
  if thread_count == 1        → sequential visit() (unchanged)
  if !use_kahns_ || !kahn_pred_ → original level-based parallel BFS (unchanged)
  else                        → Kahn's three-phase algorithm:

  Phase 1+2: Discovery + In-Degree Counting  (single-threaded)
  Phase 3:   Batch-Dispatch Parallel Traversal (multi-threaded)
```

### 4.2 Phase 1+2: Discovery + In-Degree Counting

Single-threaded BFS from seed vertices (those already in the level queue). For each vertex discovered:

1. Assign in-degree = 0 for seeds, in-degree = count of active predecessors for discovered successors.
2. Record vertex in the active set.
3. Set `bfsInQueue` flag to prevent `enqueue()` from re-adding during Phase 3.

Data structures:
- `in_degree_init`: flat `std::vector<int>` indexed by `graph_->id(vertex)`. Value -1 = not active, >= 0 = in-degree count. Grows dynamically if vertex IDs exceed initial capacity (see Section 6.1).
- `active_vertices`: list of all discovered vertices for iteration.
- Both are persistent across calls via `KahnState` to avoid re-allocation (see Section 5.4).

The discovery uses `kahn_pred_` -- the same `SearchAdj` filter used by the arrival and required paths -- ensuring identical edge filtering to the original BFS.

### 4.3 Phase 3: Batch-Dispatch Parallel Traversal

```
ready_batch = {vertices with in_degree == 0}

while ready_batch is not empty:
    next_ready = {}

    if batch is small (< thread_count):
        process single-threaded
    else:
        for each vertex in ready_batch:
            dispatch_queue_->dispatch(lambda(tid):
                visitor_copy[tid]->visit(vertex)
                for each successor of vertex:
                    atomic decrement in_degree[successor]
                    if in_degree reached 0:
                        lock; next_ready.push_back(successor)
            )
        dispatch_queue_->finishTasks()

    ready_batch.swap(next_ready)
```

Key properties:
- One task dispatched per vertex -- `DispatchQueue` handles load balancing across its thread pool.
- `finishTasks()` uses condition_variable internally (no spin-wait).
- Successor in-degree decrements use `std::memory_order_acq_rel` to ensure predecessor writes are visible.
- Newly-ready vertices are collected into `next_ready` under a mutex, then swapped into the next batch.

### 4.4 Cleanup

After traversal:
- Processed levels are cleared from the `LevelQueue`.
- `first_level_` / `last_level_` are recalculated via `resetLevelBounds()` to track any remaining queued vertices (e.g., those beyond `to_level`).
- Active vertex IDs are saved in `KahnState::prev_ids` for efficient reset on the next call.

## 5. Implementation Details

### 5.1 Files Modified

| File | Changes |
|---|---|
| `include/sta/Bfs.hh` | Added `<functional>`, `<memory>`. Added `kahnForEachSuccessor` pure virtual, `resetLevelBounds`, `KahnState` forward decl + `unique_ptr` member, `use_kahns_` flag, `kahn_pred_` pointer, and public accessors. |
| `search/Bfs.cc` | Added `<atomic>`. Defined `KahnState` struct. Rewrote `visitParallel` with three branches (sequential / level-based / Kahn's). Added `kahnForEachSuccessor` overrides for Fwd (out-edges) and Bkwd (in-edges). Added `resetLevelBounds`. |
| `search/Search.cc` | Added two lines in `Search` constructor to wire up `kahn_pred_` on both arrival and required iterators: `arrival_iter_->setKahnPred(search_adj_)` and `required_iter_->setKahnPred(search_adj_)`. |

### 5.2 Class Hierarchy

```
BfsIterator (base)
  ├─ kahnForEachSuccessor() = 0  [pure virtual]
  ├─ resetLevelBounds()
  ├─ KahnState (persistent arrays, pimpl)
  ├─ use_kahns_ flag
  ├─ kahn_pred_ (SearchPred* for Kahn's discovery, separate from search_pred_)
  │
  ├─ BfsFwdIterator
  │    └─ kahnForEachSuccessor: iterates out-edges with
  │       searchFrom/searchThru/searchTo via kahn_pred_
  │
  └─ BfsBkwdIterator
       └─ kahnForEachSuccessor: iterates in-edges with
          searchTo/searchThru/searchFrom via kahn_pred_
```

### 5.3 KahnState (Persistent Storage)

```cpp
struct BfsIterator::KahnState {
  std::vector<int> in_degree_init;              // flat array, indexed by VertexId
  std::unique_ptr<std::atomic<int>[]> in_degree; // atomic copy for parallel phase
  size_t in_degree_size = 0;
  std::vector<VertexId> prev_ids;               // IDs to reset on next call
};
```

- Allocated lazily on first Kahn's call.
- `in_degree_init` grows dynamically (never shrinks). Only touched entries are reset between calls via `prev_ids`.
- `in_degree` (atomic array) is reallocated only when the max vertex ID grows.

### 5.4 Memory Ordering

| Operation | Ordering | Rationale |
|---|---|---|
| `in_degree_init` writes (discovery) | Non-atomic | Single-threaded phase; `dispatch()` provides happens-before. |
| `in_degree[].store()` (setup) | `relaxed` | Single-threaded; `dispatch()` provides happens-before. |
| `in_degree[].fetch_sub()` (worker) | `acq_rel` | Last predecessor's decrement synchronizes all prior arrival writes to the successor's reader thread. |
| `total_visited.fetch_add()` | `relaxed` | Counter read only after `finishTasks()` barrier. |

### 5.5 Interaction with enqueue() During visit()

`ArrivalVisitor::visit()` calls `enqueueAdjacentVertices()` which calls `enqueue()`. During Kahn's:
- Active vertices have `bfsInQueue` set during discovery -> `enqueue()` is a no-op (flag already set).
- Vertices beyond `to_level` are not in the active set -> `enqueue()` adds them to the `LevelQueue` normally for future passes.

### 5.6 kahnForEachSuccessor vs enqueueAdjacentVertices

These two methods have nearly identical edge-iteration logic (same predicates, same edge direction). They are intentionally kept as separate methods because:
- `enqueueAdjacentVertices` is called millions of times in the non-Kahn's path. Wrapping it in `std::function` would add overhead to all BFS operations.
- `kahnForEachSuccessor` accepts a `std::function<void(Vertex*)>` callback, used in both discovery and the worker. The `std::function` overhead is negligible relative to per-vertex computation.

## 6. Pitfalls and Bugs Found

### 6.1 ObjectTable Block-Based Vertex IDs

**Problem**: `graph_->vertexCount()` returns the **live** object count, but `graph_->id(vertex)` returns `(block_index << 7) + slot_index`. After vertex deletions, live count drops but blocks persist. A vertex in block 2 can have ID 260 even when only 79 vertices are alive.

**How we found it**: The `rmp.gcd_restructure.tcl` OpenROAD test crashed with a segfault. The rmp module deletes cells during restructuring, creating gaps between live vertex count and max vertex ID. Our in-degree array was sized to `vertexCount() + 1` and accessed out of bounds.

**Solution**: `in_degree_init` grows dynamically during discovery (`resize(vid + 128, -1)` when any ID exceeds current capacity). Worker lambdas include a bounds check (`sid < in_deg_size`). The atomic array is sized to `max_id + 1` after discovery completes.

**Note**: The other developer's implementation has the same latent bug (`graph_->vertexCount() + 1` sizing) but it hasn't manifested because their code only runs on the delay-calc path, which doesn't encounter the deletion pattern that rmp triggers.

### 6.2 Null Search Predicate on Arrival Iterator

**Problem**: The arrival BFS iterator is constructed with `search_pred_ = nullptr`:

```cpp
arrival_iter_(new BfsFwdIterator(BfsIndex::arrival, nullptr, this))
```

This is intentional in the original BFS -- the visitor provides its own predicate (`adj_pred_`) at call time via `enqueueAdjacentVertices(vertex, adj_pred_)`. The null `search_pred_` is never dereferenced.

Kahn's discovery phase needs a predicate upfront (before any visitor runs) to know which edges to follow. Using `search_pred_` directly caused a null pointer dereference.

**How we found it**: The `rmp.gcd_restructure.tcl` test crashed in `kahnForEachSuccessor` with `pred->searchFrom(vertex)` dereferencing null. Stack trace showed the call came from arrival propagation via `Search::findArrivals1`.

**Solution**: Introduced `kahn_pred_` -- a separate predicate pointer dedicated to Kahn's discovery and successor decrement. Set via `setKahnPred()` and wired up in the `Search` constructor:

```cpp
arrival_iter_->setKahnPred(search_adj_);
required_iter_->setKahnPred(search_adj_);
```

If `kahn_pred_` is null when Kahn's is enabled, `visitParallel` falls back to the original level-based BFS. This ensures no crash even if a caller enables Kahn's without setting the predicate.

### 6.3 Memory Visibility Across Threads

**Problem**: When predecessor P finishes computing arrivals and successor S starts reading them, S must see P's writes.

**Original BFS**: `finishTasks()` between levels provides a full memory fence.

**Kahn's**: The `fetch_sub(1, memory_order_acq_rel)` on the in-degree counter creates the happens-before chain. When the last predecessor's decrement triggers S's readiness, all prior writes by all predecessors are visible to S's processing thread. The batch-dispatch model adds a `finishTasks()` barrier between batches as an additional fence.

### 6.4 "Arrivals Unchanged" Optimization

**Original BFS**: If `ArrivalVisitor::visit()` finds arrivals haven't changed, it skips `enqueueAdjacentVertices` -- fanout is not re-evaluated.

**Kahn's**: The discovery phase conservatively discovers ALL reachable vertices. Fanout in-degrees are decremented unconditionally after visit, regardless of whether arrivals changed. This means some vertices may be visited unnecessarily. They will find no change and produce no further effect.

**Impact**: Slightly more work for incremental updates where only a small subset changes. Correct for all cases.

### 6.5 Latch Multi-Pass Convergence

Latch D-to-Q edges are excluded from the BFS by search predicates. Latch convergence is handled by the outer multi-pass loop in `Search::findAllArrivals()` / `Search::findFilteredArrivals()`, which re-seeds latch Q outputs between `visitParallel` calls. Kahn's operates within a single `visitParallel` call and is orthogonal to this mechanism.

### 6.6 levelFinished() Callback

`VertexVisitor::levelFinished()` is a virtual hook called at level boundaries. No override exists in the codebase (base implementation is empty). With Kahn's, it is called once after all vertices are processed. If a future subclass relies on per-level callbacks, it would need adaptation.

## 7. Comparison with Other Developer's Approach

The other implementation (`BfsFwdInDegreeIterator`) in the alternate repository takes a different design approach. Key differences:

### 7.1 Architecture

| Aspect | Other Developer | Our Approach |
|---|---|---|
| Class design | New standalone `BfsFwdInDegreeIterator : StaState` | Integrated into existing `BfsIterator` with toggle |
| Scope | Forward-only, delay calc (`GraphDelayCalc`) only | Forward + backward, any `visitParallel` caller |
| Integration | Requires caller to use new class and call `computeInDegrees()` explicitly | Drop-in: `setKahnPred(pred)` + `setUseKahns(true)` on existing iterator |

### 7.2 Discovery

| Aspect | Other Developer | Our Approach |
|---|---|---|
| Strategy | Iterate ALL vertices + ALL edges (full graph) | BFS from seeds (active subgraph only) |
| Cost | O(V_total + E_total) every call | O(V_active + E_active) per call |
| Incremental variant | Yes (`computeInDegrees(invalid_delays)` with reachability pass) | Natural (seed-based discovery covers only dirty subgraph) |
| Loop breaking | `to_vertex->level() >= vertex->level()` (ad-hoc) | `SearchPred::searchThru()` (matches Levelizer exactly) |

### 7.3 Parallelism

| Aspect | Other Developer | Our Approach |
|---|---|---|
| Dispatch model | Batch: dispatch all ready -> `finishTasks()` -> next batch | Same (adopted from their approach -- see Section 7.6) |
| Ready queue | `std::vector<Vertex*>` with `swap()` | Same |
| Newly-ready collection | `ready_lock_` mutex on `ready_` vector | `next_ready_lock` mutex on `next_ready` vector |

### 7.4 Thread Safety

| Aspect | Other Developer | Our Approach |
|---|---|---|
| Active-set check | `vertex->visited()` (non-atomic `bool`) -- **data race risk** | `in_degree_init[id] >= 0` (read-only during parallel phase) -- safe |
| Edge dedup | `std::set<Edge*> processed_edges_` with **per-edge mutex lock** -- serialization bottleneck | Not needed (in-degrees computed correctly upfront) |
| Vertex marking | `vertex->setVisited(true)` from worker threads | `vertex->setBfsInQueue()` (atomic field) |

### 7.5 Array Sizing

| Aspect | Other Developer | Our Approach |
|---|---|---|
| Size | `graph_->vertexCount() + 1` (fixed) | Dynamic growth during discovery + bounds checks |
| Risk | **Same ObjectTable ID bug** -- IDs can exceed vertexCount after deletions. Latent bug that hasn't manifested because their code path (delay calc) doesn't trigger the deletion pattern. | Fixed (see Section 6.1) |

### 7.6 What We Adopted from Their Approach

Our initial implementation used a custom `KahnReadyQueue` with spin-wait workers (`std::this_thread::yield()`). Comparing with their approach revealed that dispatching one task per vertex into `DispatchQueue` and using `finishTasks()` as a batch barrier is significantly more efficient:

- `DispatchQueue` uses `condition_variable` for blocking -- no wasted CPU on spin-wait.
- Natural load balancing -- the thread pool picks up work items automatically.
- Simpler code -- no custom queue class needed.

This change cut our test suite overhead from 87s to 28s (vs 27s for original BFS).

### 7.7 Performance Comparison

| Approach | STA Regression (6109 tests) |
|---|---|
| Original level-based BFS | 27-30s |
| Our Kahn's v1 (hash map + spin-wait) | 87s |
| Our Kahn's v2 (dense array + spin-wait) | 42s |
| Our Kahn's v3 (dense array + batch dispatch) | 28s |
| Other developer (delay-calc only, separate class) | Reports ~45% speedup on large designs |

## 8. Test Plan and Results

### 8.1 OpenSTA Standalone Regression

```bash
cd tools/OpenROAD/src/sta/build
cmake .. && make -j$(nproc)
ctest -j$(nproc)
```

**Pass criteria**: All tests pass with `use_kahns_ = true`. Results must be bit-identical to `use_kahns_ = false`.

**Result**: **PASS** -- 6109/6109 tests pass with both settings.

### 8.2 OpenROAD Full Regression

```bash
cd tools/OpenROAD/build
cmake --build . -j$(nproc)
ctest -j$(nproc)
```

**Pass criteria**: All OpenROAD tests pass, including flows that modify the netlist between timing updates.

**Key test cases exercised**:
- `rmp.gcd_restructure.tcl` -- restructure deletes cells, causing vertex ID gaps. This test originally crashed (Section 6.1, 6.2) and drove two bug fixes (dynamic array sizing and `kahn_pred_` separation).
- `rsz.*` -- resizer modifies netlist incrementally between timing updates.
- `cts.*` -- clock tree synthesis adds buffers and triggers re-timing.

**Result**: **PASS** -- all OpenROAD regressions pass after the two fixes.

### 8.3 Thread Count Sweep

```tcl
set_thread_count 1  ;# Falls back to sequential visit()
set_thread_count 2
set_thread_count 4
set_thread_count 8
```

**Pass criteria**: Identical timing reports across all thread counts.

### 8.4 Toggle Consistency

```tcl
# Run 1: use_kahns_ = false
report_checks -digits 6 > results_original.rpt
# Run 2: use_kahns_ = true
report_checks -digits 6 > results_kahns.rpt
# diff results_original.rpt results_kahns.rpt
```

**Pass criteria**: Reports are identical.

### 8.5 Performance Expectations

| Scenario | Expectation |
|---|---|
| Small designs (< 10K vertices) | Kahn's within 10% of original (discovery overhead amortized) |
| Large designs (> 100K vertices) with uneven levels | Kahn's faster due to barrier elimination |
| Incremental updates (small active set) | Kahn's overhead proportional to active set, not total graph |
| High thread counts (8-16 threads) | Kahn's scales better (no idle threads at level barriers) |

### 8.6 Stress Tests

- Design with a single long chain (worst case -- no parallelism, discovery overhead for zero benefit).
- Design with many parallel chains (best case -- maximum parallel utilization).
- Design with latches (multi-pass convergence must work correctly).
- Rapid incremental updates (persistent KahnState reuse exercised).
- Netlist modification flows: rmp, rsz, cts (exercises ObjectTable ID gaps and graph rebuilds).
