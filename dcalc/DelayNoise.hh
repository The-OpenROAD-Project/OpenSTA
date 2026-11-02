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

#pragma once

#include <fstream>
#include <map>
#include <string>

#include "MinMax.hh"
#include "StaState.hh"
#include "NetworkClass.hh"
#include "Graph.hh"
#include "GraphCmp.hh"
#include "DelayCalcBase.hh"
#include "ArcDelayCalc.hh"

namespace sta {

class Scene;
class PrimaDelayCalc;

using WireDelayMap = std::map<Edge*, std::vector<RiseFallMinMax>, EdgeLess>;
using VertexSlewMap = std::map<Vertex*, std::vector<RiseFallMinMax>, VertexIdLess>;

class DcalcResults
{
public:
  DcalcResults(const Network *network,
               Graph *&graph);
  DcalcResults(const DcalcResults &rhs) = default;
  DcalcResults(DcalcResults &&rhs) = default;
  DcalcResults &operator=(const DcalcResults &rhs) = delete;
  DcalcResults &operator=(DcalcResults &&rhs) noexcept;
  bool empty() const;
  NetSet nets(const Network *network) const;
  WireDelayMap &wireDelayMap() { return wire_delay_map_; }
  VertexSlewMap &vertexSlewMap() {return vertex_slew_map_; }
  float wireDelay(Edge *wire_edge,
                  const Scene *scene,
                  const RiseFall *rf,
                  const MinMax *min_max);
  void wireDelay(Edge *wire_edge,
                 const Scene *scene,
                 const RiseFall *rf,
                 const MinMax *min_max,
                 // Return values.
                 float &wire_delay,
                 bool &exists);
  void setWireDelay(Edge *wire_edge,
                    const Scene *scene,
                    const RiseFall *rf,
                    const MinMax *min_max,
                    float wire_delay);
  float loadSlew(Vertex *load_vertex,
                 const Scene *scene,
                 const RiseFall *rf,
                 const MinMax *min_max);
  void loadSlew(Vertex *load_vertex,
                const Scene *scene,
                const RiseFall *rf,
                const MinMax *min_max,
                // Return values.
                float &slew,
                bool &exists);
  void setLoadSlew(Vertex *load_vertex,
                   const Scene *scene,
                   const RiseFall *rf,
                   const MinMax *min_max,
                   float load_slew);

private:
  WireDelayMap wire_delay_map_;
  VertexSlewMap vertex_slew_map_;
  size_t scene_count_{0};
};

class DelayNoise : public StaState
{
public:
  DelayNoise(StaState *sta);
  ~DelayNoise() override;
  void copyState(const StaState *sta) override;

  // Annotate noise delays and slews for aggressors with
  // sum(coupling_caps) / cap_ground > min_coupling_sum_ratio
  void annotateNoiseDelays(float min_coupling_sum_ratio,
                           float min_coupling_ratio,
                           const Scene *scene,
                           const MinMaxAll *min_max);
  void wireDelayIncr(Edge *wire_edge,
                     const Scene *scene,
                     const RiseFall *rf,
                     const MinMax *min_max,
                     // Return values.
                     float &wire_delay_incr,
                     bool &exists);
  bool annotateNoiseDelays(const Net *net,
                           float min_coupling_sum_ratio,
                           float min_coupling_ratio,
                           const Scene *scene,
                           const MinMaxAll *min_max);

  ////////////////////////////////////////////////////////////////  
  
  // Report noise delays for all nets with aggressors with
  // sum(coupling_cap) / cap_gnd > min_coupling_sum_ratio
  void reportNoiseDelays(size_t max_count,
                         float min_coupling_sum_ratio,
                         float min_coupling_ratio,
                         const Scene *scene,
                         const MinMaxAll *min_maxs,
                         int digits);
  // Report noise delays for aggressors with
  // coupling_cap / cap_gnd > min_coupling_ratio
  void reportNoiseDelays(const Net *net,
                         const RiseFallBoth *drvr_rfs,
                         float min_coupling_ratio,
                         const Scene *scene,
                         const MinMaxAll *min_maxs,
                         int digits);
  void reportNoiseDelays(ArcDcalcArgSeq &gates,
                         const Scene *scene,
                         const MinMax *min_max,
                         int digits);
  void reportVictimNets(float min_coupling_ratio,
                        const Scene *scene,
                        const MinMax *min_max);
  void reportAggressors(const Net *victim,
                        float min_coupling_ratio,
                        const Scene *scene,
                        const MinMax *min_max);
  void writeCaseFiles(const Net *net,
                      const RiseFall *drvr_rf,
                      std::string_view file_basename,
                      float min_coupling_sum_ratio,
                      float min_coupling_ratio,
                      bool no_timestamp,
                      const Scene *scene,
                      const MinMax *min_max);
  size_t primaOrderPerGate() const { return prima_order_per_gate_; }

