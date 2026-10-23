// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "DelayNoise.hh"

#include <algorithm>  // min/max
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "ConcreteNetwork.hh"
#include "Debug.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "MakeConcreteNetwork.hh"
#include "Parasitics.hh"
#include "ParseBus.hh"
#include "PortDirection.hh"
#include "PrimaDelayCalc.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "Search.hh"
#include "parasitics/SpefNamespace.hh"
#include "Sta.hh"
#include "StaConfig.hh"
#include "Units.hh"
#include "VerilogNamespace.hh"
#include "VerilogWriter.hh"

namespace sta {

using AggressorCapMap = std::map<const Net*, float>;


DelayNoise::DelayNoise(StaState *sta) :
  StaState(sta),
  prima_dcalc_(new PrimaDelayCalc(sta)),
  noise_delays_(network_, graph_)
{
}

DelayNoise::~DelayNoise()
{
  delete prima_dcalc_;
}

// Notify components.
void
DelayNoise::copyState(const StaState *sta)
{
  StaState::copyState(sta);
  prima_dcalc_->copyState(sta);
}

void
DelayNoise::annotateNoiseDelays(float min_coupling_sum_ratio,
                                float min_coupling_ratio,
                                const Scene *scene,
                                const MinMaxAll *min_max1)
{
  DcalcResults noise_delays = ensureNoiseDelays(min_coupling_sum_ratio,
                                                min_coupling_ratio,
                                                scene, min_max1);

  if (!noise_delays.empty()) {
    for (const auto& [load_vertex, load_slews] : noise_delays.vertexSlewMap()) {
      for (const MinMax *min_max : min_max1->range()) {
        for (const RiseFall *rf : RiseFall::range()) {
          const RiseFallMinMax &slews = load_slews[scene->index()];
          float slew_incr;
          bool slew_exists;
          slews.value(rf, min_max, slew_incr, slew_exists);
          if (slew_exists
              && !load_vertex->slewAnnotated(rf, min_max)) {
            DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
            const Slew &slew = graph_->slew(load_vertex, rf, ap_index);
            graph_->setSlew(load_vertex, rf, ap_index, delaySum(slew, slew_incr, this));
          }
        }
      }
      load_vertex->setHasNoiseSlewAnnotation(true);
      graph_delay_calc_->delayInvalid(load_vertex);
    }

    for (const auto& [wire_edge,  wire_delays] : noise_delays.wireDelayMap()) {
      for (const MinMax *min_max : min_max1->range()) {
        DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
        for (const RiseFall *rf : RiseFall::range()) {
          float delay_incr = wire_delays[scene->index()].value(rf, min_max);
          if (!graph_->wireDelayAnnotated(wire_edge, rf, ap_index)) {
            const ArcDelay &delay = graph_->wireArcDelay(wire_edge, rf, ap_index);
            graph_->setWireArcDelay(wire_edge, rf, ap_index,
                                    delaySum(delay, delay_incr, this));
          }
        }
      }

      wire_edge->setHasNoiseDelayAnnotation(true);
      graph_delay_calc_->delayInvalid(wire_edge->to(graph_));
    }

    report_->report("Found {} nets with coupling caps over threshold.",
                    noise_delays.nets(network_).size());
    report_->report("Annoated {} load pin slews / wire delays.",
                    noise_delays.vertexSlewMap().size());
  }
}

// For path reporting.
void
DelayNoise::wireDelayIncr(Edge *wire_edge,
                          const Scene *scene,
                          const RiseFall *rf,
                          const MinMax *min_max,
                          // Return values.
                          float &wire_delay_incr,
                          bool &exists)
{
  noise_delays_.wireDelay(wire_edge, scene, rf, min_max,
                          wire_delay_incr, exists);
}

////////////////////////////////////////////////////////////////

DcalcResults
DelayNoise::ensureNoiseDelays(float min_coupling_sum_ratio,
                              float min_coupling_ratio,
                              const Scene *scene,
                              const MinMaxAll *min_max)
{
  if (noise_delays_.empty()) {
    found_coupling_caps_ = false;
    // Save results in noise_delays_ for reference by path reporting.
    noise_delays_ = findNoiseDelays(min_coupling_sum_ratio, min_coupling_ratio,
                                    scene, min_max);

    if (!found_coupling_caps_)
      report_->warn(2182, "No coupling capacitors found. Use read_spef -keep_capacitive_coupling.");
  }
  return noise_delays_;
}

DcalcResults
DelayNoise::findNoiseDelays(float min_coupling_sum_ratio,
                            float min_coupling_ratio,
                            const Scene *scene,
                            const MinMaxAll *min_max)
{
  DcalcResults noise_delays(network_, graph_);
  NetIterator *net_iter = network_->netIterator(network_->topInstance());
  while (net_iter->hasNext()) {
    const Net *net = net_iter->next();
    findNoiseDelays(net, min_coupling_sum_ratio, min_coupling_ratio,
                    scene, min_max, noise_delays);
  }
  delete net_iter;
  return noise_delays;
}

void
DelayNoise::findNoiseDelays(const Net *net,
                            float min_coupling_sum_ratio,
                            float min_coupling_ratio,
                            const Scene *scene,
                            const MinMaxAll *min_max1,
                            DcalcResults &noise_delays)
{
  const Pin *drvr_pin = netDrvrPin(net);
  if (drvr_pin) {
    Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
    if (drvr_vertex) {
      LoadPinIndexMap load_pin_index_map =
        graph_delay_calc_->makeLoadPinIndexMap(drvr_vertex);
      const DelaysWrtClks drvr_clk_arrivals =
        search_->arrivalsWrtClks(drvr_vertex, scene);
      for (const MinMax *min_max : min_max1->range()) {
        Parasitics *parasitics = scene->parasitics(min_max);
        const Parasitic *parasitic = parasitics->findParasiticNetwork(net);
        if (parasitic
            && couplingCapRatio(parasitic, scene, min_max) > min_coupling_sum_ratio) {
          for (const RiseFall *drvr_rf : RiseFall::range()) {
            ArcDcalcArgSeq gates = makeArcDcalcArgs(net, drvr_vertex, drvr_rf,
                                                    parasitic,
                                                    min_coupling_ratio,
                                                    scene, min_max,
                                                    drvr_clk_arrivals);
            if (!gates.empty()
                && !prima_dcalc_->checkArgs(gates, scene, min_max)) {
              alignAggressors(gates, load_pin_index_map, scene, min_max);
              findNoiseDelays(gates, load_pin_index_map, scene, min_max,
                              noise_delays);
            }
          }
        }
      }
    }
  }
}

ArcDcalcArgSeq
DelayNoise::makeArcDcalcArgs(const Net *net,
                             Vertex *drvr_vertex,
                             const RiseFall *drvr_rf,
                             const Parasitic *parasitic,
                             float min_coupling_ratio,
                             const Scene *scene,
                             const MinMax *min_max,
                             const DelaysWrtClks &drvr_clk_arrivals)
{
  Edge *edge;
  const TimingArc *arc;
  findDrvrEdgeArc(drvr_vertex, drvr_rf, drvr_clk_arrivals,
                  scene, min_max, edge, arc);
  if (arc) {
    const Pin *in_pin = edge->from(graph_)->pin();
    ArcDcalcArg drvr(in_pin, drvr_vertex->pin(), edge, arc, 0.0);
    prima_dcalc_->setDcalcArgParasiticSlew(drvr, scene, min_max);
    float cap_gnd = capGnd(parasitic, scene, min_max);
    float min_coupling_cap = cap_gnd * min_coupling_ratio;
    ConstNetSeq aggressors = aggressorNets(net, parasitic, min_coupling_cap,
                                           scene, min_max);
    if (!aggressors.empty()) {
      ArcDcalcArgSeq gates = makeArcDcalcArgs(drvr, aggressors, scene, min_max);
      prima_dcalc_->setDcalcArgParasiticSlew(gates, scene, min_max);
      return gates;
    }
  }
  return ArcDcalcArgSeq();
}

void
DelayNoise::findNoiseDelays(ArcDcalcArgSeq &gates,
                            LoadPinIndexMap &load_pin_index_map,
                            const Scene *scene,
                            const MinMax *min_max,
                            DcalcResults &noise_delays)
{
  // Need more poles to accommodate aggressor parasitics.
  prima_dcalc_->setPrimaReduceOrder(gates.size() * prima_order_per_gate_);
  if (debug_->check("delay_noise", 1))
    debugPrint(debug_, "delay_noise", 1, "{} {}",
               ArcDcalcArg::to_string(gates, this),
               min_max->to_string());

  ArcDcalcArg &drvr_arg = gates[0];
  ArcDcalcResultSeq dcalc_results =
    prima_dcalc_->gateDelays(gates, load_pin_index_map, scene, min_max);
  ArcDcalcResult &dcalc_result = dcalc_results[0];
  Vertex *drvr_vertex = drvr_arg.drvrVertex(graph_);
  const RiseFall *drvr_rf = drvr_arg.drvrEdge();
  findLoadDelays(drvr_vertex, drvr_rf, dcalc_result,
                 load_pin_index_map, scene, min_max, noise_delays);
}

void
DelayNoise::findLoadDelays(Vertex *drvr_vertex,
                           const RiseFall *drvr_rf,
                           ArcDcalcResult &dcalc_result,
                           LoadPinIndexMap &load_pin_index_map,
                           const Scene *scene,
                           const MinMax *min_max,
                           DcalcResults &noise_delays)
{
  DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      Pin *load_pin = load_vertex->pin();
      size_t load_idx = load_pin_index_map[load_pin];
      const ArcDelay &delay_dcalc = graph_->wireArcDelay(wire_edge, drvr_rf, ap_index);
      const Slew &slew_dcalc = graph_->slew(load_vertex, drvr_rf, ap_index);
      ArcDelay delay_noise = dcalc_result.wireDelay(load_idx);
      Slew slew_noise = dcalc_result.loadSlew(load_idx);
      debugPrint(debug_, "delay_noise", 1,
                 "{} {} delay {} -> {} slew {} -> {}",
                 sdc_network_->pathName(load_pin),
                 drvr_rf->to_string(),
                 delayAsString(delay_dcalc, min_max, 3, this),
                 delayAsString(delay_noise, min_max, 3, this),
                 delayAsString(slew_dcalc, min_max, 3, this),
                 delayAsString(slew_noise, min_max, 3, this));

      // Multiple timing arcs with the same output transition
      // annotate the same wire edges so they must be combined
      // rather than set.
      float delay_incr = delayAsFloat(delayDiff(delay_noise, delay_dcalc, this));
      float slew_incr = delayAsFloat(delayDiff(slew_noise, slew_dcalc, this));
      float slew;
      bool exists;
      noise_delays.loadSlew(load_vertex, scene, drvr_rf, min_max, slew, exists);
      if (!exists || min_max->compare(slew_incr, slew))
        noise_delays.setLoadSlew(load_vertex, scene, drvr_rf, min_max, slew_incr);

      float delay;
      noise_delays.wireDelay(wire_edge, scene, drvr_rf, min_max, delay, exists);
      if (!exists || min_max->compare(delay_incr, delay))
        noise_delays.setWireDelay(wire_edge, scene, drvr_rf, min_max, delay_incr);
    }
  }
}

