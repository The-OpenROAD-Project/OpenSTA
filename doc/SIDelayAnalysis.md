# SI delay analysis (crosstalk delay)

This note describes the crosstalk (signal integrity, SI) delay analysis
added to OpenSTA in upstream commit `4f8d3ae6` ("Delay calculation noise
analysis support"). It is written from the source as of that commit and
is meant as a reference for reading and debugging the code.

The commands and source use the word "noise" (`annotate_noise_delays`,
`DelayNoise`, `-fields noise`). Here it always means **delay noise**: the
change in delay of a switching net caused by neighbouring nets switching
at the same time. Glitch (functional) noise on quiet nets is not
analyzed.

## Contents

- [Background](#background)
- [Inputs and requirements](#inputs-and-requirements)
- [Commands](#commands)
- [Flow overview](#flow-overview)
- [Step by step](#step-by-step)
- [Reporting and debugging](#reporting-and-debugging)
- [Function map](#function-map)
- [Parameters and defaults](#parameters-and-defaults)
- [Observations and limitations](#observations-and-limitations)
- [Example](#example)

## Background

Normal delay calculation lumps every coupling capacitor to ground. When
a neighbouring net (the **aggressor**) switches during the same window
as the net being timed (the **victim**), charge injected through the
coupling capacitance changes the victim waveform.

```
            Cc (coupling cap)
  aggressor ────||──── victim

  victim voltage (rising)
  VDD ┤                     ╭─────   aggressor switching opposite:
      │               ╭─────╯        slower edge, MAX delay increases
      │          ╭────╯ ╭─────────   quiet aggressor (normal dcalc)
      │      ╭───╯ ╭────╯
      │  ╭───╯ ╭───╯  ╭──────────    aggressor switching same way:
   0  ┼──╯─────╯──────╯              faster edge, MIN delay decreases
      └───────────────────────────► time
```

- Max (setup) analysis uses aggressors switching **opposite** to the
  victim, which increases the victim wire delay.
- Min (hold) analysis uses aggressors switching the **same** way as the
  victim, which decreases it.

The analysis computes, per victim wire edge (driver pin to load pin),
the difference between the delay simulated with aggressors and the
delay from normal delay calculation, and adds it to the timing graph.

## Inputs and requirements

| Input | Why |
|---|---|
| Liberty with CCS output current waveforms | The victim and aggressor drivers are simulated with the CCS driver model in `PrimaDelayCalc`. Without CCS data `checkLibrarayCcs` warns (1579) and every net fails the Prima argument check, so nothing is annotated. |
| Liberty `voltage_map` for `VDD` | `PrimaDelayCalc::checkArgs` fails with "vdd not defined" otherwise. |
| SPEF read with `-keep_capacitive_coupling` | Without it coupling capacitors are grounded on read and no aggressors are found (warning 2182/2183). |
| SDC with clocks | Aggressors are filtered by arrival windows relative to clock edges. |

## Commands

| Command | Purpose |
|---|---|
| `annotate_noise_delays [-scene] [-min] [-max] [-min_coupling_sum_ratio r] [-min_coupling_ratio r]` | Run the analysis on all nets and add the delay/slew increments to the graph. |
| `report_noise_delays [-net net] [-max_count n] [-rise\|-fall] [-scene] [-min] [-max] [-min_coupling_sum_ratio r] [-min_coupling_ratio r] [-digits d]` | Report normal vs. noisy wire delay and slew per load pin. With `-net`, one net is analyzed directly. |
| `report_checks -fields noise` | Adds a `Noise` column showing the wire delay increment on wire arcs of reported paths. |
| `report_victim_nets [-scene] [-min] [-max] [-min_coupling_sum_ratio r]` | List nets whose coupling to ground capacitance ratio exceeds the threshold. |
| `report_aggressors net [-scene] [-min] [-max] [-min_coupling_ratio r]` | List aggressors of a net and their coupling capacitance. |
| `write_delay_noise_case net basename -rise\|-fall [...]` | Write a Verilog and SPEF file containing one victim and its aggressors, for reproducing a case in isolation. |
| `report_gate_noise_delays gates` (hidden) | Debug: run the noise simulation on a hand-built gate list. |

Tcl: `dcalc/DelayNoise.tcl`. SWIG: `dcalc/DelayNoise.i`. `Sta` wrappers:
`Sta::annotateNoiseDelays`, `Sta::reportNoiseDelays`,
`Sta::reportVictimNets`, `Sta::reportAggressors`,
`Sta::writeNoiseDelayCase` in `search/Sta.cc`.

## Flow overview

```
annotate_noise_delays
 │
 ├─ Sta::noiseDelayPreamble
 │    checkLibrarayCcs ── warn if no CCS models
 │    searchPreamble + findArrivals ── arrival windows needed for filtering
 │
 └─ DelayNoise::annotateNoiseDelays
      │
      ├─ ensureNoiseDelays ── computed once, cached in noise_delays_
      │   └─ findNoiseDelays: for every net in the top instance
      │       │
      │       ├─ 1. victim filter      couplingCapRatio > min_coupling_sum_ratio
      │       │
      │       └─ for min_max in {min, max}, drvr_rf in {rise, fall}
      │           ├─ 2. victim arc       findDrvrEdgeArc
      │           ├─ 3. aggressors       aggressorNets + makeArcDcalcArgs
      │           │                      (coupling threshold + window overlap)
      │           ├─ 4. Prima checks     PrimaDelayCalc::checkArgs
      │           ├─ 5. alignment        alignAggressors
      │           ├─ 6. simulation       PrimaDelayCalc::gateDelays
      │           └─ 7. increments       findLoadDelays (keep worst per edge)
      │
      └─ 8. annotate graph
            load slew  += slew increment   (unless SDF annotated)
            wire delay += delay increment  (unless SDF annotated)
            delayInvalid(load vertex) ── downstream timing updates
```

## Step by step

### 1. Victim filter

`DelayNoise::findNoiseDelays(const Net *, ...)` handles one net. The
driver pin comes from `netDrvrPin` (first driver pin on the net that has
a driver vertex). The net's parasitic network must exist and pass:

```
couplingCapRatio = coupling_cap / cap_gnd  >  min_coupling_sum_ratio
```

computed by `DelayNoise::capacitances`:

- `cap_gnd` = sum over the net's internal (non-external) parasitic nodes
  of the node ground capacitance, plus the pin capacitance of nodes
  that are pins (`pinCap`, from `Sdc::pinCaps`).
- `coupling_cap` = sum of every coupling capacitor in the net's
  parasitic network.
- If `cap_gnd` is 0, the ratio is taken as 1.0.

### 2. Victim arc

For each min/max and each driver transition, `findDrvrEdgeArc` scans the
timing arcs into the driver vertex whose output transition matches,
keeps those whose arrival window overlaps the driver's own window (see
step 3), and picks the one with the worst input slew (largest for max,
smallest for min). The result is an `ArcDcalcArg` for the victim; it is
always `gates[0]`.

### 3. Aggressor selection

```
             victim net parasitics
  ┌─────────────────────────────────────────┐
  │ drvr ─R─●─R─●─R─● load                  │
  │         │   │   │                       │
  │        Cc1 Cc2 Cc3 ── external nodes on │
  │         │   │   │     other nets        │
  └─────────┼───┼───┼───────────────────────┘
            A   A   B        A: Cc1+Cc2   B: Cc3
```

`DelayNoise::aggressorNets` walks the victim's coupling capacitors. For
each capacitor with one node on the victim and an external node on
another net, it adds the capacitance to that net's total. A net is an
aggressor candidate if:

```
coupling_cap(victim, aggressor)  >  cap_gnd(victim) * min_coupling_ratio
```

`DelayNoise::makeArcDcalcArgs(drvr, aggressors, ...)` then turns each
candidate into a gate:

- Aggressor transition: same as the victim for min, opposite for max.
- Aggressor driver: the first pin in `network_->drivers(net)`.
- Aggressor arc: `findDrvrEdgeArc` on the aggressor driver vertex, using
  the **victim's** arrival windows, so only arcs whose window overlaps
  the victim's are kept.

`arrivalWindowsOverlap` compares windows per clock edge:

```
  aggressor output window = [input_min + arc_delay, input_max + arc_delay]
  victim window           = [victim_min, victim_max]     (driver pin)

  victim     |==========|
  aggr A          |==========|     overlap   (an endpoint is inside)
  aggr B                        |====|   no overlap → dropped
```

Arrivals come from `Search::arrivalsWrtClks`; only clock edges present
for both victim and aggressor are compared. If no aggressor survives,
the net is skipped for that transition.

### 4. Prima argument checks

`PrimaDelayCalc::checkArgs` must pass for every gate: a table model with
CCS output waveforms, a parasitic network, `VDD` defined in the
library, and the input slew inside the waveform table's slew axis.
Otherwise the net is skipped.

### 5. Aggressor alignment

The aggressor switching times are chosen to give the worst victim
delay. `DelayNoise::alignAggressors` does this in three parts.

**a. Initial offsets** (`findAggressorInitialOffsets`). For each gate,
the time its output crosses Vth is estimated as

```
gate_delay = (ref_time(in_slew) - in_slew / 2) + arc_delay
```

where `ref_time` comes from the CCS output waveforms. Each aggressor's
initial offset is `victim gate_delay - aggressor gate_delay`, which
makes every driver output cross Vth at the same time as the victim.

**b. Common offset search** (`findDeltaDelayOffset`). All aggressors are
then shifted together by one extra offset. The objective,
`loadDelay`, runs a full Prima simulation and returns the victim wire
delay at one "canary" load: the load with the largest slew for max
(smallest for min), from `findMaxSlewLoad`.

```
  victim wire delay at canary load
     │           ╭──╮   ← worst alignment (max analysis)
     │        ╭──╯  ╰──╮
     │ ───────╯        ╰──────
     └────────────────────────► common aggressor offset
        0   4ps  8ps   16ps ...
```

- Bracket: start at offsets 0 and 4 ps and pick the direction in which
  the delay gets worse. Keep stepping, doubling the step each time,
  until the delay stops getting worse.
- Refine: binary search between the bracket ends. At each midpoint the
  delay is evaluated at `offset` and `offset + 1 ps`; the sign of the
  difference decides which half is kept. The search stops when the
  bracket width is within 5% of the smaller endpoint magnitude.

Each objective evaluation is one multi-driver Prima simulation, so the
search costs one simulation per bracket step plus two per binary
search step.

**c. Final input delays.** Each aggressor's input delay is set to
`initial_offset + offset`. Every input delay (victim and aggressors) is
then reduced by the smallest aggressor initial offset, so the victim
input delay becomes `-min(initial_offsets)`. Relative timing between
the drivers is unchanged.

### 6. Multi-driver simulation (PrimaDelayCalc)

`PrimaDelayCalc::gateDelays` takes the whole gate list (victim plus
aggressors) and simulates them together.

```
  victim drvr ──R──●──R──●──► victim loads
                   │     │
                  Cg     Cc ── stamped between the two nets
                   ╧     │
  aggr drvr  ──R──●──R──●──► aggressor loads
                   │
                  Cg
                   ╧

  coupling to any net NOT in the gate list → stamped to ground
```

- **Nodes** (`findNodeCount`): starting at each driver pin, walk every
  gate's parasitic network through resistors. External nodes are not
  included. Zero-ohm resistors merge the two nodes.
- **Stamping** (`stampEqns`): each driver is a current port
  (`stampDriver`). Resistors become conductances (`stampResistors`).
  Ground and pin capacitances go on the diagonal. A coupling capacitor
  is stamped between nodes when both nets are in the gate list; each
  side stamps half, because each net's parasitic network contains the
  capacitor. Otherwise it is stamped to ground (`stampCapacitors`).
- **Reduction**: PRIMA model order reduction is used when the node count
  exceeds the order. `DelayNoise` sets the order to
  `3 * gate count` (`prima_order_per_gate_`).
- **Integration** (`simulate1`): fixed time step, one backward Euler
  step, then BDF2. The time step is 2% of the smallest
  `driver_resistance * load_cap`.
- **Driver model** (`updateCeffIdrvr`): each driver's current comes from
  its CCS output waveforms at the input slew and a running effective
  capacitance. Before the driver's waveform begins, and after it ends,
  the driver is modeled as a hold resistance.
- **Measurement**: Vth and slew threshold crossings are recorded for
  every pin node. The simulation stops when every victim load voltage
  is past the upper slew threshold by half the remaining swing (lower
  threshold for falling), or at the computed end time.
- **Results** (`dcalcResults`): only the victim net's driver gets
  results (gate delay, driver slew, and per-load wire delay and slew).

### 7. Increments

`DelayNoise::findLoadDelays` compares the simulation with the graph for
each victim wire edge:

```
delay_incr = simulated wire delay - graph wire delay
slew_incr  = simulated load slew  - graph load slew
```

If a wire edge already has an increment for the same scene,
transition and min/max, the worse one is kept (larger for max, smaller
for min). Results are stored in a `DcalcResults` object (wire edge and
load vertex maps, per scene, rise/fall, min/max).

### 8. Annotation

`DelayNoise::annotateNoiseDelays` writes the increments into the graph:

- `Graph::setSlew(load, ..., slew + slew_incr)` unless the slew is SDF
  annotated. The vertex is marked with `setHasNoiseSlewAnnotation`.
- `Graph::setWireArcDelay(edge, ..., delay + delay_incr)` unless the
  wire delay is SDF annotated. The edge is marked with
  `setHasNoiseDelayAnnotation`.
- `GraphDelayCalc::delayInvalid` on the load vertex, so delays
  downstream of the load (which depend on the new slew) and arrivals
  are recomputed by the next timing update.

It then reports the number of nets and annotated loads.

## Reporting and debugging

- `report_noise_delays` prints, per load pin, `delay -> noisy delay`
  and `slew -> noisy slew` with the transition, min/max and pin name.
- `report_checks -fields noise` reads the cached increments through
  `DelayNoise::wireDelayIncr` (from `ReportPath`) and shows them on
  wire arcs only.
- Debug tags: `sta::set_debug delay_noise <level>` (1: gate list and
  per-load delay/slew before and after; 2: final aggressor input
  delays; 3: initial offsets and offset search; 4: window overlaps) and
  `sta::set_debug prima <level>` (simulation detail).
- `write_delay_noise_case` writes `<basename>.v` and `<basename>.spef`
  with only the victim, aggressors and their loads. Related waveform
  tools added in the same commit: `write_gate_spice`,
  `write_gate_gnuplot` and `write_path_gnuplot` (`spice/`), for
  comparing Prima waveforms against a SPICE simulation.

## Function map

| Step | Function | File |
|---|---|---|
| Entry | `Sta::annotateNoiseDelays`, `Sta::noiseDelayPreamble`, `Sta::checkLibrarayCcs` | `search/Sta.cc` |
| Cache | `DelayNoise::ensureNoiseDelays` | `dcalc/DelayNoise.cc` |
| Net loop | `DelayNoise::findNoiseDelays` (3 overloads) | `dcalc/DelayNoise.cc` |
| Victim filter | `couplingCapRatio`, `capacitances`, `capGnd`, `pinCap` | `dcalc/DelayNoise.cc` |
| Arc choice | `findDrvrEdgeArc`, `arrivalWindowsOverlap` | `dcalc/DelayNoise.cc` |
| Aggressors | `aggressorNets`, `makeArcDcalcArgs`, `aggressorNetsSorted` | `dcalc/DelayNoise.cc` |
| Alignment | `alignAggressors`, `findAggressorInitialOffsets`, `findMaxSlewLoad`, `findDeltaDelayOffset`, `loadDelay` | `dcalc/DelayNoise.cc` |
| Simulation | `PrimaDelayCalc::gateDelays`, `checkArgs`, `simulate`, `simulate1`, `findNodeCount`, `stampEqns`, `stampCapacitors`, `updateCeffIdrvr`, `measureThresholds`, `dcalcResults` | `dcalc/PrimaDelayCalc.cc` |
| Increments | `findLoadDelays`, `DcalcResults` | `dcalc/DelayNoise.cc`, `.hh` |
| Annotation | `DelayNoise::annotateNoiseDelays`, `Vertex::setHasNoiseSlewAnnotation`, `Edge::setHasNoiseDelayAnnotation` | `dcalc/DelayNoise.cc`, `graph/Graph.cc` |
| Reporting | `reportNoiseDelays`, `reportVictimNets`, `reportAggressors`, `wireDelayIncr`, `ReportPath` noise field | `dcalc/DelayNoise.cc`, `search/ReportPath.cc` |
| Case writer | `writeCaseFiles` and `writeCase*` | `dcalc/DelayNoise.cc` |

## Parameters and defaults

| Parameter | Code default | Meaning |
|---|---|---|
| `-min_coupling_sum_ratio` | 0.7 | Victim filter: total coupling cap / ground cap. |
| `-min_coupling_ratio` | 0.1 | Aggressor filter: coupling cap to one aggressor / victim ground cap. |
| Alignment start step | 4 ps | First bracket step in `findDeltaDelayOffset`, doubled each step. |
| Slope probe | 1 ps | Offset used to decide the search direction. |
| Search tolerance | 5% | Relative bracket width at which the search stops. |
| PRIMA order per gate | 3 | Reduced model order is `3 * gate count`. |

The command help text and `doc/ChangeLog.md` state the two ratio
defaults the other way round (0.1 and 0.7). The values above are the
ones in `dcalc/DelayNoise.tcl`.

## Observations and limitations

These describe the code as of `4f8d3ae6`.

- **One pass.** Arrival windows come from the timing before annotation.
  Windows are not recomputed and the analysis is not iterated.
- **Results are cached.** `ensureNoiseDelays` computes only when the
  cache is empty. Running `annotate_noise_delays` or
  `report_noise_delays` (without `-net`) again with different
  thresholds reuses the first results.
- **Graph values are edited directly.** The increments are added to the
  stored wire delays and slews. The `hasNoise*Annotation` flags are set
  but nothing reads them, so a later delay recalculation of the victim
  driver (for example after a netlist or parasitic change) recomputes
  those values without the increments.
- **Same clock only.** Windows are compared only for clock edges that
  both victim and aggressor have. Aggressors timed by other clocks, or
  unclocked ones, are never selected.
- **Window overlap test** checks whether either end of the aggressor
  window falls inside the victim window. An aggressor window that fully
  contains the victim window is not counted as overlapping.
- **One common offset.** After initial alignment, all aggressors move
  together by a single offset optimized at a single load (the canary).
  Aggressors are not aligned independently, and other loads of the
  victim may not see their own worst case.
- **First driver.** A multi-driver aggressor net uses only its first
  driver.
- **Non-selected coupling is grounded.** Coupling to nets that are not
  selected as aggressors is stamped to ground (no switching factor).
- **Requires CCS.** NLDM-only libraries produce no annotation.
- **Search cost.** Every alignment step is a full multi-driver
  simulation, and the search stops on a relative tolerance. When the
  best offset is at or near zero the bracket shrinks toward zero and
  the search can take many steps.

## Example

```tcl
read_liberty my_ccs_lib.lib
read_verilog design.v
link_design top
read_sdc design.sdc
read_spef -keep_capacitive_coupling design.spef

report_checks                       ;# baseline
report_victim_nets
report_aggressors some_net

annotate_noise_delays
report_noise_delays -max_count 10
report_checks -fields {slew noise}
```
