# Tags, Scenes, and Timing Propagation in OpenSTA (Scene-based fork)

This document describes how `Tag`/`ClkInfo`/`TagGroup` distinguish and store multiple
timing paths per vertex, how arrival and required times propagate through the graph,
how delay calculation is indexed, how multiple clocks and multiple scenes
(MCMM) are carried through all of it, and how the shared data structures are
made safe under multithreaded propagation (§7).

It is written as a handoff reference: each section starts with a plain-language
explanation before diving into the code. Section 0 is a glossary of terms used
throughout.

It reflects this fork, in which the upstream `Corner`/`PathAnalysisPt`/
`DcalcAnalysisPt` machinery has been replaced by the `Scene`/`Mode` model.
All file references are relative to the repo root.

---

## 0. Glossary — plain-language definitions

**Timing graph.** The circuit reduced to a graph: every pin is a *vertex*, every
way a signal can get from one pin to another (through a gate, or along a wire)
is an *edge*. A gate edge can contain several *timing arcs* (e.g. rising input
→ falling output).

**Arrival time.** The latest (for setup) or earliest (for hold) time a signal
transition can reach a pin, measured from the launching clock edge. Computed by
walking the graph forward from startpoints, adding delays.

**Required time.** The time a signal *must* arrive at a pin for the design to
work — derived from the capturing flop's clock edge, its setup/hold value, and
constraints. Computed by walking the graph backward from endpoints.

**Slack.** Required minus arrival (for setup). Positive = timing met, negative
= violation. Hold is the mirror image (arrival minus required).

**Startpoint / endpoint.** Startpoints: input ports and register clock pins
(where paths launch). Endpoints: register data pins with setup/hold checks and
output ports (where paths are captured/checked).

**Min/max (early/late).** Every analysis is done twice: with the largest
delays (*max*, used for setup checks) and the smallest delays (*min*, used for
hold checks).

**Rise/fall (rf).** Whether the signal at this pin is transitioning low→high
or high→low. Delays differ per transition, so paths are tracked per rise/fall.

**Clock edge.** A specific edge (rise or fall) of a specific clock, e.g.
"clk1 rising". A path is launched by one clock edge and captured by another;
the pair determines the timing check window.

**Ideal vs propagated clock.** An *ideal* clock arrives everywhere at a fixed
modeled latency (`set_clock_latency`). A *propagated* clock is actually traced
through the clock tree buffers, so each flop sees a real, calculated delay
(*insertion delay* = delay from clock source into the tree).

**Clock uncertainty.** Extra margin (`set_clock_uncertainty`) subtracted from
the check window to model jitter/skew not otherwise captured.

**CRPR (clock reconvergence pessimism removal).** When launch and capture
clock paths share a common stretch of clock tree, using max delay for one and
min delay for the other double-counts variation on the shared part. CRPR
computes that shared pessimism and adds it back. To do this, the analysis must
remember *which physical clock-tree branch* launched each path — that is what
the "crpr clock path" stored in `ClkInfo` is for.

**Timing exception.** An SDC command that overrides default single-cycle
timing: `set_false_path` (ignore these paths), `set_multicycle_path` (allow N
cycles), `set_max_delay`/`set_min_delay` (fixed limit). Exceptions can have
`-from/-thru/-to` points; a path only matches once it has passed the right
sequence of points. The *exception state* in a tag records "how far along the
-thru list this path has gotten so far."

**Tag.** The label that distinguishes one class of paths from another at the
same pin. If two signals reach a pin but were launched by different clocks, or
belong to different scenes, or matched different exceptions, they must be kept
as separate arrival numbers — the tag is the key that keeps them separate.
"One tag = one arrival/required slot on the vertex."

**Interning (a.k.a. hash-consing).** "Store one shared copy, reuse it
everywhere." Before creating a new Tag (or ClkInfo, or TagGroup), the code
looks it up in a global hash table. If an equal one already exists, that
existing object is reused; only if not is a new one created and added to the
table. Same idea as string interning in Java/Python. Three payoffs: (1) two
equal tags are literally the same object, so "same tag?" is a pointer compare;
(2) memory stays bounded — a million paths tagged identically share one Tag;
(3) each object gets a small stable index number (`TagIndex`), so a `Path`
can store a 28-bit index instead of a pointer. `Search::findTag`,
`findClkInfo`, and `findTagGroup` all follow this pattern. Where this document
says "interned" or "shared table", this is all it means.

**Scene.** One complete analysis view: which liberty libraries (corner), which
parasitics (SPEF), and which mode/SDC to use. Roughly what other tools call an
"analysis view" in MCMM (multi-corner multi-mode) analysis. Defined via the
`define_scene` Tcl command.

**Mode.** One set of constraints: an `Sdc` plus everything derived from it
(clock network, constant propagation, generated clocks). Several scenes can
share one mode (same SDC, different corners).

**Levelized BFS.** The propagation order: vertices are processed level by
level (level = max logic depth from a startpoint), so by the time a vertex is
visited, all its fanin already has final values. Backward for requireds.

**Generated clock (genclk).** A clock defined by `create_generated_clock`,
derived from a master clock through logic. Before normal analysis, the tool
runs a special search from the master source to the genclk source pin to
measure the genclk's insertion delay; tags on that search are marked
"genclk source path".

**Pulse clock.** A clock arc producing a pulse rather than a level transition;
needs its sense tracked through the clock tree.

**Segment start.** `set_input_delay` placed on an *internal* pin creates a
"path segment" starting there; the tag marks it so the pin behaves like a
startpoint.

**Derating.** `set_timing_derate` scale factors applied to delays (OCV
margining) when a delay is used in a check.

---

## 1. The Scene/Mode data model

*Plain language: a Scene answers "which libraries, which parasitics, which
constraints do I analyze with?" A Mode is the constraint half of that. The
whole MCMM setup is just a list of Scenes, and every path in the analysis
knows which Scene it belongs to.*

### Scene (`include/sta/Scene.hh`, `search/Scene.cc`)

A `Scene` is one analysis view — the fork's replacement for the upstream
(corner, min/max path analysis point) pair. It bundles:

| Member | Meaning |
|---|---|
| `name_`, `index_` | Unique name and dense index (0..N-1) |
| `mode_` | The `Mode` (and therefore `Sdc`) this scene analyzes under |
| `liberty_[min/max]` | Liberty libraries per min/max (`std::array<LibertySeq, 2>`) |
| `parasitics_[min/max]` | `Parasitics*` per min/max (`std::array<Parasitics*, 2>`) |

The scene collection lives on `StaState` as `SceneSeq scenes_`
(`include/sta/StaState.hh:132`), created by `Sta::makeScenes` (`search/Sta.cc`).
There is no `Scenes` collection class (`friend class Scenes;` in `Scene.hh` is a
leftover). Max scene count is `scene_count_max = 128`
(`include/sta/SearchClass.hh:137`).

### Mode (`include/sta/Mode.hh`, `search/Mode.cc`)

A `Mode` is "Sdc and dependent state". Each Mode owns:

- one `Sdc` (constraints)
- one `Sim` (constant propagation — which pins are tied 0/1 and what that disables)
- one `ClkNetwork` (which pins are in clock trees)
- one `Genclks` (generated clock machinery)
- `PathGroups` (report grouping)

and a `SceneSeq scenes_` — the scenes that reference it.

