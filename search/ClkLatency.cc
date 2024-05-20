// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#include "ClkLatency.hh"

#include <algorithm>

#include "Report.hh"
#include "Debug.hh"
#include "Units.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Clock.hh"
#include "Graph.hh"
#include "PathVertex.hh"
#include "StaState.hh"
#include "Search.hh"
#include "PathAnalysisPt.hh"
#include "ClkInfo.hh"

namespace sta {

ClkLatency::ClkLatency(StaState *sta) :
  StaState(sta)
{
}

ClkDelays
ClkLatency::findClkDelays(const Clock *clk,
                          const Corner *corner,
                          bool include_internal_latency)
{
  ConstClockSeq clks;
  clks.push_back(clk);
  ClkDelayMap clk_delay_map = findClkDelays(clks, corner,
                                            include_internal_latency);
  return clk_delay_map[clk];
}

void
ClkLatency::reportClkLatency(ConstClockSeq &clks,
                             const Corner *corner,
                             bool include_internal_latency,
                             int digits)
{
  ClkDelayMap clk_delay_map = findClkDelays(clks, corner, include_internal_latency);

  // Sort the clocks to report in a stable order.
  ConstClockSeq sorted_clks;
  for (const Clock *clk : clks)
    sorted_clks.push_back(clk);
  std::sort(sorted_clks.begin(), sorted_clks.end(), ClkNameLess());

  for (const Clock *clk : sorted_clks) {
    ClkDelays clk_delays = clk_delay_map[clk];
    reportClkLatency(clk, clk_delays, digits);
    report_->reportBlankLine();
  }
}

void
ClkLatency::reportClkLatency(const Clock *clk,
                             ClkDelays &clk_delays,
                             int digits)
{
  Unit *time_unit = units_->timeUnit();
  report_->reportLine("Clock %s", clk->name());
  for (const RiseFall *src_rf : RiseFall::range()) {
    for (const RiseFall *end_rf : RiseFall::range()) {
      PathVertex path_min;
      Delay insertion_min;
      Delay delay_min;
      float internal_latency_min;
      Delay latency_min;
      bool exists_min;
      clk_delays.delay(src_rf, end_rf, MinMax::min(), insertion_min,
                       delay_min, internal_latency_min, latency_min,
                       path_min, exists_min);
      PathVertex path_max;
      Delay insertion_max;
      Delay delay_max;
      float internal_latency_max;
      Delay latency_max;
      bool exists_max;
      clk_delays.delay(src_rf, end_rf, MinMax::max(), insertion_max,
                       delay_max, internal_latency_max, latency_max,
                       path_max, exists_max);
      if (exists_min & exists_max) {
        report_->reportLine("%s -> %s",
                            src_rf->name(),
                            end_rf->name());
        report_->reportLine("    min     max");

        report_->reportLine("%7s %7s source latency",
                            delayAsString(insertion_min, this, digits),
                            delayAsString(insertion_max, this, digits));
        report_->reportLine("%7s %7s network latency %s",
                            delayAsString(delay_min, this, digits),
                            "",
                            sdc_network_->pathName(path_min.pin(this)));
        report_->reportLine("%7s %7s network latency %s",
                            "",
                            delayAsString(delay_max, this, digits),
                            sdc_network_->pathName(path_max.pin(this)));
        if (internal_latency_min != 0.0
            || internal_latency_max != 0.0)
          report_->reportLine("%7s %7s internal clock latency",
                              time_unit->asString(internal_latency_min, digits),
                              time_unit->asString(internal_latency_max, digits));
        report_->reportLine("---------------");
        report_->reportLine("%7s %7s latency",
                            delayAsString(latency_min, this, digits),
                            delayAsString(latency_max, this, digits));
        Delay skew = latency_max - latency_min;
        report_->reportLine("        %7s skew",
                            delayAsString(skew, this, digits));
        report_->reportBlankLine();
      }
    }
  }
}

ClkDelayMap
ClkLatency::findClkDelays(ConstClockSeq &clks,
                          const Corner *corner,
                          bool include_internal_latency)
{
  ClkDelayMap clk_delay_map;
  // Make entries for the relevant clocks to filter path clocks.
  for (const Clock *clk : clks)
    clk_delay_map[clk];
  for (Vertex *clk_vertex : *graph_->regClkVertices()) {
    VertexPathIterator path_iter(clk_vertex, this);
    while (path_iter.hasNext()) {
      PathVertex *path = path_iter.next();
      const ClockEdge *path_clk_edge = path->clkEdge(this);
      const PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
      if (path_clk_edge
          && (corner == nullptr
              || path_ap->corner() == corner)) {
        const Clock *path_clk = path_clk_edge->clock();
        auto delays_itr = clk_delay_map.find(path_clk);
        if (delays_itr != clk_delay_map.end()) {
          ClkDelays &clk_delays = delays_itr->second;
          const RiseFall *clk_rf = path_clk_edge->transition();
          const MinMax *min_max = path->minMax(this);
          const RiseFall *end_rf = path->transition(this);
          Delay latency = ClkDelays::latency(path, this);
          Delay clk_latency;
          bool exists;
          clk_delays.latency(clk_rf, end_rf, min_max, clk_latency, exists);
          if (!exists || delayGreater(latency, clk_latency, min_max, this))
            clk_delays.setLatency(clk_rf, end_rf, min_max, path,
                                  include_internal_latency, this);
        }
      }
    }
  }
  return clk_delay_map;
}

////////////////////////////////////////////////////////////////

ClkDelays::ClkDelays()
{
  for (auto src_rf_index : RiseFall::rangeIndex()) {
    for (auto end_rf_index : RiseFall::rangeIndex()) {
      for (auto mm_index : MinMax::rangeIndex()) {
        insertion_[src_rf_index][end_rf_index][mm_index] = 0.0;
        delay_[src_rf_index][end_rf_index][mm_index] = 0.0;
        internal_latency_[src_rf_index][end_rf_index][mm_index] = 0.0;
        latency_[src_rf_index][end_rf_index][mm_index] = 0.0;
        exists_[src_rf_index][end_rf_index][mm_index] = false;
      }
    }
  }
}

void
ClkDelays::delay(const RiseFall *src_rf,
                 const RiseFall *end_rf,
                 const MinMax *min_max,
                 // Return values.
                 Delay &insertion,
                 Delay &delay,
                 float &lib_clk_delay,
                 Delay &latency,
                 PathVertex &path,
                 bool &exists) const
{
  int src_rf_index = src_rf->index();
  int end_rf_index = end_rf->index();
  int mm_index = min_max->index();
  path = path_[src_rf_index][end_rf_index][mm_index];
  insertion = insertion_[src_rf_index][end_rf_index][mm_index];
  delay = delay_[src_rf_index][end_rf_index][mm_index];
  lib_clk_delay = internal_latency_[src_rf_index][end_rf_index][mm_index];
  latency = latency_[src_rf_index][end_rf_index][mm_index];
  exists = exists_[src_rf_index][end_rf_index][mm_index];
}

void
ClkDelays::latency(const RiseFall *src_rf,
                   const RiseFall *end_rf,
                   const MinMax *min_max,
                   // Return values.
                   Delay &latency,
                   bool &exists) const
{
  int src_rf_index = src_rf->index();
  int end_rf_index = end_rf->index();
  int mm_index = min_max->index();
  latency = latency_[src_rf_index][end_rf_index][mm_index];
  exists = exists_[src_rf_index][end_rf_index][mm_index];
}

void
ClkDelays::setLatency(const RiseFall *src_rf,
                      const RiseFall *end_rf,
                      const MinMax *min_max,
                      PathVertex *path,
                      bool include_internal_latency,
                      StaState *sta)
{
  int src_rf_index = src_rf->index();
  int end_rf_index = end_rf->index();
  int mm_index = min_max->index();

  float insertion = insertionDelay(path, sta);
  insertion_[src_rf_index][end_rf_index][mm_index] = insertion;

  float delay1 = delay(path, sta);
  delay_[src_rf_index][end_rf_index][mm_index] = delay1;

  float internal_latency = 0.0;
  if (include_internal_latency) {
    internal_latency = clkTreeDelay(path, sta);
    internal_latency_[src_rf_index][end_rf_index][mm_index] = internal_latency;
  }

  float latency = insertion + delay1 + internal_latency;
  latency_[src_rf_index][end_rf_index][mm_index] = latency;

  path_[src_rf_index][end_rf_index][mm_index] = *path;
  exists_[src_rf_index][end_rf_index][mm_index] = true;
}

Delay
ClkDelays::latency(PathVertex *clk_path,
                   StaState *sta)
{

  float insertion = insertionDelay(clk_path, sta);
  float delay1 = delay(clk_path, sta);
  float lib_clk_delay = clkTreeDelay(clk_path, sta);
  return insertion + delay1 + lib_clk_delay;
}

float
ClkDelays::delay(PathVertex *clk_path,
                 StaState *sta)
{
  Arrival arrival = clk_path->arrival(sta);
  const ClockEdge *path_clk_edge = clk_path->clkEdge(sta);
  return delayAsFloat(arrival) - path_clk_edge->time();
}

float
ClkDelays::insertionDelay(PathVertex *clk_path,
                          StaState *sta)
{
  const ClockEdge *clk_edge = clk_path->clkEdge(sta);
  const Clock *clk = clk_edge->clock();
  const RiseFall *clk_rf = clk_edge->transition();
  ClkInfo *clk_info = clk_path->clkInfo(sta);
  const Pin *src_pin = clk_info->clkSrc();
  const PathAnalysisPt *path_ap = clk_path->pathAnalysisPt(sta);
  const MinMax *min_max = clk_path->minMax(sta);
  return delayAsFloat(sta->search()->clockInsertion(clk, src_pin, clk_rf, min_max,
                                                    min_max, path_ap));
}

float
ClkDelays::clkTreeDelay(PathVertex *clk_path,
                        StaState *sta)
{
  const Vertex *vertex = clk_path->vertex(sta);
  const Pin *pin = vertex->pin();
  const LibertyPort *port = sta->network()->libertyPort(pin);
  const MinMax *min_max = clk_path->minMax(sta);
  const RiseFall *rf = clk_path->transition(sta);
  float slew = delayAsFloat(clk_path->slew(sta));
  return port->clkTreeDelay(slew, rf, min_max);
}

} // namespace
