// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <vector>

#include "Scene.hh"

namespace sta {

class Report;
class Debug;
class Units;
class Network;
class NetworkEdit;
class NetworkReader;
class Graph;
class Edge;
class Levelize;
class Sim;
class Search;
class Parasitics;
class ArcDelayCalc;
class GraphDelayCalc;
class Latches;
class DispatchQueue;
class Variables;

using ModeSeq = std::vector<Mode*>;
using ModeSet = std::set<Mode*>;

// Most STA components use functionality in other components.
// This class simplifies the process of copying pointers to the
// components.  It is deliberately simple to minimize circular
// dependencies between the it and the components.
class StaState
{
public:
  // Make an empty state.
  StaState();
  StaState(const StaState *sta);
  // Copy the state from sta.  This is virtual so that a component
  // can notify sub-components.
  virtual void copyState(const StaState *sta);
  virtual ~StaState() {}
  Report *report() { return report_; }
  Report *report() const { return report_; }
  void setReport(Report *report);
  Debug *debug() { return debug_; }
  Debug *debug() const { return debug_; }
  void setDebug(Debug *debug);
  Units *units() { return units_; }
  Units *units() const { return units_; }
  void copyUnits(const Units *units);
  Network *network() { return network_; }
  Network *network() const { return network_; }
  NetworkEdit *networkEdit();
  NetworkEdit *networkEdit() const;
  NetworkReader *networkReader();
  NetworkReader *networkReader() const;
  Network *sdcNetwork() { return sdc_network_; }
  Network *sdcNetwork() const { return sdc_network_; }
  // Command network uses the SDC namespace.
  Network *cmdNetwork() { return cmd_network_; }
  Network *cmdNetwork() const { return cmd_network_; }
  Graph *graph() { return graph_; }
  Graph *graph() const { return graph_; }
  Graph *&graphRef() { return graph_; }
  Levelize *levelize() { return levelize_; }
  Levelize *levelize() const { return levelize_; }
  ArcDelayCalc *arcDelayCalc() { return arc_delay_calc_; }
  ArcDelayCalc *arcDelayCalc() const { return arc_delay_calc_; }
  GraphDelayCalc *graphDelayCalc() { return graph_delay_calc_; }
  GraphDelayCalc *graphDelayCalc() const { return graph_delay_calc_; }
  Search *search() { return search_; }
  Search *search() const { return search_; }
  Latches *latches() { return latches_; }
  Latches *latches() const { return latches_; }
  unsigned threadCount() const { return thread_count_; }
  float sigmaFactor() const { return sigma_factor_; }
  bool crprActive(const Mode *mode) const;
  Variables *variables() { return variables_; }
  const Variables *variables() const { return variables_; }
  // Edge is default cond disabled by timing_disable_cond_default_arcs var.
  [[nodiscard]] bool isDisabledCondDefault(const Edge *edge) const;

  const SceneSeq &scenes() { return scenes_; }
  const SceneSeq &scenes() const { return scenes_; }
  bool multiScene() const { return scenes_.size() > 1; }
  size_t scenePathCount() const;
  DcalcAPIndex dcalcAnalysisPtCount() const;

  const SceneSet scenesSet();

  ModeSeq &modes() { return modes_; }
  const ModeSeq &modes() const { return modes_; }
  bool multiMode() const { return modes_.size() > 1; }

protected:
  Report *report_;
  Debug *debug_;
  Units *units_;
  Network *network_;
  Network *sdc_network_;
  // Network used by command interpreter (SdcNetwork).
  Network *cmd_network_;
  SceneSeq scenes_;
  ModeSeq modes_;
  Graph *graph_;
  Levelize *levelize_;
  ArcDelayCalc *arc_delay_calc_;
  GraphDelayCalc *graph_delay_calc_;
  Search *search_;
  Latches *latches_;
  Variables *variables_;
  int thread_count_;
  DispatchQueue *dispatch_queue_;
  float sigma_factor_;
};

} // namespace