  ////////////////////////////////////////////////////////////////
  // Accessors for tcl debug

  // coupling_cap/cap_gnd > min_coupling_ratio
  NetSeq aggressorNetsSorted(const Net *victim,
                             float min_coupling_ratio,
                             const Scene *scene,
                             const MinMax *min_max);
  ArcDcalcArg makeNetGateDrvr(const Net *net,
                              const RiseFall *drvr_rf);
  ArcDcalcArgSeq makeArcDcalcArgs(ArcDcalcArg &drvr,
                                  ConstNetSeq &aggressors,
                                  const Scene *scene,
                                  const MinMax *min_max);
  ArcDcalcArgSeq makeArcDcalcArgs(const Net *net,
                                  const RiseFall *drvr_rf,
                                  float min_coupling_ratio,
                                  const Scene *scene,
                                  const MinMax *min_max);
  void alignAggressorsCmd(ArcDcalcArgSeq &gates,
                          const Scene *scene,
                          const MinMax *min_max);
  // sum(coupling_caps) / cap_ground > min_coupling_ratio
  NetSeq findVictimNetsSorted(float min_coupling_sum_ratio,
                              const Scene *scene,
                              const MinMax *min_max);

protected:
  DcalcResults ensureNoiseDelays(float min_coupling_sum_ratio,
                                 float min_coupling_ratio,
                                 const Scene *scene,
                                 const MinMaxAll *min_max);
  DcalcResults findNoiseDelays(float min_coupling_sum_ratio,
                               float min_coupling_ratio,
                               const Scene *scene,
                               const MinMaxAll *min_max);
  void findNoiseDelays(const Net *net,
                       float min_coupling_sum_ratio,
                       float min_coupling_ratio,
                       const Scene *scene,
                       const MinMaxAll *min_max1,
                       DcalcResults &noise_delays);
  void findNoiseDelays(ArcDcalcArgSeq &gates,
                       LoadPinIndexMap &load_pin_index_map,
                       const Scene *scene,
                       const MinMax *min_max,
                       DcalcResults &noise_delays);
  void findLoadDelays(Vertex *drvr_vertex,
                      const RiseFall *drvr_rf,
                      ArcDcalcResult &dcalc_result,
                      LoadPinIndexMap &load_pin_index_map,
                      const Scene *scene,
                      const MinMax *min_max,
                      DcalcResults &noise_delays);
  ArcDcalcArgSeq makeArcDcalcArgs(const Net *net,
                                  Vertex *drvr_vertex,
                                  const RiseFall *drvr_rf,
                                  const Parasitic *parasitic,
                                  float min_coupling_ratio,
                                  const Scene *scene,
                                  const MinMax *min_max,
                                  const DelaysWrtClks &drvr_clk_arrivals);
  NetSeq findVictimNets(float min_coupling_sum_ratio,
                        const Scene *scene,
                        const MinMax *min_max);
  NetSeq aggressorNets(const Net *net,
                       const Parasitic *parasitic,
                       float min_coupling_cap,
                       const Scene *scene,
                       const MinMax *min_max);
  void alignAggressors(ArcDcalcArgSeq &gates,
                       LoadPinIndexMap &load_pin_index_map,
                       const Scene *scene,
                       const MinMax *min_max);
  const Pin *findMaxSlewLoad(Vertex *drvr_vertex,
                             const RiseFall *drvr_rf,
                             const Scene *scene,
                             const MinMax *min_max);
  FloatSeq findAggressorInitialOffsets(ArcDcalcArgSeq &gates,
                                       const Scene *scene,
                                       const MinMax *min_max);
  float findDeltaDelayOffset(ArcDcalcArgSeq &gates,
                             LoadPinIndexMap &load_pin_index_map,
                             const Scene *scene,
                             const MinMax *min_max,
                             FloatSeq &initial_offsets,
                             size_t load_idx);
  float loadDelay(ArcDcalcArgSeq &gates,
                  LoadPinIndexMap &load_pin_index_map,
                  const Scene *scene,
                  const MinMax *min_max,
                  size_t load_idx,
                  FloatSeq &initial_offsets,
                  float offset);
  void findDrvrEdgeArc(Vertex *drvr_vertex,
                       const RiseFall *drvr_rf,
                       const DelaysWrtClks &victim_clk_arrivals,
                       const Scene *scene,
                       const MinMax *min_max,
                       // Return values.
                       Edge *&edge,
                       const TimingArc *&arc);
  bool arrivalWindowsOverlap(const DelaysWrtClks &victim_clk_arrivals,
                             const DelaysWrtClks &aggr_input_clk_arrivals,
                             const Edge *edge,
                             const TimingArc *arc,
                             const Scene *scene,
                             const MinMax *min_max);
  float couplingCapRatio(const Parasitic *parasitic,
                         const Scene *scene,
                         const MinMax *min_max) const;
  void capacitances(const Parasitic *parasitic,
                    const Scene *scene,
                    const MinMax *min_max,
                    // Return values.
                    float &coupling_cap,
                    float &cap_gnd) const;
  float pinCap(const Pin *pin,
               const Scene *scene,
               const MinMax *min_max) const;
  float capGnd(const Parasitic *parasitic,
               const Scene *scene,
               const MinMax *min_max) const;
  const Pin *netDrvrPin(const Net *net) const;
  // sum(coupling_caps) / cap_ground
  float couplingCapRatio(const Net *net,
                         const Scene *scene,
                         const MinMax *min_max) const;
  // sum(coupling_caps)
  float couplingCap(const Net *net,
                    const Scene *scene,
                    const MinMax *min_max) const;
  // capacitance to ground
  float capGnd(const Net *net,
               const Scene *scene,
               const MinMax *min_max) const;
  float victimCapToAggressor(const Net *victim,
                             const Net *aggressor,
                             const Scene *scene,
                             const MinMax *min_max) const;