////////////////////////////////////////////////////////////////

void
DelayNoise::reportNoiseDelays(size_t max_count,
                              float min_coupling_sum_ratio,
                              float min_coupling_ratio,
                              const Scene *scene,
                              const MinMaxAll *min_maxs,
                              int digits)
{
  DcalcResults noise_delays = ensureNoiseDelays(min_coupling_sum_ratio,
                                                min_coupling_ratio,
                                                scene, min_maxs);
  if (!noise_delays.empty()) {
    NetSeq nets;
    for (const Net *net : noise_delays.nets(network_))
      nets.push_back(net);
    sort(nets, NetPathNameLess(network_));

    size_t i = 0;
    report_->report("  delay             slew");
    for (const Net *net : nets) {
      const Pin *drvr_pin = netDrvrPin(net);
      if (drvr_pin) {
        Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
        if (drvr_vertex) {
          reportNoiseDelays(drvr_vertex, RiseFallBoth::riseFall(), scene,
                            MinMaxAll::minMax(), noise_delays, digits);
          i++;
        }
      }
      if (i >= max_count)
        break;
    }
  }
}

void
DelayNoise::reportNoiseDelays(const Net *net,
                              const RiseFallBoth *drvr_rfs,
                              float min_coupling_ratio,
                              const Scene *scene,
                              const MinMaxAll *min_maxs,
                              int digits)
{
  found_coupling_caps_ = false;
  report_->report("  delay             slew");
  for (const RiseFall *drvr_rf : drvr_rfs->range()) {
    for (const MinMax *min_max : min_maxs->range()) {
      ArcDcalcArgSeq gates = makeArcDcalcArgs(net, drvr_rf, min_coupling_ratio,
                                              scene, min_max);
      reportNoiseDelays(gates, scene, min_max, digits);
    }
  }
  if (!found_coupling_caps_)
    report_->warn(2183, "No coupling capacitors found. Use read_spef -keep_capacitive_coupling.");
}

void
DelayNoise::reportNoiseDelays(ArcDcalcArgSeq &gates,
                              const Scene *scene,
                              const MinMax *min_max,
                              int digits)
{
  prima_dcalc_->setDcalcArgParasiticSlew(gates, scene, min_max);

  DcalcResults noise_delays(network_, graph_);
  ArcDcalcArg &drvr_arg = gates[0];
  Vertex *drvr_vertex = drvr_arg.drvrVertex(graph_);
  LoadPinIndexMap load_pin_index_map =
    graph_delay_calc_->makeLoadPinIndexMap(drvr_vertex);
  findNoiseDelays(gates, load_pin_index_map, scene, min_max, noise_delays);
  reportNoiseDelays(drvr_vertex, drvr_arg.drvrEdge()->asRiseFallBoth(),
                    scene, min_max->asMinMaxAll(), noise_delays, digits);
}