**Relationship: many Scenes → one Mode → one Sdc.** `Scene::sdc()` forwards to
`mode_->sdc()`. This is how one set of constraints (a mode) can be analyzed
across several corners (scenes), and how multiple modes with different SDCs can
coexist in one run.

### Index formulas (the glue between scenes and flat arrays)

*Plain language: delays, slews, and slack tables are stored in flat C arrays.
The slot number for "scene S, min or max" is always `S*2 + (0 for min, 1 for
max)`. Three differently-named accessors exist because they index three
different kinds of arrays, but they all compute the same number.*

```cpp
Scene::pathIndex(min_max)            = index_ * MinMax::index_count + min_max->index();
Scene::dcalcAnalysisPtIndex(min_max) = index_ * MinMax::index_count + min_max->index();
Scene::libertyIndex(min_max)         = index_ * MinMax::index_count + min_max->index();
```

- `dcalcAnalysisPtIndex` — indexes graph arc-delay and slew slots (§5).
- `pathIndex` (`PathAPIndex`) — indexes per-analysis-point arrays such as
  worst-slack / TNS tables (`search/Search.cc:3686,3699`, `search/WorstSlack.cc`)
  and `Path::pathAnalysisPtIndex` (`search/Path.cc:334`).
- `libertyIndex` — indexes scene-specific liberty cells/arcs (§5.2).

Total analysis point count: `StaState::dcalcAnalysisPtCount() =
MinMax::index_count * scenes_.size()` (`search/StaState.cc:166`).

---

## 2. Tags: the path discriminator

*Plain language: a pin does not have "an arrival time" — it has one arrival
time per distinguishable path class through it. The Tag is the composite key
that defines those classes. Everything in this section is about what goes into
that key, and when two keys count as "the same".*

### 2.1 Tag (`search/Tag.hh`, `search/Tag.cc`)

| Field | Meaning |
|---|---|
| `scene_` | **The Scene this path belongs to.** Replaces upstream `PathAnalysisPt*`. Immutable along a path; compared first in every tag comparison, so paths from different scenes never merge. |
| `rf_index_` | Rise/fall of the signal at this vertex |
| `min_max_index_` | Min (hold/early) or max (setup/late) analysis |
| `clk_info_` | Pointer to a shared `ClkInfo` — the launching clock context (§2.2) |
| `is_clk_` | True while the path is still inside the clock network. Clock paths get separate tags from data paths because a clock pin can also have input arrivals wrt other clocks. |
| `input_delay_` | `set_input_delay` that seeded the path (startpoint only; never propagated forward — see `thruTag`) |
| `is_segment_start_` | Path segment start (`set_input_delay` on an internal pin) |
| `states_` | `ExceptionStateSet*` — progress through `-thru` points of timing exceptions (false path, multicycle, path delay, filter). This is how exception matching is incremental instead of re-searching. |
| `is_loop_`, `is_filter_` | Derived from `states_` at construction (`Tag.cc:62-71`). "Filter" = the internal exception used by `report_timing -from/-thru/-to` to trace only matching paths. "Loop" = tag on a combinational loop breaking exception. |
| `index_` | Dense index into `Search::tags_` table; `Path` stores only this 28-bit index |

Tags live in one global table: `Search::findTag` (`search/Search.cc:2875`)
looks the tag up in `tag_set_` and creates it only if new (see "interning" in
the glossary). Equal tags are therefore the same object — pointer compares
work — and each tag gets a `TagIndex` (28 bits, `tag_index_bit_count` in
`SearchClass.hh`). Rise/fall
sibling tags are created together so `mutateTag` can cheaply re-index a tag for
the opposite transition via `tags_[tagsTableRfIndex(index, to_rf)]`
(`Search.cc:2634`).

### 2.2 ClkInfo (`search/ClkInfo.hh`, `search/ClkInfo.cc`)

*Plain language: ClkInfo answers "which clock launched this path, from where,
and with what latency/uncertainty?" It is split out of Tag because thousands
of tags share the same clock context — keeping one shared copy (in
`clk_info_set_`, same lookup-or-create pattern as tags) saves memory and makes
comparisons cheap.*

| Field | Meaning |
|---|---|
| `scene_` | Scene (again — ClkInfo is per-scene too) |
| `clk_edge_` | The launching `ClockEdge` (clock + rise/fall). Null = unclocked. **This is the "which clock" dimension of a tag.** |
| `clk_src_` | Clock source pin the path came from (a clock created on multiple pins → distinct ClkInfos per pin) |
| `gen_clk_src_` | Generated-clock source pin, for genclk source paths |
| `is_gen_clk_src_path_` | Tag is on a generated-clock *source* path (the pre-pass that measures genclk insertion delay) |
| `is_propagated_` | Propagated vs ideal clock |
| `insertion_` | Source latency (`set_clock_latency -source` or computed genclk insertion) |
| `latency_` | Ideal clock network latency (`set_clock_latency`) |
| `uncertainties_` | `set_clock_uncertainty` on clock/pin |
| `is_pulse_clk_`, `pulse_clk_sense_` | Pulse clock handling |
| `crpr_clk_path_` | **CRPR anchor**: the clock path (a `Path` value) at the last clock driver pin. Only set when the clock is propagated and CRPR is enabled. Lets paths launched from different min/max clock-tree branches stay distinct so `CheckCrpr` can subtract common pessimism. |
| `min_max_index_` | Min/max |

### 2.3 Tag comparison: equal vs match

*Plain language: there are two different questions. (1) "Is this exactly the
same tag?" — used when checking whether a tag already exists in the global
table. (2) "Should these
two paths share one storage slot, keeping only the worst?" — used when merging
arrivals at a vertex. The second is deliberately looser: paths that differ only
in clock latency numbers still land in one slot, because only the worst one
can ever matter for slack.*

Two comparison regimes (`search/Tag.cc`):

- **`Tag::cmp`/`equal`** — full identity, used by `findTag` to decide reuse
  vs create. Compares
  scene index, full `ClkInfo::cmp` (edge, src pin, insertion, latency,
  uncertainties, crpr path, ...), rf, min/max, is_clk, input_delay,
  segment_start, exception states.
- **`Tag::matchCmp`/`match`** — *merging* criterion, used to decide when two
  paths share one arrival slot. Compares scene index, rf, min/max,
  **clock edge only** (not the full ClkInfo!), is_clk, is_gen_clk_src,
  segment_start, exception states, and optionally the CRPR clock vertex id.

Consequence: paths that differ only in clock latency/insertion/uncertainty
merge into one slot (worst one wins), but paths from different clock *edges*,
different scenes, or (with CRPR active) different last-clock-driver vertices
stay separate.

Variants: `matchNoCrpr` (ignore crpr pin; used by required propagation when the
exact tag was pruned), `matchNoPathAp` / `matchCrpr` (ignore min/max; used to
find the opposite-min/max path for CRPR), `stateEqualCrpr` (only loop states).

Hashes: `Tag::findHash` folds scene index, rf, min/max, is_clk, segment_start,
state hashes, and `clk_info_->hash()`; `hash(match_crpr_clk_pin)` optionally
adds the crpr clock vertex id.

### 2.4 TagGroup: per-vertex tag→slot map (`search/TagGroup.hh/.cc`)