  ////////////////////////////////////////////////////////////////

  void reportNoiseDelays(Vertex *drvr_vertex,
                         const RiseFallBoth *drvr_rfs,
                         const Scene *scene,
                         const MinMaxAll *min_maxs,
                         DcalcResults &noise_delays,
                         int digits);

  ////////////////////////////////////////////////////////////////

  Network *makeCaseNetwork(const ArcDcalcArgSeq &gates);
  Instance *ensureCaseInstance(NetworkReader *case_network,
                               const Instance *inst);
  Net *ensureCaseNet(NetworkReader *case_network,
                     const Net *net);
  Pin *ensureCasePin(NetworkReader *case_network,
                     const Pin *pin,
                     Net *case_net);
  void ensureCasePort(NetworkReader *case_network,
                      Net *case_net,
                      PortDirection *dir);
  bool hasTopLevelPortPin(const Net *net);

  void writeCaseSpef(const ArcDcalcArgSeq &gates,
                     const std::string &filename,
                     Network *case_network,
                     float min_coupling_sum_ratio,
                     bool no_timestamp,
                     const Scene *scene,
                     const MinMax *min_max);
  void writeCaseSpefHeader(const ArcDcalcArgSeq &gates,
                           std::ofstream &stream,
                           bool no_timestamp);
  void writeCaseSpePorts(Network *case_network,
                         std::ofstream &stream);
  void writeCaseSpefNets(const ArcDcalcArgSeq &gates,
                         float min_coupling_sum_ratio,
                         const Scene *scene,
                         const MinMax *min_max,
                         std::ofstream &stream);
  void writeCaseSpefNetCaps(const ArcDcalcArgSeq &gates,
                            const ArcDcalcArg &gate,
                            const Parasitic *parasitic,
                            Parasitics *parasitics,
                            std::ofstream &stream);
  void writeCaseSpefNetRes(const Parasitic *parasitic,
                           Parasitics *parasitics,
                           std::ofstream &stream);
  std::string staToSpef(std::string_view sta_name);
  float spefCap(float cap);
  std::string spefNodeName(const ParasiticNode *node,
                           const Parasitics *parasitics);
  std::string spefNodeName(const Pin *pin);

  std::string topCellName(const ArcDcalcArgSeq &gates);

private:
  PrimaDelayCalc *prima_dcalc_;
  DcalcResults noise_delays_;
  size_t prima_order_per_gate_{3}; // Poles per victim/aggressor gate and parasitics.
  bool found_coupling_caps_{false};
};

} // namespace sta