void
DelayNoise::reportNoiseDelays(Vertex *drvr_vertex,
                              const RiseFallBoth *drvr_rfs,
                              const Scene *scene,
                              const MinMaxAll *min_maxs,
                              DcalcResults &noise_delays,
                              int digits)
{
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      Pin *load_pin = load_vertex->pin();
      for (const RiseFall *drvr_rf : drvr_rfs->range()) {
        for (const MinMax *min_max : min_maxs->range()) {
          DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
          const ArcDelay &delay = graph_->wireArcDelay(wire_edge, drvr_rf, ap_index);
          const Slew &slew = graph_->slew(load_vertex, drvr_rf, ap_index);
          ArcDelay delay_incr = noise_delays.wireDelay(wire_edge,scene,drvr_rf,min_max);
          Slew slew_incr = noise_delays.loadSlew(load_vertex,scene,drvr_rf,min_max);
          report_->report("{:>7} -> {:>7} {:>7} -> {:>7} {} {} {}",
                          delayAsString(delay, min_max, digits, this),
                          delayAsString(delaySum(delay, delay_incr, this),
                                        min_max, digits, this),
                          delayAsString(slew, min_max, digits, this),
                          delayAsString(delaySum(slew, slew_incr, this),
                                        min_max, digits, this),
                          drvr_rf->to_string(true),
                          min_max->to_string(),
                          network_->pathName(load_pin));
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
DelayNoise::reportVictimNets(float min_coupling_ratio,
                             const Scene *scene,
                             const MinMax *min_max)
{
  NetSeq victims = findVictimNetsSorted(min_coupling_ratio, scene, min_max);
  report_->report("Found {} nets with coupling caps over threshold.", victims.size());
  report_->report("Net sum(coupling_caps)/gnd_cap cap_ratio");
  for (const Net *victim :victims) {
    float coupling_cap = couplingCap(victim, scene, min_max);
    float cap_gnd = capGnd(victim, scene, min_max);
    float cap_ratio = couplingCapRatio(victim, scene, min_max);
    Unit *cap_unit = units_->capacitanceUnit();
    report_->report("{} {}/{} {:.2f}",
                    network_->pathName(victim),
                    cap_unit->asString(coupling_cap, 3),
                    cap_unit->asString(cap_gnd, 3),
                    cap_ratio);
  }
}

void
DelayNoise::reportAggressors(const Net *victim,
                             float min_coupling_ratio,
                             const Scene *scene,
                             const MinMax *min_max)
{
  NetSeq aggressors = aggressorNetsSorted(victim, min_coupling_ratio, scene, min_max);
  if (aggressors.empty())
    report_->warn(2180, "No aggressors with coupling cap ratio > min_coupling_ratio found.");
  else {
    Unit *cap_unit = units_->capacitanceUnit();
    report_->report("Aggressor coupling_cap");
    for (const Net *aggressor : aggressors) {
      float cap = victimCapToAggressor(victim, aggressor, scene, min_max);
      report_->report("  {} {}",
                      network_->pathName(aggressor),
                      cap_unit->asString(cap, 3));
    }
  }
}

////////////////////////////////////////////////////////////////

// Debugging helper.
void
DelayNoise::alignAggressorsCmd(ArcDcalcArgSeq &gates,
                               const Scene *scene,
                               const MinMax *min_max)
{
  if (!gates.empty()) {
    prima_dcalc_->setDcalcArgParasiticSlew(gates, scene, min_max);
    prima_dcalc_->setPrimaReduceOrder(gates.size() * 3);
    ArcDcalcArg &drvr_arg = gates[0];
    Vertex *drvr_vertex = drvr_arg.drvrVertex(graph_);
    LoadPinIndexMap load_pin_index_map =
      graph_delay_calc_->makeLoadPinIndexMap(drvr_vertex);
    alignAggressors(gates, load_pin_index_map, scene, min_max);
  }
}

void
DelayNoise::alignAggressors(ArcDcalcArgSeq &gates,
                            LoadPinIndexMap &load_pin_index_map,
                            const Scene *scene,
                            const MinMax *min_max)
{
  ArcDcalcArg &drvr_arg = gates[0];
  Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_arg.drvrPin());
  // Use load with max/min slew for alignment canary.
  const Pin *victim_load = findMaxSlewLoad(drvr_vertex, drvr_arg.drvrEdge(),
                                           scene, min_max);
  size_t victim_idx = load_pin_index_map[victim_load];
  FloatSeq initial_offsets = findAggressorInitialOffsets(gates, scene, min_max);
  double offset = findDeltaDelayOffset(gates, load_pin_index_map, scene, min_max,
                                       initial_offsets, victim_idx);

  // Make sure all aggressor input delays are positive.
  float min_input_delay = *std::min_element(initial_offsets.begin() + 1,
                                            initial_offsets.end());
  offset -= min_input_delay;

  drvr_arg.setInputDelay(-min_input_delay);
  size_t drvr_count = gates.size();
  for (size_t drvr_idx = 1; drvr_idx < drvr_count; drvr_idx++) {
    ArcDcalcArg &gate = gates[drvr_idx];
    float input_delay = initial_offsets[drvr_idx] + offset;
    gate.setInputDelay(input_delay);
    debugPrint(debug_, "delay_noise", 2, "{} input delay {}",
               network_->pathName(gate.drvrPin()),
               delayAsString(input_delay, min_max, 3, this));
  }
}

const Pin *
DelayNoise::findMaxSlewLoad(Vertex *drvr_vertex,
                            const RiseFall *drvr_rf,
                            const Scene *scene,
                            const MinMax *min_max)
{
  DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
  const Pin *pin_max = nullptr;
  float slew_max = min_max->initValue();

  // Use first load for alignment canary.
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *to_vertex = wire_edge->to(graph_);
      float wire_slew = delayAsFloat(graph_->slew(to_vertex, drvr_rf, ap_index));
      if (min_max->compare(wire_slew, slew_max)) {
        pin_max = wire_edge->to(graph_)->pin();
        slew_max = wire_slew;
      }
    }
  }
  return pin_max;
}

// Find aggressor input delay offsets to make the gate outputs cross
// Vth at the same time as the victim.
FloatSeq
DelayNoise::findAggressorInitialOffsets(ArcDcalcArgSeq &gates,
                                        const Scene *scene,
                                        const MinMax *min_max)
{
  size_t drvr_count = gates.size();
  FloatSeq initial_offsets(drvr_count);
  DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
  ArcDelay drvr_delay = 0.0;
  for (size_t drvr_idx = 0; drvr_idx < drvr_count; drvr_idx++) {
    ArcDcalcArg &gate = gates[drvr_idx];
    GateTableModel *table_model = gate.arc()->gateTableModel(scene, min_max);
    OutputWaveforms *output_waveforms = table_model->outputWaveforms();
    float ref_time = output_waveforms->referenceTime(delayAsFloat(gate.inSlew()));
    // Time the input ramp starts to rise/fall.
    float input_delay = ref_time - gate.inSlew() / 2.0;

    ArcDelay delay = graph_->arcDelay(gate.edge(), gate.arc(), ap_index);
    // Time gate driver output crosses Vth (includes time for input to cross Vth).
    ArcDelay gate_delay = delaySum(input_delay, delay, this);
    if (drvr_idx == 0)
      drvr_delay = gate_delay;
    else {
      float offset = delayAsFloat(delayDiff(drvr_delay, gate_delay, this));
      initial_offsets[drvr_idx] = offset;
      debugPrint(debug_, "delay_noise", 3, "{} initial offset {}",
                 network_->pathName(gate.drvrPin()),
                 delayAsString(offset, min_max, 3, this));
    }
  }
  return initial_offsets;
}