*Plain language: a vertex may hold ten paths, so it needs a little directory
saying "the tag for scene0/max/rise/clk1 lives in slot 3". That directory is
the TagGroup. Because many vertices (e.g. all pins in one cone) end up with
identical directories, one shared copy of each distinct directory is kept:
the vertex stores just a group number plus its own array of Path slots.*

- `PathIndexMap = VectorMap<Tag*, size_t, TagMatchLess>`
  (`SearchClass.hh:113`) maps tag → index into the vertex's `Path[]` array.
  Because the map is ordered by `TagMatchLess`, tags that *match* collapse to
  one slot.
- TagGroups use the same lookup-or-create table pattern, in
  `Search::tag_group_set_`
  (`Search::findTagGroup`, `Search.cc:2642`): vertices with the same tag set
  share one `TagGroup` object; the vertex stores just a `tag_group_index_`
  (`Graph.hh:322`).
- Group-level flags: `has_clk_tag_`, `has_genclk_src_tag_`, `has_filter_tag_`,
  `has_loop_tag_` — cheap per-vertex queries without iterating tags.
- `TagGroupBldr` is the mutable builder used while (re)computing a vertex's
  arrivals: `tagMatchPath` finds the matching slot, `setMatchPath`/`insertPath`
  write it. `default_path_count_ = #scenes × 2(rf) × 2(min/max)` — the
  expected common case of one tag per (scene, rf, min/max).

### 2.5 Path (`include/sta/Path.hh`, `search/Path.cc`)

*Plain language: a Path is one storage slot — one (arrival, required) pair
plus a pointer to the previous Path so the worst path can be traced back for
reporting. It is intentionally tiny; everything descriptive lives in the tag.*

```cpp
Path *prev_path_;        // previous path in the worst-path tree
Arrival arrival_;
Required required_;      // required time lives here too (no separate array)
union { VertexId vertex_id_; EdgeId prev_edge_id_; };
TagIndex tag_index_ : 28;
bool is_enum_ : 1;
unsigned prev_arc_idx_ : 2;
```

Everything else is derived from the tag: `Path::scene()` = `tag->scene()`,
`Path::dcalcAnalysisPtIndex()` = `scene->dcalcAnalysisPtIndex(minMax())`, etc.
Arrivals for a vertex live in `Vertex::paths_` (flat `Path[]`,
`Graph.hh:267-268,318`), indexed by `TagGroup::pathIndex(tag)`;
`Path::pathIndex()` recovers its slot by pointer arithmetic against
`vertex->paths()`.

---

## 3. Arrival propagation

*Plain language: start at clock pins and input ports, plant initial tags and
times there, then sweep forward through the graph in level order. At each pin,
pull every path from every fanin pin, add the edge delay, work out what the
tag becomes on this side of the edge, and keep the worst time per tag slot.*

Forward levelized BFS (`BfsFwdIterator arrival_iter_`) driven by
`Search::findArrivals` → `ArrivalVisitor::visit(Vertex*)` (`Search.cc:1136`).
Each visit *re-derives all* arrivals of a vertex from its fanin, builds a
`TagGroupBldr`, and commits.

### 3.1 Seeding (startpoints)

`Search::seedArrivals` (`Search.cc:1442`) enqueues clock roots, graph roots,
and input drivers. When visited, `ArrivalVisitor::seedArrivals`
(`Search.cc:1221`) loops over modes and dispatches:

- **Clock source pins → `seedClkArrivals`** (`Search.cc:1493`). The
  multiplicative fan-out of tags is explicit here:

  ```cpp
  for (const Clock *clk : *clks)                    // per clock on the pin
    for (Scene *scene : mode->scenes())             // per scene of the mode
      for (const MinMax *min_max : MinMax::range()) // min & max
        for (const RiseFall *rf : RiseFall::range())// rise & fall
          seedClkArrival(pin, rf, clk, clk->edge(rf), min_max, insertion, scene, ...);
  ```

  One physical clock pin ⇒ `#clocks × #scenes × 2 × 2` tags. Each
  `seedClkArrival` builds a `ClkInfo` (propagated?, latency, insertion,
  uncertainties), collects false-path `-from clk` exception states, and calls
  `findTag(scene, rf, min_max, clk_info, is_clk=true, ...)`. Arrival =
  insertion + `clk_edge->time()`.

- **Input arrivals (`set_input_delay`) → `seedInputArrival`**
  (`Search.cc:1697-1950`): one loop per `InputDelay` on the pin (this is how a
  port carries arrivals wrt *multiple different clocks* — each
  `set_input_delay -clock X` is a separate `InputDelay` object and produces
  its own tags), nested over scene, min/max, rise/fall. `inputDelayTag` builds
  the ClkInfo for the launch clock, makes a non-clock tag with `input_delay`
  set, then runs `mutateTag` to pick up `-thru` exceptions starting at that pin.

- **Unconstrained inputs / register clock pins / `set_min|max_delay -from`
  internal startpoints → `makeUnclkedPaths`** (`Search.cc:1615`), also looping
  scene × min/max × rf, tag via `fromUnclkedInputTag` (null clk edge).

### 3.2 Propagating a path across an edge

