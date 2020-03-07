// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "DisallowCopyAssign.hh"

namespace sta {

class Report;
class Debug;
class Units;
class Network;
class NetworkEdit;
class NetworkReader;
class Sdc;
class Corners;
class Graph;
class Levelize;
class Sim;
class Search;
class Parasitics;
class ArcDelayCalc;
class GraphDelayCalc;
class Latches;
class DispatchQueue;

// Most STA components use functionality in other components.
// This class simplifies the process of copying pointers to the
// components.  It is deliberately simple to minimize circular
// dependencies between the it and the components.
class StaState
{
public:
  // Make an empty state.
  StaState();
  explicit StaState(const StaState *sta);
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
  Sdc *sdc() { return sdc_; }
  Sdc *sdc() const { return sdc_; }
  Corners *corners() { return corners_; }
  Corners *corners() const { return corners_; }
  Graph *graph() { return graph_; }
  Graph *graph() const { return graph_; }
  Levelize *levelize() { return levelize_; }
  Levelize *levelize() const { return levelize_; }
  Parasitics *parasitics() { return parasitics_; }
  Parasitics *parasitics() const { return parasitics_; }
  ArcDelayCalc *arcDelayCalc() { return arc_delay_calc_; }
  ArcDelayCalc *arcDelayCalc() const { return arc_delay_calc_; }
  GraphDelayCalc *graphDelayCalc() { return graph_delay_calc_; }
  GraphDelayCalc *graphDelayCalc() const { return graph_delay_calc_; }
  Sim *sim() { return sim_; }
  Sim *sim() const { return sim_; }
  Search *search() { return search_; }
  Search *search() const { return search_; }
  Latches *latches() { return latches_; }
  Latches *latches() const { return latches_; }
  unsigned threadCount() const { return thread_count_; }
  bool pocvEnabled() const { return pocv_enabled_; }
  float sigmaFactor() const { return sigma_factor_; }

protected:
  Report *report_;
  Debug *debug_;
  Units *units_;
  Network *network_;
  Network *sdc_network_;
  // Network used by command interpreter (SdcNetwork).
  Network *cmd_network_;
  Sdc *sdc_;
  Corners *corners_;
  Graph *graph_;
  Levelize *levelize_;
  Parasitics *parasitics_;
  ArcDelayCalc *arc_delay_calc_;
  GraphDelayCalc *graph_delay_calc_;
  Sim *sim_;
  Search *search_;
  Latches *latches_;
  int thread_count_;
  DispatchQueue *dispatch_queue_;
  bool pocv_enabled_;
  float sigma_factor_;

private:
  DISALLOW_COPY_AND_ASSIGN(StaState);
};

} // namespace