float
DelayNoise::findDeltaDelayOffset(ArcDcalcArgSeq &gates,
                                 LoadPinIndexMap &load_pin_index_map,
                                 const Scene *scene,
                                 const MinMax *min_max,
                                 FloatSeq &initial_offsets,
                                 size_t load_idx)
{
  // offset1 lower bound
  // offset2 upper bound
  float offset1 = 0.0;
  float delta = 4e-12;          // 4ps
  float offset2 = offset1 + delta;
  float offset_tol = .05;            // 5%
  float delay1 = loadDelay(gates, load_pin_index_map, scene, min_max, load_idx,
                           initial_offsets, offset1);
  float delay2 = loadDelay(gates, load_pin_index_map, scene, min_max, load_idx,
                           initial_offsets, offset2);
  bool forward = min_max->compare(delay2, delay1);
  if (!forward) {
    std::swap(offset1, offset2);
    std::swap(delay1, delay2);
    delta = -delta;
  }
  // Surround the peak.
  // Push offset3 out until it is on the other side of the peak.
  while (true) {
    float offset3 = offset2 + delta;
    float delay3 = loadDelay(gates, load_pin_index_map, scene, min_max,
                             load_idx, initial_offsets, offset3);
    debugPrint(debug_, "delay_noise", 3, "offset {}:{}:{} delay {}:{}:{}",
               delayAsString(offset1, min_max, 6, this),
               delayAsString(offset2, min_max, 6, this),
               delayAsString(offset3, min_max, 6, this),
               delayAsString(delay1, min_max, 6, this),
               delayAsString(delay2, min_max, 6, this ),
               delayAsString(delay3, min_max, 6, this));
    if (min_max->compare(delay3, delay2)) {
      offset1 = offset2;
      delay1 = delay2;
      offset2 = offset3;
      delay2 = delay3;
      delta *= 2.0;
    }
    else {
      offset2 = offset3;
      break;
    }
  }
  if (!forward)
    std::swap(offset1, offset2);
  // Binary search for max delay.
  while (std::abs(offset1 - offset2) > std::min(std::abs(offset1),
                                                std::abs(offset2)) * offset_tol) {
    debugPrint(debug_, "delay_noise", 3, "offset {}:{} delay {}:{}",
               delayAsString(offset1, min_max, 3, this),
               delayAsString(offset2, min_max, 3, this),
               delayAsString(delay1, min_max, 3, this),
               delayAsString(delay2, min_max, 3, this));
    float offset3 = (offset1 + offset2) / 2.0;
    float delay3 = loadDelay(gates, load_pin_index_map, scene, min_max,
                             load_idx, initial_offsets, offset3);
    float delay4 = loadDelay(gates, load_pin_index_map, scene, min_max,
                             load_idx, initial_offsets, offset3 + 1e-12);
    if (min_max->compare(delay4, delay3)) {
      offset1 = offset3;
      delay1 = delay3;
    }
    else {
      offset2 = offset3;
      delay2 = delay3;
    }
  }
  return offset1;
}

// objective function
float
DelayNoise::loadDelay(ArcDcalcArgSeq &gates,
                      LoadPinIndexMap &load_pin_index_map,
                      const Scene *scene,
                      const MinMax *min_max,
                      size_t load_idx,
                      FloatSeq &initial_offsets,
                      float offset)
{
  size_t drvr_count = gates.size();
  for (size_t drvr_idx = 1; drvr_idx < drvr_count; drvr_idx++) {
    ArcDcalcArg &gate = gates[drvr_idx];
    gate.setInputDelay(initial_offsets[drvr_idx] + offset);
  }

  ArcDcalcResultSeq dcalc_results =
    prima_dcalc_->gateDelays(gates, load_pin_index_map, scene, min_max);
  ArcDelay load_delay = dcalc_results[0].wireDelay(load_idx);
  return delayAsFloat(load_delay);
}

////////////////////////////////////////////////////////////////

NetSeq
DelayNoise::aggressorNetsSorted(const Net *victim,
                                float min_coupling_ratio,
                                const Scene *scene,
                                const MinMax *min_max)
{
  Parasitics *parasitics = scene->parasitics(min_max);
  const Parasitic *parasitic = parasitics->findParasiticNetwork(victim);
  if (parasitic) {
    float cap_gnd = capGnd(parasitic, scene, min_max);
    float min_coupling_cap = cap_gnd * min_coupling_ratio;
    NetSeq aggressors = aggressorNets(victim, parasitic, min_coupling_cap,
                                      scene, min_max);
    std::ranges::sort(aggressors,
                      [victim, scene, min_max, this] (const Net *aggr1,
                                                      const Net *aggr2) {
                        return victimCapToAggressor(victim, aggr1, scene, min_max)
                          > victimCapToAggressor(victim, aggr2, scene, min_max);
                      }) ;
    return aggressors;
  }
  else
    return NetSeq();
}

NetSeq
DelayNoise::aggressorNets(const Net *net,
                          const Parasitic *parasitic,
                          float min_coupling_cap,
                          const Scene *scene,
                          const MinMax *min_max)
{
  Parasitics *parasitics = scene->parasitics(min_max);
  AggressorCapMap aggr_caps;
  for (ParasiticCapacitor *pcap : parasitics->capacitors(parasitic)) {
    float cap = parasitics->value(pcap);
    const ParasiticNode *node1 = parasitics->node1(pcap);
    const ParasiticNode *node2 = parasitics->node2(pcap);
    if (node1 && node2) {
      const Net *net1 = parasitics->net(node1, network_);
      const Net *net2 = parasitics->net(node2, network_);
      if (net2
          && net1 == net
          && net2 != net
          && parasitics->isExternal(node2))
        aggr_caps[net2] += cap;
      if (net1
          && net2 == net
          && net1 != net
          && parasitics->isExternal(node1))
        aggr_caps[net1] += cap;
      found_coupling_caps_ = true;
    }
  }
  NetSeq aggressors;
  for (const auto [net, cap] : aggr_caps) {
    if (cap > min_coupling_cap)
      aggressors.push_back(net);
  }
  return aggressors;
}

////////////////////////////////////////////////////////////////

NetSeq
DelayNoise::findVictimNets(float min_coupling_sum_ratio,
                           const Scene *scene,
                           const MinMax *min_max)
{
  NetSeq victims;
  NetIterator *net_iter = network_->netIterator(network_->topInstance());
  while (net_iter->hasNext()) {
    const Net *net = net_iter->next();
    if (couplingCapRatio(net, scene, min_max) > min_coupling_sum_ratio)
      victims.push_back(net);
  }
  return victims;
}

NetSeq
DelayNoise::findVictimNetsSorted(float min_coupling_sum_ratio,
                                 const Scene *scene,
                                 const MinMax *min_max)
{
  NetSeq victims = findVictimNets(min_coupling_sum_ratio, scene, min_max);
  std::ranges::sort(victims,
                    [scene, min_max, this] (const Net *net1,
                                            const Net *net2) {
                      return couplingCapRatio(net1, scene, min_max)
                        > couplingCapRatio(net2, scene, min_max);
                    });
  return victims;
}

// sum(couping_caps) / cap_gnd
float
DelayNoise::couplingCapRatio(const Net *net,
                             const Scene *scene,
                             const MinMax *min_max) const
{
  Parasitics *parasitics = scene->parasitics(min_max);
  const Parasitic *parasitic = parasitics->findParasiticNetwork(net);
  if (parasitic)
    return couplingCapRatio(parasitic, scene, min_max);
  else
    return 0.0;
}

float
DelayNoise::couplingCapRatio(const Parasitic *parasitic,
                             const Scene *scene,
                             const MinMax *min_max) const
{
  float coupling_cap, cap_gnd;
  capacitances(parasitic, scene, min_max, coupling_cap, cap_gnd);
  if (cap_gnd == 0.0)
    return 1.0;
  else
    return coupling_cap / cap_gnd;
}

float
DelayNoise::capGnd(const Net *net,
                   const Scene *scene,
                   const MinMax *min_max) const
{
  Parasitics *parasitics = scene->parasitics(min_max);
  const Parasitic *parasitic = parasitics->findParasiticNetwork(net);
  if (parasitic)
    return capGnd(parasitic, scene, min_max);
  return 0.0;
}

float
DelayNoise::capGnd(const Parasitic *parasitic,
                   const Scene *scene,
                   const MinMax *min_max) const
{
  float coupling_cap, cap_gnd;
  capacitances(parasitic, scene, min_max, coupling_cap, cap_gnd);
  return cap_gnd;
}

void
DelayNoise::capacitances(const Parasitic *parasitic,
                         const Scene *scene,
                         const MinMax *min_max,
                         // Return values.
                         float &coupling_cap,
                         float &cap_gnd) const
{
  Parasitics *parasitics = scene->parasitics(min_max);
  cap_gnd = 0.0;
  for (ParasiticNode *node : parasitics->nodes(parasitic)) {
    if (!parasitics->isExternal(node)) {
      const Pin *pin = parasitics->pin(node);
      if (pin)
        cap_gnd += pinCap(pin, scene, min_max);
      cap_gnd += parasitics->nodeGndCap(node);
    }
  }
  coupling_cap = 0.0;
  for (ParasiticCapacitor *cap : parasitics->capacitors(parasitic))
    coupling_cap += parasitics->value(cap);
}