`PathVisitor::visitEdge` (`Search.cc:2015`) iterates every `Path` on the from
vertex (all scenes' paths interleaved), gates each by the search predicate
*for that path's mode* (`pred_->searchThru(edge, mode)` — so a disable or
constant in one mode does not block another mode's paths), and calls
`visitFromPath` (`Search.cc:2072`) — the dispatcher that decides how the
"to tag" is derived from the "from tag" by edge role:

| Edge situation | Tag builder |
|---|---|
| Generated-clock source path (`from_clk_info->isGenClkSrcPath()`) | `thruClkTag` |
| Register CLK→Q | `fromRegClkTag` then `thruTag` (§3.3) |
| Latch D→Q | `latchOutArrival` + `thruTag` |
| Still in clock network (`from_tag->isClock()`) | `thruClkTag` |
| Ordinary data edge | `thruTag` |

`to_arrival = from_arrival + arc_delay`, where `arc_delay` comes from
`Search::deratedDelay(..., dcalc_ap, sdc)` with
`dcalc_ap = from_path->dcalcAnalysisPtIndex()` — i.e. **the tag's scene picks
which delay slot is read** (§5.3). For clock paths under OCV, the insertion
uses early/late opposite conventions, and check-clock slews may read the
opposite min/max slot (`scene->dcalcAnalysisPtIndex(min_max->opposite())`,
`Search.cc:2116,2204`).

**`Search::mutateTag`** (`Search.cc:2536`) is the single function that changes
tag identity along an edge. *Plain language: as a path crosses an edge, its
tag usually stays the same shape — but if the edge passes a `-thru` point of
some exception, the exception state advances; if a false path completes, the
path dies; if the path exits the clock tree, is_clk flips. mutateTag applies
all of those rules and returns the resulting "to" tag from the global table
(or null to kill the path).*

1. Advances each `ExceptionState` in `from_tag->states()` whose next `-thru`
   matches (from_pin, to_pin, to_rf, min_max); adds states for new exceptions
   whose first `-thru` begins at to_pin (`sdc->exceptionThruStates`).
2. Kills the path (returns null tag) when a false path completes, or a loop
   exception completes, or a path-delay `-to` pin is passed.
3. If nothing changed (same ClkInfo, same is_clk, same segment_start/input
   handling), fast-path: re-index the existing tag for the new rise/fall.
4. Otherwise `findTag(scene, to_rf, min_max, to_clk_info, to_is_clk, ...)`.

**Scene and input_delay never change across an edge; is_clk, ClkInfo, rf, and
exception states do.**

### 3.3 Clock tag → data tag at register CLK→Q

*Plain language: at a flop's CLK→Q arc, the path stops being "a clock" and
becomes "data launched by that clock". Right at that boundary the analysis
snapshots which clock-tree branch it came from (for CRPR) and picks up any
`-from <clock>` exceptions.*

At a `regClkToQ` arc (`visitFromPath`, `Search.cc:2131-2173`):

1. `clkInfoWithCrprClkPath(from_clk_info, from_path)` (`Search.cc:2356`)
   rebuilds the ClkInfo with `crpr_clk_path_ = from_path` (when CRPR active) —
   the launched data path remembers *which* clock-tree branch launched it.
2. `fromRegClkTag` (`Search.cc:2332`) makes a fresh tag with `is_clk=false`,
   pulling in `-from <clock>` exception states.
3. Launch arrival is normalized by `clkPathArrival` (ideal clocks re-add
   insertion + edge time + latency), then the arc delay is added.

Inside the clock tree, `thruClkInfo` (`Search.cc:2424`) rebuilds ClkInfo only
when something changes: propagated flag turns on at a pin, genclk src pin
recorded, **CRPR clock path captured when leaving the clock network or
reaching a reg clk pin**, pulse-clock sense, per-pin `set_clock_latency`
override, uncertainty override. `to_is_clk` goes false when clock propagation
stops (non-wire/combinational edge or `clkStopPropagation`).

### 3.4 Merging and pruning

*Plain language: when several fanin paths land on the same tag slot, only the
worst survives (latest for max, earliest for min). CRPR pruning goes further:
if a path can't possibly become the worst even after the best-case pessimism
credit, drop it now instead of dragging it through the rest of the design.*

- `ArrivalVisitor::visitFromToPath` (`Search.cc:1330`) merges: look up
  matching tag in the builder (`tagMatchPath`); keep the incoming arrival only
  if no match exists or `delayGreater(to_arrival, match->arrival(), min_max)` —
  i.e. worst arrival per (matching) tag per min/max.
- CRPR pruning (`pruneCrprArrivals`, `Search.cc:1377`): among tags that differ
  only in crpr clock pin, drop those that cannot be worst even after removing
  the maximum possible common pessimism (`Crpr::maxCrpr`). A second
  no-crpr-matching builder (`tag_bldr_no_crpr_`) tracks the crpr-blind worst
  arrival for this comparison.
- `Search::setVertexArrivals` (`Search.cc:2678`) finds-or-creates the shared
  TagGroup and
  writes the `Path[]`; `arrivalsChanged` decides whether fanout must be
  re-enqueued (incremental timing).

---

## 4. Required propagation

*Plain language: requireds start at the endpoints. For each path (tag) ending
at a flop's D pin, the required time is "capture clock edge + its network
delay − setup value − uncertainty + CRPR credit" (mirror-image for hold), with
multicycle/max-delay exceptions overriding the default one-cycle window. Those
numbers then walk backward: the required at a pin is the tightest requirement
imposed by any of its fanout, minus the edge delay to get there.*

Backward levelized BFS (`BfsBkwdIterator required_iter_`) driven by
`Search::findRequireds` (`Search.cc:3143`).

### 4.1 Seeding at endpoints

`seedRequireds` → `seedRequired(vertex)` (`Search.cc:3328`) runs
`VisitPathEnds::visitPathEnds` with a `FindEndRequiredVisitor`:

- `VisitPathEnds::visitClkedPathEnds` (`search/VisitPathEnds.cc:84`) iterates
  **every Path (= every tag) on the endpoint vertex**, filters by scene
  membership in the mode and min/max, then builds `PathEnd` objects per
  constraint. A `PathEnd` pairs a data path with the specific check that
  constrains it: `PathEndCheck` (setup/hold from in-edge check arcs whose
  `check_role->pathMinMax()` matches the tag's min/max), `PathEndLatchCheck`,
  `PathEndOutputDelay` (`set_output_delay`), `PathEndPathDelay`
  (`set_max/min_delay` override), `PathEndGatedClock`, `PathEndDataCheck`
  (`set_data_check`), `PathEndUnconstrained`.
- For each constrained end: `required = path_end->requiredTime(sta)`, merged
  into the tag's slot with `RequiredCmp::requiredSet` (worst over ends).

Required-time math (`search/PathEnd.cc`):

```
requiredTime = targetClkArrivalNoCrpr ± margin + crpr
targetClkArrivalNoCrpr = tgt_clk_delay (latency/insertion of capture clock path)
                       + cycleAccting(src_edge, tgt_edge)->requiredTime(role)
                       + clock uncertainty
                       + multicycle adjustment
```

setup subtracts the check margin, hold adds it. `CycleAccting` is the object
that works out, for a given (launch edge, capture edge) pair — possibly of
different clocks with different periods — which capture edge closes the check
window and what the default single-cycle relationship is. CRPR
(`CheckCrpr::checkCrpr(path, tgt_clk_path)`) uses the crpr clock paths recorded
in both tags' ClkInfos. The governing exception (multicycle, path delay, false
path) is the highest-priority match found via `Search::exceptionTo` against the
states carried in the data path's tag.

### 4.2 Backward propagation

`RequiredVisitor::visit(vertex)` (`Search.cc:3441`):

1. Init a scratch `requireds_` array sized `tag_group->pathCount()` — one slot
   per tag, initialized to +∞/−∞ per the tag's min/max opposite.
2. `visitFanoutPaths`: for each out-edge and each from-path, compute the tag
   the forward search *would* produce at the to-vertex (same `thruTag`/
   `mutateTag` machinery — this guarantees the backward walk pairs up with the
   exact forward tags), then in `RequiredVisitor::visitFromToPath`
   (`Search.cc:3461`):
   - exact lookup: `to_tag_group->hasTag(to_tag)` → `from_required =
     to_required − arc_delay`;
   - if the exact tag was CRPR-pruned at the to-vertex: fall back to any
     to-path matching `Tag::matchNoCrpr` (same scene, clock edge, rf, min/max,
     states — ignoring crpr pin).
   - `RequiredCmp::requiredSet` keeps the worst (min over fanout for max
     paths, max for min paths).
3. If the vertex is also an endpoint, seed its own path-end requireds too.
4. `requiredsSave` writes each slot into `path->setRequired(...)` — requireds
   live inline in the same `Path` objects as arrivals, same tag slot, no
   separate array.
5. If anything changed, enqueue fanin (BFS proceeds toward startpoints).

Slack: `Path::slack = required − arrival` (max) / `arrival − required` (min).
WNS (worst negative slack) / TNS (total negative slack, sum over endpoints)
tables are per `PathAPIndex = scene->pathIndex(min_max)`; reduction across
scenes happens only at reporting (`totalNegativeSlack`, `worstSlack` loop over
`scenes_`).

---

## 5. Delay calculation and scene indexing

*Plain language: before the search runs, every timing arc in the graph gets
its delay computed once per (scene, min/max) and stored in a flat array on the
edge. The search never recomputes delays — it just reads the slot its tag's
scene points at. Slews (transition times) are stored the same way per vertex.*

### 5.1 No more DcalcAnalysisPt

`DcalcAnalysisPt` is gone; `DcalcAPIndex` is a plain `int`
(`include/sta/GraphClass.hh:65`) computed as
`scene->dcalcAnalysisPtIndex(min_max) = scene_index*2 + mm_index`.

Graph storage (`graph/Graph.cc`):

- Arc delays: flat per-edge array, slot = `arc->index() * ap_count_ + ap_index`
  where `ap_count_ = 2 * #scenes` (`Graph::arcDelay`, `Graph.cc:743`;
  `initArcDelays`, `:901`). Annotation bits (marking SDF-annotated delays)
  parallel this layout.
- Slews: per-vertex flat array, slot = `ap_index * RiseFall::index_count +
  rf->index()` (`Graph.cc:648-691`).

### 5.2 GraphDelayCalc iteration

`GraphDelayCalc::findDelays` (`dcalc/GraphDelayCalc.cc:338`) runs a parallel
forward BFS over drivers. Per driver edge (`findDriverEdgeDelays`,
`GraphDelayCalc.cc:994`):

```cpp
for (Scene *scene : scenes_) {
  const Mode *mode = scene->mode();
  if (search_pred_->searchFrom(from_vertex, mode)
      && search_pred_->searchThru(edge, mode)) {
    for (const MinMax *min_max : MinMax::range())
      for (const TimingArc *arc : arc_set->arcs())
        findDriverArcDelays(drvr_vertex, ..., arc, scene, min_max, ...);
  }
}
```

Per (scene, min/max) the calculation selects:

- **Liberty model**: `arc->gateModel(scene, min_max)` →
  `TimingArc::sceneArc(scene->libertyIndex(min_max))`
  (`liberty/TimingArc.cc:614`). When the same cell exists in several corner
  libraries, each `LibertyCell` keeps its per-scene variants in a
  `scene_cells_` array (`LibertyCell::sceneCell`, `liberty/Liberty.cc:1519`);
  the scene's liberty index picks the right variant. Op-cond scaling comes
  from `scene->sdc()->operatingConditions(min_max)`.
- **Parasitics**: `scene->parasitics(min_max)`
  (`GraphDelayCalc::parasiticLoad`, `GraphDelayCalc.cc:1405`) — this is where
  the per-scene SPEF (from `define_scene -spef`) is consumed.

Results are annotated with `ap_index = scene->dcalcAnalysisPtIndex(min_max)`
(`annotateDelaysSlews` / `annotateLoadDelays`).

### 5.3 How search consumes delays

During arrival propagation, `visitFromPath` reads
`dcalc_ap = from_path->dcalcAnalysisPtIndex(this)` — which is
`from_tag->scene()->dcalcAnalysisPtIndex(from_tag->minMax())` — and
`Search::deratedDelay` (`Search.cc:3013`) fetches
`graph_->arcDelay(edge, arc, dcalc_ap)` plus timing derates from the mode's
Sdc. **The tag is thus the single source of truth: its scene picks the delay/
slew slots, its mode picks the Sdc, its min/max picks the analysis side.**

Timing checks read the clock-side slew from
`scene->checkClkSlewIndex(min_max)` = `dcalcAnalysisPtIndex(checkClkSlewMinMax)`,
where `checkClkSlewMinMax` depends on the analysis type (`search/Scene.cc:102`):
`single` → min, `bc_wc` → same min/max, `ocv` → opposite min/max. (*Plain
language: in OCV analysis, for a setup check the data path uses max delays but
the capture clock should be as fast as possible — so the check reads the
clock slew from the opposite min/max slot.*)

---

## 6. How multiple clocks and multiple scenes coexist in a TagGroup

*Plain language: the engine does not run once per scene or once per clock. It
runs one propagation, and the tags do the bookkeeping: every combination of
scene × clock edge × min/max × rise/fall × exception context is its own tag,
its own slot, its own numbers. Isolation falls out of the comparison rules —
tags from different scenes or clocks simply never compare equal, so they never
merge or interfere.*

- **Multiple clocks**: each clock (and each of its edges) on a startpoint
  seeds its own `ClkInfo`/tag (§3.1). A pin clocked by N clock edges holds ≥N
  tags; `Tag::matchCmp` compares `clkEdge()`, so paths from different clock
  edges never merge. Different *source pins* of the same clock, different
  genclk source paths, and (with CRPR) different last-clock-driver vertices
  also stay distinct via ClkInfo/crpr-pin comparison.
- **Multiple scenes**: seeding loops `for (Scene *scene : mode->scenes())`
  create one tag per scene; every tag comparison orders by scene index first,
  so per-scene paths flow through one shared BFS but never interact. A
  vertex's `TagGroup` therefore holds the union of all scenes' × clocks' ×
  rf × min/max × exception-state tags, each with its own arrival/required slot
  in `Vertex::paths_`.
- **Mode isolation**: the search predicate is evaluated per path using the
  tag's mode (`from_path->mode()`), so disables/constants from one mode's
  Sdc/Sim don't block another mode's paths on the same edge.
- Cross-scene reduction (worst slack, TNS, reporting) happens only after
  search, indexed by `scene->pathIndex(min_max)`.

`VertexPathIterator` (`Path.hh:187`) is the standard way to walk a vertex's
paths filtered by (scene, min/max, rf).

---

## 7. Threading and concurrency

*Plain language: OpenSTA runs single-threaded by default. When more threads are
enabled, worker threads exist only inside a handful of well-delimited parallel
phases (delay calc, arrival search, required search, path-end collection, clock
skew). Everything those phases need — graph topology, levelization, constraints,
liberty data, scenes — is built beforehand on the main thread and treated as
read-only inside the phase. Shared structures that must grow during a phase
(the tag/ClkInfo/TagGroup tables, BFS queues, invalidation sets) take a mutex
on the write path; the hot read paths are lock-free.*

### 7.1 Thread pool

- Default thread count is **1** (`Sta::defaultThreadCount`, `search/Sta.cc:307`).
  The Tcl command `set_thread_count` (`util/Util.i:99`) calls
  `Sta::setThreadCount`, which stores `StaState::thread_count_` and creates (or
  resizes) a `DispatchQueue` thread pool (`Sta::setThreadCount1`,
  `search/Sta.cc:320`; pool in `util/DispatchQueue.cc`,
  `include/sta/DispatchQueue.hh`). No pool exists until thread count > 1.
- `DispatchQueue::dispatch(fn)` hands a task to a worker; each task receives its
  thread index (used to pick per-thread scratch state).
  `DispatchQueue::finishTasks()` blocks until all dispatched tasks complete
  (`DynamicLatch`, an atomic counter with C++20 `atomic::wait`). Every parallel
  phase ends with `finishTasks()` — worker threads never outlive a phase.
- All Tcl commands, netlist edits, SDC changes, liberty/SPEF reading, and
  reporting run on the main thread. Worker threads run only in the phases below.

### 7.2 The parallel phases

| Phase | Entry point | Parallelized over |
|---|---|---|
| Delay calculation | `GraphDelayCalc::findDelays` → `iter_->visitParallel` (`dcalc/GraphDelayCalc.cc:356`) | driver vertices, level by level |
| Arrival search | `Search::findArrivals1` / `findClkArrivals` → `arrival_iter_->visitParallel` (`search/Search.cc:657,1047`) | vertices, level by level |
| Required search | `Search::findRequireds` → `required_iter_->visitParallel` (`search/Search.cc:3152`) | vertices, reverse level order |
| Path end collection | `PathGroups::makeGroupPathEnds` (`search/PathGroup.cc:1008`) | endpoint vertices |
| Clock skew | `ClkSkews::findClkSkew` (`search/ClkSkew.cc:198`) | register clock vertices; per-thread partial skew maps merged after `finishTasks` |

The generated-clock source-path search is deliberately **not** parallel
(`Genclks::findSrcArrivals`, `search/Genclks.cc:788-795`, comment: "Parallel
visit is slightly slower (at last check)").

### 7.3 The level-barrier invariant

`BfsIterator::visitParallel` (`search/Bfs.cc:160`) processes one level at a
time: the level's vertex list is split into one contiguous chunk per thread,
the chunks are dispatched, and `finishTasks()` is called **before moving to the
next level**. (If a level has fewer vertices than threads it is visited
serially on the main thread.) Two consequences:

1. **Each vertex is visited by exactly one thread.** Per-vertex and per-edge
   result storage is therefore written without locks: `Vertex::paths_`
   (written via `Search::setVertexArrivals` → `Vertex::makePaths`,
   `graph/Graph.cc:1198`), per-vertex slew slots (`Graph::setSlew`,
   `graph/Graph.cc:677`), and per-edge arc delay slots. The arrays themselves
   are allocated on the main thread before the phase (`Graph::initSlews` /
   `Graph::initArcDelays`, `graph/Graph.cc:866-914`) — the parallel phase only
   fills slots.
2. **All fanin (fanout, for requireds) is final before a vertex is visited.**
   When `ArrivalVisitor` reads the from-vertex `Path` arrays in
   `visitFaninPaths`, those vertices are at lower levels, completed before the
   previous level barrier. Latch D→Q edges, which violate level order, are not
   followed in-pass: the latch output is parked in `postponed_arrivals_` (under
   `postponed_arrivals_lock_`, `search/Search.cc:1428-1437`) and visited on the
   next pass of the loop in `Search::findAllArrivals`.

Workers do enqueue fanout vertices into the BFS queue during a visit; the queue
is protected by `BfsIterator::queue_lock_` (`search/Bfs.cc:271`) with a
double-checked "already in queue" flag kept as an atomic per-vertex bitmask
(`Vertex::bfs_in_queue_`, `std::atomic<uint8_t>`, `include/sta/Graph.hh:325`).

### 7.4 Per-thread visitor state

`visitParallel` makes one `visitor->copy()` per thread (`search/Bfs.cc:169-172`);
each copy owns the mutable scratch state a visit needs:

- `ArrivalVisitor` copies own their `TagGroupBldr` scratch builders
  (`tag_bldr_`, `tag_bldr_no_crpr_`, `search/Search.cc:1083-1096`) and a
  private `tag_cache_` (`PathVisitor` copy constructor passes
  `make_tag_cache=true`, `search/Search.cc:1962-1973`) — see §7.5.
- `FindVertexDelays::copy` clones the `ArcDelayCalc` "because it needs separate
  state for each thread" (`dcalc/GraphDelayCalc.cc:321-327`).
- `MakeEndpointPathEnds` copies its `PathEndVisitor` per thread
  (`search/PathGroup.cc:979-997`); results funnel into shared `PathGroup`s
  under a lock (§7.6).
- `ClkSkews` gives each thread its own `ClkSkewMap` (`partial_skews`,
  `search/ClkSkew.cc:195-203`) and reduces them on the main thread after the
  barrier.

### 7.5 Interned global tables: locked create, lock-free read

The three interning tables of §2 grow concurrently during arrival/required
search. All use the same discipline — mutex on create, no lock on read:

- **Tags** — `Search::findTag` takes `tag_lock_` (`search/Search.cc:2894`) to
  probe/insert `tag_set_`. Before touching the lock it probes the visitor's
  per-thread `tag_cache_` (`search/Search.cc:2888-2892`); cache hits skip the
  global lock entirely.
- **ClkInfos** — `Search::findClkInfo` takes `clk_info_lock_`
  (`search/Search.cc:2984`).
- **TagGroups** — `Search::findTagGroup` takes `tag_group_lock_`
  (`search/Search.cc:2646`). `TagGroup::ref_count_` is `std::atomic<int>`
  (`search/TagGroup.hh:82`) because `setVertexArrivals` increments/decrements
  it from concurrent visits.

Index-to-object lookup (`Search::tag(TagIndex)`, used on every `Path::tag`
call) reads the `tags_` array with **no lock**. That is safe because the array
pointers are atomic (`std::atomic<Tag**> tags_`, `std::atomic<TagGroup**>
tag_groups_`, `include/sta/Search.hh:632,641`) and growth is copy-then-publish:
under the table lock, a double-size array is allocated, existing entries are
copied, and only then is the atomic pointer swung
(`search/Search.cc:2919-2930,2659-2670` — "make the new array and copy the
contents into it before updating tags_ so that other threads can use
Search::tag(TagIndex) without returning gubbish"). The retired arrays are
parked in `tags_prev_`/`tag_groups_prev_` and freed only after the parallel
pass by `Search::deleteTagsPrev` (`search/Search.cc:671-681`), so a reader
holding the old pointer never sees freed memory. New tags are stored into
`tags_[index]` *before* being inserted into `tag_set_`
(`search/Search.cc:2906-2909`), so any tag visible in the set is indexable.

### 7.6 Other shared structures written during parallel phases

Each has a dedicated mutex (`LockGuard` = `std::scoped_lock<std::mutex>`,
`include/sta/Mutex.hh:32`):

| Structure | Lock | Who writes concurrently |
|---|---|---|
| BFS level queues | `BfsIterator::queue_lock_` (`search/Bfs.cc:271`) | visits enqueueing fanout/fanin |
| `invalid_arrivals_` / `invalid_requireds_` | `invalid_arrivals_lock_` (`search/Search.cc:851,930` — "Lock for StaDelayCalcObserver called by delay calc threads") | delay-calc threads invalidating search results via observer |
| `invalid_tns_` | `tns_lock_` (`search/Search.cc:3721`) | endpoint slack invalidation from visits |
| Worst-slack queue per path index | `WorstSlack::lock_` (`search/WorstSlack.cc:265` — "Locking is required because ArrivalVisitor is called by multiple threads") | arrival visits updating WNS |
| `postponed_arrivals_` (latch outputs), `postponed_clk_endpoints_` | own locks (`search/Search.cc:1437,1024`) | arrival visits |
| `filtered_arrivals_` | `filtered_arrivals_lock_` (`search/Search.cc:2704`) | `setVertexArrivals` when a filter tag lands |
| `PathGroup::path_ends_` + pruning threshold | `PathGroup::lock_` (`search/PathGroup.cc:119,180`) | per-endpoint path-end insertion in `makeGroupPathEnds` |
| `invalid_check_edges_` / `invalid_latch_edges_` | `invalid_edge_lock_` (`dcalc/GraphDelayCalc.cc:762-782`) | dcalc visits queueing check/latch edges (processed serially after the parallel pass, `GraphDelayCalc.cc:359-367`) |
| Multi-driver net registry | `multi_drvr_lock_` (`dcalc/GraphDelayCalc.cc:814` — taken only when a vertex has multiple drivers: "Avoid locking for single driver nets") | lazy `MultiDrvrNet` creation during dcalc |
| Cycle accounting table | `Sdc::cycle_acctings_lock_` (`sdc/Sdc.cc:2411-2417` — "Determine cycle accounting on demand") | `CycleAccting` interning from required-time math in parallel required search |
| CUDD BDD manager (`Sim`) | `bdd_lock_` (`search/Sim.cc:91,121`) | BDD evaluation — the shared CUDD manager is serialized |
| Parasitic maps | `ConcreteParasitics::lock_` (`parasitics/ConcreteParasitics.cc:915-1187`) | on-demand reduced-parasitic creation and lookups from dcalc threads |
| Lazy voltage waveforms | `LibertyCell::waveform_lock_` + atomic `have_voltage_waveforms_` double-check (`liberty/Liberty.cc:1861-1868`) | first dcalc thread to need CCS waveforms for a cell |
| Debug print buffers | `Debug::buffer_lock_` (`include/sta/Debug.hh:60-66`) | `debugPrint` from any thread |

### 7.7 Pre-populated, read-only during parallel phases

The following are built or updated only on the main thread (between commands or
in a serial preamble) and are read without locks inside parallel phases:

- **Graph topology** — vertices/edges are made/deleted only during graph
  building and netlist edits (`Graph::makeEdge`, `graph/Graph.cc:707`); never
  from worker threads.
- **Levelization** — `Levelize` runs serially before search (no locks anywhere
  in `search/Levelize.cc`); levels are what the barrier discipline of §7.3 is
  built on.
- **Sdc / exceptions / clocks** — constraint data is read-only during search
  (exception `-thru` lookup in `mutateTag`, derates, etc.); the one lazily
  built piece, cycle accounting, is locked (§7.6).
- **Liberty data** — read-only during dcalc/search except the lazy voltage
  waveforms noted above.
- **Scenes and Modes** — `scenes_` / `modes_` are created by
  `define_scene`-time code on the main thread; parallel phases only read them.
- **Sim (constant propagation) values** — computed when constraints change;
  visits read per-pin values, and the shared BDD manager used for function
  evaluation is lock-serialized (§7.6).

Network edits and constraint changes therefore never race with search: they run
on the main thread and merely insert into the `invalid_*` sets, which the next
(possibly parallel) pass consumes.

For debugging: `set_thread_count 1` makes all of the above run serially and
deterministically ordered (see §10.7).

---

## 8. Quick reference: what makes two paths "different"

A vertex holds separate arrival/required slots for paths that differ in any of:

1. Scene
2. Min/max (setup vs hold analysis)
3. Rise/fall at the vertex
4. Launching clock edge (clock + edge) — including "unclocked"
5. Clock vs data (`is_clk_`) — a clock pin's clock path vs input-arrival paths
6. Generated-clock source path flag
7. Exception state progress (false path / multicycle / path delay / filter `-thru` chains)
8. Segment start flag
9. Input delay object (full tag identity only; merges under match rules)
10. CRPR last-clock-driver vertex (only when CRPR enabled and clocks propagated)

Everything else about the clock context (latency, insertion, uncertainty,
pulse sense) lives in ClkInfo and affects tag *identity* (whether a separate
Tag object exists in the table) but not tag *matching* — such paths share a
slot and the worst one wins.

---

## 9. Worked example (plain language)

Design: input port `in1` with `set_input_delay -clock clk1 2.0`, flop `r1`
clocked by propagated `clk1`, two scenes `fast` (mode M, FF libs) and `slow`
(mode M, SS libs).

At `r1/CLK` the vertex holds clock tags:
`{fast,slow} × {min,max} × {rise,fall}` for `clk1` — 8 tags, each `is_clk`,
each ClkInfo carrying the accumulated insertion for its scene/min-max.

At `r1/Q` the CLK→Q arc converts each max clock tag into a data tag: `is_clk`
false, clock edge `clk1 rise` (say), ClkInfo now carrying the crpr clock path
snapshot at `r1/CLK`. Meanwhile any combinational path from `in1` reaching the
same downstream pins carries tags with `input_delay` set and clock edge
`clk1 rise` — different tags (different exception/input context), so both
arrivals coexist at every shared pin.

At a downstream flop `r2/D`, `visitPathEnds` builds a `PathEndCheck` per data
tag against `r2`'s capture clock path, computes required with setup margin,
uncertainty, and CRPR (comparing the launch crpr path via `r1/CLK` with the
capture crpr path via `r2/CLK`), and requireds flow backward per tag. Slack
per scene is then just `required − arrival` in each slot; `report_checks`
reduces across scenes at the end.

---

## 10. Debugging tags: commands and techniques

*Plain language: everything below answers "what paths does this pin actually
hold, and why?" The commands live in the Tcl layer; most are intentionally
undocumented/hidden debug commands in the `sta::` namespace. Run them from any
OpenSTA shell. Arrivals must exist first — run `report_checks` (or anything
that triggers `findArrivals`) or a `find_timing` before inspecting, otherwise
vertices report "no arrivals".*

### 10.1 Report all tags + arrivals/requireds on a pin: `report_tag_arrivals`

The workhorse. Internal debug proc (`search/Search.tcl:808`, marked "Internal
debugging command", call as `sta::report_tag_arrivals`):

```tcl
sta::report_tag_arrivals [-digits N] pin_name
```

Calls `Search::reportArrivals(vertex, report_tag_index=1, digits)`
(`search/Search.cc:2748` via `report_tag_arrivals_cmd`, `search/Search.i:285`).
Output — one line per Path slot on the vertex, sorted:

```
Vertex r1/Q (fall)
Group 17
 r max 1.234 / 4.567 23 scene1 clk1 r clk_src ck1buf/Y crpr_pin r1/CLK max ...
 ^   ^   ^       ^    ^  ^      ^
 rf  mm  arrival required tag_index scene  <rest = Tag::to_string, see 10.4>
```

Format string: `" {rf} {min_max} {arrival} / {required} {tag}"`. So this one
command answers both "what tags does this pin have" and "what arrival/required
is stored per tag". `Group N` is the vertex's TagGroup number in the shared
group table.

### 10.2 Report the worst (or every) path through a pin: `report_path`

User-visible command with two hidden flags `-all` and `-tags`
(`search/Search.tcl:614`, comment: "Note that -all and -tags are intentionally
hidden"):

```tcl
report_path [-min|-max] [-all] [-tags] [-format ...] pin r|f
```

- default: reports the worst arrival path ending at the pin for that rf/min-max.
- `-all`: iterates every Path on the vertex (`$vertex path_iterator $rf $min_max`)
  and reports each one — one full path report per tag. Also prints
  `Tag group: <index>`.
- `-tags`: prefixes each report with `Tag: <Tag::to_string>` — this is "see
  details of a particular tag of a pin" combined with the full path that
  carries it.

So `report_path -max -all -tags r1/D f` = every fall/max path class into
`r1/D`, each labeled with its tag.

### 10.3 Arrival/required/slack per clock edge: `report_arrival` / `report_required` / `report_slack`

User-level, tag-aware but summarized per launching clock edge
(`search/Search.tcl:741-802` → `Sta::reportArrivalWrtClks` etc.,
`search/Sta.cc:3311`):

```tcl
report_arrival  [-scene scene] [-digits N] pin
report_required [-scene scene] [-digits N] pin
report_slack    [-scene scene] [-digits N] pin
```

These walk the vertex's paths and print min/max rise/fall values grouped by
clock edge (the tag's `clkEdge()`), optionally restricted to one scene. Use
these when you want "what are the arrivals wrt each clock" without raw tag
dumps.

### 10.4 Reading a tag dump: anatomy of `Tag::to_string`

(`search/Tag.cc:86-174`; ClkInfo part `search/ClkInfo.cc:145`.) A line like

```
23 scene1 rmax clk1 r (clock prop) clk_src ck1buf/Y crpr_pin ck2buf/Y max -false_path -from ... (next thru ...)
```

decodes as:

| Piece | Meaning |
|---|---|
| `23` | TagIndex (slot in the global tag table; stable within a run) |
| `scene1` | Scene name |
| `rmax` | rise/fall + min/max of the tag |
| `clk1 r` / `unclocked` | launching clock edge |
| `(clock prop)` / `(clock ideal)` / `(genclk)` | is_clk flag; propagated vs ideal; genclk source path |
| `clk_src <pin>` | clock source pin from ClkInfo |
| `crpr_pin <pin> <min/max>` / `crpr_pin null` | CRPR anchor: last clock driver path recorded in ClkInfo |
| `input <pin>` | seeding `set_input_delay` pin |
| `segment_start` | segment start flag |
| trailing exception text | each `ExceptionState`: the exception plus `(next thru ...)` = which `-thru` point the path must hit next, or `(thrus complete)` |

`ClkInfo::to_string` (printed by `report_clk_infos`) additionally shows
`insert <delay>` (source insertion) and `uncertain <min>:<max>`.

### 10.5 Global table dumps and statistics

All in `sta::` namespace (`search/Search.i:273-330`):

| Command | What it does |
|---|---|
| `sta::report_tags` | Dump every Tag in the global table `Search::tags_` (`Search::reportTags`, `Search.cc:2938`), plus longest hash-bucket stat |
| `sta::report_clk_infos` | Dump every ClkInfo in the global table, sorted (`Search.cc:2955`) — inspect all distinct clock contexts (insertions, crpr pins) in the run |
| `sta::report_tag_groups` | Dump every TagGroup: index, hash, and the tag→slot map per group (`Search.cc:2818`) |
| `sta::report_path_count_histogram` | Histogram: N paths-per-vertex vs vertex count (`Search.cc:2838`) — first stop when tag explosion is suspected |
| `sta::tag_count` | Number of distinct tags |
| `sta::clk_info_count` | Number of distinct ClkInfos |
| `sta::tag_group_count` | Number of distinct TagGroups |
| `sta::path_count` | Total Path slots over all vertices |

Watching `tag_count`/`path_count` before and after a constraint change is the
quick way to measure tag-explosion cost of exceptions.

### 10.6 Scriptable access from Tcl (vertex/path objects)

For ad-hoc debugging scripts (`graph/Graph.i`, `search/Search.i:1212-1285`):

```tcl
set pin [get_pins r1/Q]
foreach vertex [$pin vertices] {          ;# bidirect pins have 2 vertices
  puts "group [$vertex tag_group_index]"  ;# TagGroup number
  set iter [$vertex path_iterator "rise" "max"]   ;# rf + min/max filter
  while {[$iter has_next]} {
    set path [$iter next]
    puts "[$path tag]"          ;# Tag::to_string
    puts "arrival  [$path arrival]"
    puts "required [$path required]"
    puts "slack    [$path slack]"
    puts "pins     [$path pins]" ;# walk prev_path chain back to startpoint
  }
  $iter finish
}
```

`$path pins` returns the whole worst-path pin trace (follows `prev_path_`);
`$path start_path` gives the startpoint path. `PathEnd` objects (from
`find_timing_paths`) expose `path`, `slack`, `margin`, `data_required_time`,
etc.

### 10.7 `set_debug` trace categories

`sta::set_debug <category> <level>` (kept out of the global namespace on
purpose — `tcl/Util.tcl:288`). Use `set_thread_count 1` first; parallel BFS
interleaves output. Tag-relevant categories:

| Category | What it traces |
|---|---|
| `search` 1 | Pass-level: "find arrivals pass N", counts, invalidations (`Search.cc:655,807`) |
| `search` 2 | Per-vertex: "find arrivals \<vertex\>", seed decisions, invalid arrival/required marks (`Search.cc:847,1148,1245`) |
| `search` 3 | **Per-edge tag propagation** — the money level: prints from-vertex, `from tag:` / `to tag  :` (both `Tag::to_string`), and the merge decision `from_arrival + arc_delay = to_arrival >/< stored` (`ArrivalVisitor::visitFromToPath`, `Search.cc:1345-1361`). Answers "why did/didn't this tag reach this pin". |
| `search` 4 | Path deletes, "arrivals changed" per vertex |
| `crpr` 2 | CRPR resolution: crpr pin, src/tgt deltas (`search/Crpr.cc:239-297`) |
| `genclk` | Generated clock source-path search (tags with `is_gen_clk_src_path`) |
| `bfs` | BFS queue mechanics |
| `levelize`, `endpoint`, `tns` | Supporting machinery |

Typical session for "why is this path missing / where does this tag die":

```tcl
set_thread_count 1
sta::set_debug search 3
report_checks -through [get_pins u1/Y]   ;# or find_timing
# grep the log for the pin name; watch the from/to tag lines and the
# point where mutateTag kills the tag (false path complete, etc.)
```

### 10.8 C++-level hooks (for gdb / temporary instrumentation)

- `Search::reportArrivals(vertex, true, digits)` — callable from gdb on any
  vertex.
- `Tag::to_string(sta)`, `ClkInfo::to_string(sta)`, `Path::to_string(sta)`,
  `TagGroup::report(sta)`, `TagGroup::reportArrivalMap(sta)`,
  `TagGroupBldr::reportArrivalEntries()` — every level of the machinery has a
  printer.
- `Vertex::tagGroupIndex()` + `Search::tagGroup(index)` to go from vertex to
  its group in a debugger.
- `debugPrint(debug_, "search", 3, ...)` is the pattern to add temporary
  traces (per `doc/CodingGuidelines.txt` / repo conventions: `debugPrint` for
  debug, `report_->report(...)` for always-on prints; never leave logging in
  hot paths).

---

## 11. Key file index

| Area | Files |
|---|---|
| Tag definition, hashing, matching | `search/Tag.hh`, `search/Tag.cc` |
| Clock context | `search/ClkInfo.hh`, `search/ClkInfo.cc` |
| Per-vertex tag set / slot map | `search/TagGroup.hh`, `search/TagGroup.cc` |
| Path (arrival+required storage) | `include/sta/Path.hh`, `search/Path.cc` |
| Seeding, propagation, tag mutation, requireds | `search/Search.cc`, `include/sta/Search.hh` |
| Path end enumeration / required math | `search/VisitPathEnds.cc`, `search/PathEnd.cc` |
| CRPR | `search/Crpr.cc` |
| Scene / Mode | `include/sta/Scene.hh`, `search/Scene.cc`, `include/sta/Mode.hh`, `search/Mode.cc` |
| Delay calc | `dcalc/GraphDelayCalc.cc`, `graph/Graph.cc` |
| BFS iterators | `search/Bfs.cc` |
| Thread pool / locking primitives | `include/sta/DispatchQueue.hh`, `util/DispatchQueue.cc`, `include/sta/Mutex.hh` |