float
DelayNoise::pinCap(const Pin *pin,
                   const Scene *scene,
                   const MinMax *min_max) const
{
  const Sdc *sdc = scene->sdc();
  float pin_cap = min_max->initValue();
  for (const RiseFall *rf : RiseFall::range()) {
    float pin_cap1 = 0.0;
    float wire_cap = 0.0;
    float fanout = 0.0;
    sdc->pinCaps(pin, rf, scene, min_max, pin_cap1, wire_cap, fanout);
    pin_cap = min_max->minMax(pin_cap, pin_cap1);
  }
  return pin_cap;
}

float
DelayNoise::couplingCap(const Net *net,
                        const Scene *scene,
                        const MinMax *min_max) const
{
  Parasitics *parasitics = scene->parasitics(min_max);
  const Parasitic *parasitic = parasitics->findParasiticNetwork(net);
  float coupling_cap = 0.0;
  if (parasitic) {
    for (ParasiticCapacitor *cap : parasitics->capacitors(parasitic))
      coupling_cap += parasitics->value(cap);
  }
  return coupling_cap;
}

float
DelayNoise::victimCapToAggressor(const Net *victim,
                                 const Net *aggressor,
                                 const Scene *scene,
                                 const MinMax *min_max) const
{
  Parasitics *parasitics = scene->parasitics(min_max);
  const Parasitic *parasitic = parasitics->findParasiticNetwork(victim);
  float coupling_cap = 0.0;
  if (parasitic) {
    for (ParasiticCapacitor *cap : parasitics->capacitors(parasitic)) {
      ParasiticNode *node1 = parasitics->node1(cap);
      if (node1) {
        const Net *net1 = parasitics->net(node1, network_);
        if (net1 == aggressor)
          coupling_cap += parasitics->value(cap);
      }
      ParasiticNode *node2 = parasitics->node2(cap);
      if (node2) {
        const Net *net2 = parasitics->net(node2, network_);
        if (net2 == aggressor)
          coupling_cap += parasitics->value(cap);
      }
    }
  }
  return coupling_cap;
}

////////////////////////////////////////////////////////////////

ArcDcalcArgSeq
DelayNoise::makeArcDcalcArgs(const Net *net,
                             const RiseFall *drvr_rf,
                             float min_coupling_ratio,
                             const Scene *scene,
                             const MinMax *min_max)
{
  ArcDcalcArg drvr = makeNetGateDrvr(net, drvr_rf);
  prima_dcalc_->setDcalcArgParasiticSlew(drvr, scene, min_max);
  const Parasitic *parasitic = drvr.parasitic();
  if (parasitic) {
    float min_coupling_cap = drvr.loadCap() * min_coupling_ratio;
    ConstNetSeq aggressors = aggressorNets(net, parasitic, min_coupling_cap,
                                           scene, min_max);
    ArcDcalcArgSeq gates = makeArcDcalcArgs(drvr, aggressors, scene, min_max);
    prima_dcalc_->setDcalcArgParasiticSlew(gates, scene, min_max);
    if (prima_dcalc_->checkArgs(gates, scene, min_max)) {
      std::string reason = prima_dcalc_->failureReason();
      report_->warn(2181, "prima arg check failed {}.", reason);
    }
    else {
      ArcDcalcArg &drvr_arg = gates[0];
      Vertex *drvr_vertex = drvr_arg.drvrVertex(graph_);
      LoadPinIndexMap load_pin_index_map =
        graph_delay_calc_->makeLoadPinIndexMap(drvr_vertex);
      alignAggressors(gates, load_pin_index_map, scene, min_max);
      return gates;
    }
  }
  return ArcDcalcArgSeq();
}

ArcDcalcArg
DelayNoise::makeNetGateDrvr(const Net *net,
                            const RiseFall *drvr_rf)
{
  const Scene *scene = scenes_[0];
  const MinMax *min_max = MinMax::max();
  const Pin *drvr_pin = netDrvrPin(net);
  if (drvr_pin) {
    Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
    if (drvr_vertex) {
      const DelaysWrtClks drvr_clk_arrivals =
        search_->arrivalsWrtClks(drvr_vertex, scene);
      Edge *edge = nullptr;
      const TimingArc *arc = nullptr;
      findDrvrEdgeArc(drvr_vertex, drvr_rf, drvr_clk_arrivals,
                      scene, min_max, edge, arc);
      if (edge) {
        const Pin *in_pin = edge->from(graph_)->pin();
        return ArcDcalcArg(in_pin, drvr_pin, edge, arc, 0.0);
      }
    }
  }
  return ArcDcalcArg();
}

ArcDcalcArgSeq
DelayNoise::makeArcDcalcArgs(ArcDcalcArg &drvr,
                             ConstNetSeq &aggressors,
                             const Scene *scene,
                             const MinMax *min_max)
{
  ArcDcalcArgSeq gates;
  if (drvr.drvrPin()) {
    gates.push_back(drvr);
    const RiseFall *drvr_rf = drvr.drvrEdge();
    Vertex *drvr_vertex = drvr.drvrVertex(graph_);
    const DelaysWrtClks drvr_clk_arrivals = search_->arrivalsWrtClks(drvr_vertex, scene);
    // Are you with me or agin me?
    const RiseFall *aggr_rf = (min_max==MinMax::min()) ? drvr_rf : drvr_rf->opposite();
    for (const Net *net : aggressors) {
      const PinSet *net_drvrs = network_->drivers(net);
      if (net_drvrs) {
        const Pin *aggr_pin = *net_drvrs->begin();
        Vertex *aggr_vertex = graph_->pinDrvrVertex(aggr_pin);
        Edge *edge = nullptr;
        const TimingArc *arc = nullptr;
        findDrvrEdgeArc(aggr_vertex, aggr_rf, drvr_clk_arrivals,
                        scene, min_max, edge, arc);
        if (edge) {
          const Pin *in_pin = edge->from(graph_)->pin();
          gates.emplace_back(in_pin, aggr_pin, edge, arc, 0.0);
        }
      }
    }
  }
  if (gates.size() > 1)
    return gates;
  else
    // None of the aggressors have an overlapping arrival window.
    return ArcDcalcArgSeq();
}

void
DelayNoise::findDrvrEdgeArc(Vertex *drvr_vertex,
                            const RiseFall *drvr_rf,
                            const DelaysWrtClks &victim_clk_arrivals,
                            const Scene *scene,
                            const MinMax *min_max,
                            // Return values.
                            Edge *&edge,
                            const TimingArc *&arc)
{
  edge = nullptr;
  arc = nullptr;
  float in_slew = min_max->initValue();
  VertexInEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge1 = edge_iter.next();
    Vertex *in_vertex = edge1->from(graph_);
    const DelaysWrtClks aggr_input_clk_arrivals =
      search_->arrivalsWrtClks(in_vertex, scene);
    for (TimingArc *arc1 : edge1->timingArcSet()->arcs()) {
      if (arc1->toEdge()->asRiseFall() == drvr_rf
          && arrivalWindowsOverlap(victim_clk_arrivals,
                                   aggr_input_clk_arrivals, edge1, arc1,
                                   scene, min_max)) {
        float in_slew1 =
          delayAsFloat(graph_delay_calc_->edgeFromSlew(in_vertex,
                                                       arc1->fromEdge()->asRiseFall(),
                                                       edge1, scene, min_max));
        if (min_max->compare(in_slew1, in_slew)) {
          edge = edge1;
          arc = arc1;
          in_slew = in_slew1;
        }
      }
    }
  }
}

bool
DelayNoise::arrivalWindowsOverlap(const DelaysWrtClks &victim_clk_arrivals,
                                  const DelaysWrtClks &aggr_input_clk_arrivals,
                                  const Edge *edge,
                                  const TimingArc *arc,
                                  const Scene *scene,
                                  const MinMax *min_max)

{
  DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
  float arc_delay = delayAsFloat(graph_->arcDelay(edge, arc, ap_index), min_max, this);
  const RiseFall *victim_rf = arc->toEdge()->asRiseFall();
  const RiseFall *aggr_in_rf = arc->fromEdge()->asRiseFall();
  for (auto const &[clk_edge, aggressor_arrivals] : aggr_input_clk_arrivals) {
    auto victim_itr = victim_clk_arrivals.find(clk_edge);
    if (victim_itr != victim_clk_arrivals.end()) {
      const RiseFallMinMaxDelay &victim_arrivals = victim_itr->second;
      bool victim_delays_exist;
      Arrival victim_min, victim_max;
      victim_arrivals.range(victim_rf, victim_min, victim_max, victim_delays_exist);
      bool aggr_input_delays_exist;
      Arrival aggr_input_min, aggr_input_max;
      aggressor_arrivals.range(aggr_in_rf, aggr_input_min, aggr_input_max,
                               aggr_input_delays_exist);
      float aggr_min = aggr_input_min + arc_delay;
      float aggr_max = aggr_input_max + arc_delay;
      if (victim_delays_exist && aggr_input_delays_exist
          && ((aggr_min >= victim_min
               && aggr_min <= victim_max)
              || (aggr_max >= victim_min
                  && aggr_max <= victim_max))) {
        debugPrint(debug_, "delay_noise", 4, "{} {} {} {} arrival {}-{} overlaps {}-{}.",
                   network_->pathName(network_->instance(edge->from(graph_)->pin())),
                   arc->to_string(),
                   clk_edge ? clk_edge->name() : "",
                   min_max->to_string(),
                   delayAsString(aggr_min, 3, this),
                   delayAsString(aggr_max, 3, this),
                   delayAsString(victim_min, 3, this),
                   delayAsString(victim_max, 3, this));
        return true;
      }
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////

// Avoid connected pin iterator.
const Pin *
DelayNoise::netDrvrPin(const Net *net) const
{
  NetPinIterator *pin_iter = network_->pinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isDriver(pin)) {
      Vertex *drvr_vertex = graph_->pinDrvrVertex(pin);
      if (drvr_vertex) {
        delete pin_iter;
        return drvr_vertex->pin();
      }
    }
  }
  delete pin_iter;
  return nullptr;
}

////////////////////////////////////////////////////////////////

void
DelayNoise::writeCaseFiles(const Net *net,
                           const RiseFall *drvr_rf,
                           std::string_view file_basename,
                           float min_coupling_sum_ratio,
                           float min_coupling_ratio,
                           bool no_timestamp,
                           const Scene *scene,
                           const MinMax *min_max)
{
  const Pin *drvr_pin = netDrvrPin(net);
  if (drvr_pin) {
    Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
    if (drvr_vertex) {
      LoadPinIndexMap load_pin_index_map =
        graph_delay_calc_->makeLoadPinIndexMap(drvr_vertex);
      const DelaysWrtClks drvr_clk_arrivals =
        search_->arrivalsWrtClks(drvr_vertex, scene);
      Parasitics *parasitics = scene->parasitics(min_max);
      const Parasitic *parasitic = parasitics->findParasiticNetwork(net);
      if (parasitic
          && couplingCapRatio(parasitic, scene, min_max) > min_coupling_sum_ratio) {
        ArcDcalcArgSeq gates = delay_noise_->makeArcDcalcArgs(net, drvr_vertex, drvr_rf,
                                                              parasitic,
                                                              min_coupling_ratio,
                                                              scene, min_max,
                                                              drvr_clk_arrivals);
        if (!gates.empty()) {
          std::string verilog_file = std::string(file_basename) + ".v";
          Network *case_network = makeCaseNetwork(gates);
          writeVerilog(verilog_file.c_str(), false, nullptr, case_network);

          std::string spef_file = std::string(file_basename) + ".spef";
          writeCaseSpef(gates, spef_file, case_network, min_coupling_ratio,
                        no_timestamp, scene, min_max);

          delete case_network;
        }
      }
    }
  }
}

Network *
DelayNoise::makeCaseNetwork(const ArcDcalcArgSeq &gates)
{
  NetworkReader *case_network = makeConcreteNetwork();
  Library *case_lib = case_network->makeLibrary("case", "");
  std::string top_cell_name = topCellName(gates);
  Cell *top_cell = case_network->makeCell(case_lib, top_cell_name, false, "");
  Instance *top_inst = case_network->makeInstance(top_cell, top_cell_name, nullptr);
  dynamic_cast<ConcreteNetwork*>(case_network)->setTopInstance(top_inst);

  for (const ArcDcalcArg &gate : gates) {
    const Pin *drvr_pin = gate.drvrPin();
    const Instance *gate_inst = network_->instance(drvr_pin);
    ensureCaseInstance(case_network, gate_inst);
    InstancePinIterator *pin_iter = network_->pinIterator(gate_inst);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      const Net *net = network_->net(pin);
      if (net) {
        Net *case_net = ensureCaseNet(case_network, net);
        ensureCasePin(case_network, pin, case_net);
        if (hasTopLevelPortPin(net))
          ensureCasePort(case_network, case_net, network_->direction(pin));

        // Make ports for gate inputs.
        if (network_->direction(pin)->isAnyInput())
          ensureCasePort(case_network, case_net, network_->direction(pin));

        // Add load pin instances.
        NetPinIterator *net_pin_iter = network_->pinIterator(net);
        while (net_pin_iter->hasNext()) {
          const Pin *load_pin = net_pin_iter->next();
          ensureCasePin(case_network, load_pin, case_net);
          const Instance *load_inst = network_->instance(load_pin);
          ensureCaseInstance(case_network, load_inst);
          const Net *load_net = network_->net(load_pin);
          if (load_net) {
            Net *case_load_net = ensureCaseNet(case_network, load_net);
            ensureCasePin(case_network, pin, case_load_net);
          }
        }
        delete net_pin_iter;
      }
      else
        ensureCasePin(case_network, pin, nullptr);
    }
    delete pin_iter;
  }
  return case_network;
}

bool
DelayNoise::hasTopLevelPortPin(const Net *net)
{
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isTopLevelPort(pin))
      return true;
  }
  delete pin_iter;
  return false;
}

Instance *
DelayNoise::ensureCaseInstance(NetworkReader *case_network,
                               const Instance *inst)
{
  std::string inst_name = network_->pathName(inst);
  Instance *case_inst = case_network->findInstance(inst_name);
  if (case_inst == nullptr)
    case_inst = case_network->makeInstance(network_->cell(inst), inst_name,
                                           case_network->topInstance());
  return case_inst;
}

Net *
DelayNoise::ensureCaseNet(NetworkReader *case_network,
                          const Net *net)
{
  std::string net_name = network_->pathName(net);
  Net *case_net = case_network->findNet(net_name);
  if (case_net == nullptr)
    case_net = case_network->makeNet(net_name, case_network->topInstance());
  return case_net;
}

Pin *
DelayNoise::ensureCasePin(NetworkReader *case_network,
                          const Pin *pin,
                          Net *case_net)
{
  const Instance *inst = network_->instance(pin);
  Instance *case_inst = ensureCaseInstance(case_network, inst);
  Port *port = network_->port(pin);
  Pin *case_pin = case_network->findPin(case_inst, port);
  if (case_pin == nullptr)
    case_pin = case_network->connect(case_inst, port, case_net);
  return case_pin;
}

void
DelayNoise::ensureCasePort(NetworkReader *case_network,
                           Net *case_net,
                           PortDirection *dir)
{
  Instance *case_top_inst = case_network->topInstance();
  Cell *case_top_cell = case_network->cell(case_top_inst);
  std::string port_name = network_->name(case_net);
  bool is_bus;
  std::string bus_name;
  int bus_index;
  parseBusName(port_name, '[', ']', '\\', is_bus, bus_name, bus_index);
  if (is_bus) {
    Port *case_port = case_network->findPort(case_top_cell, bus_name);
    if (case_port == nullptr) {
      const Instance *top_inst = network_->topInstance();
      const Cell *top_cell = network_->cell(top_inst);
      Port *port = network_->findPort(top_cell, bus_name);
      case_port = case_network->makeBusPort(case_top_cell, bus_name,
                                            network_->fromIndex(port),
                                            network_->toIndex(port));
      case_network->setDirection(case_port, dir);
    }
    Port *case_bit_port = case_network->findBusBit(case_port, bus_index);
    Pin *case_pin = case_network->findPin(case_top_inst, case_bit_port);
    if (case_pin == nullptr)
      case_network->connect(case_top_inst, case_bit_port, nullptr);
  }
  else {
    Port *case_port = case_network->findPort(case_top_cell, port_name);
    if (case_port == nullptr) {
      case_port = case_network->makePort(case_top_cell, port_name);
      case_network->setDirection(case_port, dir);
    }
    Pin *case_pin = case_network->findPin(case_top_inst, case_port);
    if (case_pin == nullptr)
      case_network->connect(case_top_inst, case_port, nullptr);
  }
}

std::string
DelayNoise::topCellName(const ArcDcalcArgSeq &gates)
{
  const ArcDcalcArg &drvr_arg = gates[0];
  const Pin *drvr_pin = drvr_arg.drvrPin();
  const Instance *drvr_inst = network_->instance(drvr_pin);
  const Instance *top_inst = network_->parent(drvr_inst);
  const Cell *top_cell = network_->cell(top_inst);
  std::string top_cell_name = network_->name(top_cell);
  return top_cell_name;
}

void
DelayNoise::writeCaseSpef(const ArcDcalcArgSeq &gates,
                          const std::string &filename,
                          Network *case_network,
                          float min_coupling_sum_ratio,
                          bool no_timestamp,
                          const Scene *scene,
                          const MinMax *min_max)
{
  std::ofstream stream;
  stream.open(filename);
  if (stream.is_open()) {
    writeCaseSpefHeader(gates, stream, no_timestamp);
    writeCaseSpePorts(case_network, stream);
    writeCaseSpefNets(gates, min_coupling_sum_ratio,
                      scene, min_max, stream);
  }
  stream.close();
}

//std::string date = sta::format("{:%m/%d/%Y %I:%M}",
//std::chrono::zoned_time{std::chrono::current_zone(),
//std::chrono::system_clock::now()});
std::string
dateTimeLocal()
{
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  if (localtime_s(&tm, &t) != 0)
    return {};  // or handle error
#else
  if (!localtime_r(&t, &tm))
    return {};  // or handle error
#endif
  char buf[32];  // plenty for "MM/DD/YYYY HH:MM\0"
  const size_t n = std::strftime(buf, sizeof(buf), "%m/%d/%Y %H:%M", &tm);
  return n ? std::string(buf, n) : std::string{};
}

void
DelayNoise::writeCaseSpefHeader(const ArcDcalcArgSeq &gates,
                                std::ofstream &stream,
                                bool no_timestamp)
{
  sta::print(stream, "*SPEF \"ieee 1481-1999\" \n");
  std::string top_cell_name = topCellName(gates);
  sta::print(stream, "*DESIGN \"{}\" \n", top_cell_name);
  std::string date = no_timestamp ? "" : dateTimeLocal();
  sta::print(stream, "*DATE \"{}\"\n", date);
  sta::print(stream, "*VENDOR \"OpenSTA\"\n");
  sta::print(stream, "*PROGRAM \"OpenSTA\"\n");
  sta::print(stream, "*VERSION \"{}\"\n", STA_VERSION);
  sta::print(stream, "*DESIGN_FLOW \"PIN_CAP NONE\"\n");
  sta::print(stream, "*DIVIDER /\n");
  sta::print(stream, "*DELIMITER :\n");
  sta::print(stream, "*BUS_DELIMITER []\n");
  sta::print(stream, "*T_UNIT 1 NS\n");
  sta::print(stream, "*C_UNIT 1 PF\n");
  sta::print(stream, "*R_UNIT 1 OHM\n");
  sta::print(stream, "*L_UNIT 1 HENRY\n");
}

void
DelayNoise::writeCaseSpePorts(Network *case_network,
                              std::ofstream &stream)
{
  sta::print(stream, "*PORTS\n");
  const Cell *top_cell = case_network->cell(case_network->topInstance());
  CellPortBitIterator *port_iter = case_network->portBitIterator(top_cell);
  while (port_iter->hasNext()) {
    const Port *port = port_iter->next();
    std::string spef_dir = "I";
    PortDirection *dir = case_network->direction(port);
    if (dir == PortDirection::input())
      spef_dir = "I";
    else if (dir->isAnyOutput())
      spef_dir = "O";
    sta::print(stream, "{} {}\n",
               case_network->name(port),
               spef_dir);
  }
  delete port_iter;
  sta::print(stream, "\n");
}

void
DelayNoise::writeCaseSpefNets(const ArcDcalcArgSeq &gates,
                              float min_coupling_sum_ratio,
                              const Scene *scene,
                              const MinMax *min_max,
                              std::ofstream &stream)
{
  Parasitics *parasitics = scene->parasitics(min_max);
  for (const ArcDcalcArg &gate : gates) {
    const Net *gate_net = gate.drvrNet(network_);
    const Parasitic *parasitic = parasitics->findParasiticNetwork(gate_net);
    if (parasitic
        && couplingCapRatio(parasitic, scene, min_max) > min_coupling_sum_ratio) {
      writeCaseSpefNetCaps(gates, gate, parasitic, parasitics, stream);
      writeCaseSpefNetRes(parasitic, parasitics, stream);
      sta::print(stream, "*END\n");
    }
  }
}

void
DelayNoise::writeCaseSpefNetCaps(const ArcDcalcArgSeq &gates,
                                 const ArcDcalcArg &gate,
                                 const Parasitic *parasitic,
                                 Parasitics *parasitics,
                                 std::ofstream &stream)
{
  const Net *gate_net = gate.drvrNet(network_);
  std::string net_name = network_->pathName(gate_net);
  std::string net_spef_name = staToSpef(net_name);
  sta::print(stream, "*D_NET {} {:.3e}\n", net_spef_name, spefCap(gate.loadCap()));

  // connections
  sta::print(stream, "*CONN\n");
  NetPinIterator *net_pin_iter = network_->pinIterator(gate_net);
  while (net_pin_iter->hasNext()) {
    const Pin *load_pin = net_pin_iter->next();
    PortDirection *dir = network_->direction(load_pin);
    std::string dir_name = dir->isAnyInput() ? "I" : "O";
    sta::print(stream, "*I {} {}\n",
               spefNodeName(load_pin),
               dir_name);
  }
  delete net_pin_iter;

  NetSet case_nets(network_);
  for (const ArcDcalcArg &gate : gates)
    case_nets.insert(gate.drvrNet(network_));

  // Grouned capacitances.
  sta::print(stream, "*CAP\n");
  // Node cap to gnd.
  size_t cap_id = 1;
  for (ParasiticNode *node : parasitics->nodes(parasitic)) {
    float cap = parasitics->nodeGndCap(node);
    if (cap > 0.0)
      sta::print(stream, "{} {} {:.3e}\n",
                 cap_id++,
                 spefNodeName(node, parasitics),
                 spefCap(cap));
  }

  // Coupling caps.
  for (ParasiticCapacitor *pcap : parasitics->capacitors(parasitic)) {
    float cap = parasitics->value(pcap);
    if (cap > 0.0) {
      const ParasiticNode *node1 = parasitics->node1(pcap);
      const ParasiticNode *node2 = parasitics->node2(pcap);
      const Net *net1 = node1 ? parasitics->net(node1, network_) : nullptr;
      const Net *net2 = node2 ? parasitics->net(node2, network_) : nullptr;
      if (net1 && net2
          && case_nets.contains(net1)
          && case_nets.contains(net2))
        sta::print(stream, "{} {} {} {:.3e}\n",
                   cap_id++,
                   spefNodeName(node1, parasitics),
                   spefNodeName(node2, parasitics),
                   spefCap(cap));
      else if (net1 == gate_net)
        sta::print(stream, "{} {} {:.3e}\n",
                   cap_id++,
                   spefNodeName(node1, parasitics),
                   spefCap(cap));
      else if (net2 == gate_net)
        sta::print(stream, "{} {} {:.3e}\n",
                   cap_id++,
                   spefNodeName(node2, parasitics),
                   spefCap(cap));
    }
  }
}

void
DelayNoise::writeCaseSpefNetRes(const Parasitic *parasitic,
                                Parasitics *parasitics,
                                std::ofstream &stream)
{
  sta::print(stream, "*RES\n");
  for (ParasiticResistor *pres : parasitics->resistors(parasitic)) {
    float res = parasitics->value(pres);
    const ParasiticNode *node1 = parasitics->node1(pres);
    const ParasiticNode *node2 = parasitics->node2(pres);
    sta::print(stream, "{} {} {} {:.3e}\n",
               parasitics->id(pres),
               spefNodeName(node1, parasitics),
               spefNodeName(node2, parasitics),
               res);
  }
}

std::string
DelayNoise::spefNodeName(const ParasiticNode *node,
                         const Parasitics *parasitics)
{
  const Pin *pin = parasitics->pin(node);
  if (pin)
    return spefNodeName(pin);
  else {
    const Net *net = parasitics->net(node, network_);
    std::string net_name = staToSpef(network_->name(net));
    unsigned net_id = parasitics->netId(node);
    std::string spef_name = net_name + ":" + std::to_string(net_id);
    return spef_name;
  }
}

std::string
DelayNoise::spefNodeName(const Pin *pin)
{
  const Instance *inst = network_->instance(pin);
  if (inst == network_->topInstance()) {
    const Port *port = network_->port(pin);
    return staToSpef(network_->name(port));
  }
  else {
    std::string inst_name = network_->pathName(inst);
    std::string inst_sname = staToSpef(inst_name);
    const LibertyPort *port = network_->libertyPort(pin);
    std::string port_sname = staToSpef(port->name());;
    return sta::format("{}:{}",
                       inst_sname,
                       port_sname);
  }
}

std::string
DelayNoise::staToSpef(std::string_view sta_name)
{
  return sta::staToSpef(sta_name, '/', '/', '\\');
}

float
DelayNoise::spefCap(float cap)
{
  return cap / 1e-12;   // pf
}

////////////////////////////////////////////////////////////////

DcalcResults::DcalcResults(const Network *network,
                           Graph *&graph) :
  wire_delay_map_(EdgeLess(network, graph)),
  vertex_slew_map_(VertexIdLess(graph)),
  scene_count_(graph ? graph->scenes().size() : 0)
{
}

DcalcResults &
DcalcResults::operator=(DcalcResults &&rhs) noexcept
{
  if (this != &rhs) {
    scene_count_ = rhs.scene_count_;
    wire_delay_map_.clear();
    wire_delay_map_.insert(std::make_move_iterator(rhs.wire_delay_map_.begin()),
                           std::make_move_iterator(rhs.wire_delay_map_.end()));
    vertex_slew_map_.clear();
    vertex_slew_map_.insert(std::make_move_iterator(rhs.vertex_slew_map_.begin()),
                            std::make_move_iterator(rhs.vertex_slew_map_.end()));
    rhs.wire_delay_map_.clear();
    rhs.vertex_slew_map_.clear();
  }
  return *this;
}

bool
DcalcResults::empty() const
{
  return vertex_slew_map_.empty()
    && wire_delay_map_.empty();
}

NetSet
DcalcResults::nets(const Network *network) const
{
  NetSet nets(network);
  for (const auto& [load_vertex, load_slews] : vertex_slew_map_) {
    const Pin *load_pin = load_vertex->pin();
    const Net *load_net = network->net(load_pin);
    nets.insert(load_net);
  }
  return nets;
}

float
DcalcResults::wireDelay(Edge *wire_edge,
                        const Scene *scene,
                        const RiseFall *rf,
                        const MinMax *min_max)
{
  auto itr = wire_delay_map_.find(wire_edge);
  if (itr == wire_delay_map_.end())
    return 0.0;
  else {
    const std::vector<RiseFallMinMax> &wire_delays = itr->second;
    return wire_delays[scene->index()].value(rf, min_max);
  }
}

void
DcalcResults::wireDelay(Edge *wire_edge,
                        const Scene *scene,
                        const RiseFall *rf,
                        const MinMax *min_max,
                        // Return values.
                        float &wire_delay,
                        bool &exists)
{
  auto itr = wire_delay_map_.find(wire_edge);
  if (itr == wire_delay_map_.end()) {
    wire_delay = 0.0;
    exists = false;
  }
  else {
    const std::vector<RiseFallMinMax> &wire_delays = itr->second;
    wire_delays[scene->index()].value(rf, min_max,
                                      wire_delay, exists);
  }
}

void
DcalcResults::setWireDelay(Edge *wire_edge,
                           const Scene *scene,
                           const RiseFall *rf,
                           const MinMax *min_max,
                           float wire_delay)
{
  std::vector<RiseFallMinMax> &wire_delays = wire_delay_map_[wire_edge];
  if (wire_delays.empty())
    wire_delays.resize(scene_count_);
  wire_delays[scene->index()].setValue(rf, min_max, wire_delay);
}

float
DcalcResults::loadSlew(Vertex *load_vertex,
                       const Scene *scene,
                       const RiseFall *rf,
                       const MinMax *min_max)
{
  auto itr = vertex_slew_map_.find(load_vertex);
  if (itr == vertex_slew_map_.end())
    return 0.0;
  else {
    std::vector<RiseFallMinMax> &load_slews = itr->second;
    const RiseFallMinMax &slews = load_slews[scene->index()];
    return slews.value(rf, min_max);
  }
}

void
DcalcResults::loadSlew(Vertex *load_vertex,
                       const Scene *scene,
                       const RiseFall *rf,
                       const MinMax *min_max,
                       // Return values.
                       float &slew,
                       bool &exists)
{
  auto itr = vertex_slew_map_.find(load_vertex);
  if (itr == vertex_slew_map_.end()) {
    slew = 0.0;
    exists = false;
  }
  else {
    std::vector<RiseFallMinMax> &load_slews = itr->second;
    const RiseFallMinMax &slews = load_slews[scene->index()];
    slews.value(rf, min_max, slew, exists);
  }
}

void
DcalcResults::setLoadSlew(Vertex *load_vertex,
                          const Scene *scene,
                          const RiseFall *rf,
                          const MinMax *min_max,
                          float load_slew)
{
  std::vector<RiseFallMinMax> &load_slews = vertex_slew_map_[load_vertex];
  if (load_slews.empty())
    load_slews.resize(scene_count_);
  RiseFallMinMax &slews = load_slews[scene->index()];
  slews.setValue(rf, min_max, load_slew);
}

} // namespace sta
