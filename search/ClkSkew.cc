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

#include "ClkSkew.hh"

#include <cmath> // abs
#include <algorithm>

#include "Fuzzy.hh"
#include "Report.hh"
#include "Debug.hh"
#include "DispatchQueue.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Bfs.hh"
#include "PathVertex.hh"
#include "StaState.hh"
#include "PathAnalysisPt.hh"
#include "SearchPred.hh"
#include "Search.hh"
#include "Crpr.hh"
#include "PathEnd.hh"

namespace sta {

using std::abs;

// Source/target clock skew.
class ClkSkew
{
public:
  ClkSkew();
  ClkSkew(PathVertex *src_path,
	  PathVertex *tgt_path,
          bool include_internal_latency,
	  StaState *sta);
  ClkSkew(const ClkSkew &clk_skew);
  void operator=(const ClkSkew &clk_skew);
  PathVertex *srcPath() { return &src_path_; }
  PathVertex *tgtPath() { return &tgt_path_; }
  float srcLatency(const StaState *sta);
  float tgtLatency(const StaState *sta);
  float srcInternalClkLatency(const StaState *sta);
  float tgtInternalClkLatency(const StaState *sta);
  Crpr crpr(const StaState *sta);
  float uncertainty(const StaState *sta);
  float skew() const { return skew_; }
  static bool srcTgtPathNameLess(ClkSkew &clk_skew1,
                                 ClkSkew &clk_skew2,
                                 const StaState *sta);

private:
  float clkTreeDelay(PathVertex &clk_path,
                     const StaState *sta);

  PathVertex src_path_;
  PathVertex tgt_path_;
  bool include_internal_latency_;
  float skew_;
};

ClkSkew::ClkSkew() :
  include_internal_latency_(false),
  skew_(0.0)
{
}

ClkSkew::ClkSkew(PathVertex *src_path,
		 PathVertex *tgt_path,
                 bool include_internal_latency,
		 StaState *sta) :
  src_path_(src_path),
  tgt_path_(tgt_path),
  include_internal_latency_(include_internal_latency)
{
  skew_ = srcLatency(sta)
    - tgtLatency(sta)
    - delayAsFloat(crpr(sta))
    + uncertainty(sta);
}

ClkSkew::ClkSkew(const ClkSkew &clk_skew)
{
  src_path_ = clk_skew.src_path_;
  tgt_path_ = clk_skew.tgt_path_;
  include_internal_latency_ = clk_skew.include_internal_latency_;
  skew_ = clk_skew.skew_;
}

void
ClkSkew::operator=(const ClkSkew &clk_skew)
{
  src_path_ = clk_skew.src_path_;
  tgt_path_ = clk_skew.tgt_path_;
  include_internal_latency_ = clk_skew.include_internal_latency_;
  skew_ = clk_skew.skew_;
}

float
ClkSkew::srcLatency(const StaState *sta)
{
  Arrival src_arrival = src_path_.arrival(sta);
  return delayAsFloat(src_arrival) - src_path_.clkEdge(sta)->time()
    + clkTreeDelay(src_path_, sta);
}

float
ClkSkew::srcInternalClkLatency(const StaState *sta)
{
  return clkTreeDelay(src_path_, sta);
}

float
ClkSkew::tgtLatency(const StaState *sta)
{
  Arrival tgt_arrival = tgt_path_.arrival(sta);
  return delayAsFloat(tgt_arrival) - tgt_path_.clkEdge(sta)->time()
    + clkTreeDelay(tgt_path_, sta);
}

float
ClkSkew::tgtInternalClkLatency(const StaState *sta)
{
  return clkTreeDelay(tgt_path_, sta);
}

float
ClkSkew::clkTreeDelay(PathVertex &clk_path,
                      const StaState *sta)
{
  if (include_internal_latency_) {
    const Vertex *vertex = clk_path.vertex(sta);
    const Pin *pin = vertex->pin();
    const LibertyPort *port = sta->network()->libertyPort(pin);
    const MinMax *min_max = clk_path.minMax(sta);
    const RiseFall *rf = clk_path.transition(sta);
    float slew = delayAsFloat(clk_path.slew(sta));
    return port->clkTreeDelay(slew, rf, min_max);
  }
  else
    return 0.0;
}

Crpr
ClkSkew::crpr(const StaState *sta)
{
  CheckCrpr *check_crpr = sta->search()->checkCrpr();
  return check_crpr->checkCrpr(&src_path_, &tgt_path_);
}

float
ClkSkew::uncertainty(const StaState *sta)
{
  TimingRole *check_role = (src_path_.minMax(sta) == SetupHold::max())
    ? TimingRole::setup()
    : TimingRole::hold();
  // Uncertainty decreases slack, but increases skew.
  return -PathEnd::checkTgtClkUncertainty(&tgt_path_, tgt_path_.clkEdge(sta),
                                          check_role, sta);
}

bool
ClkSkew::srcTgtPathNameLess(ClkSkew &clk_skew1,
                            ClkSkew &clk_skew2,
                            const StaState *sta)
{
  Network *network = sta->sdcNetwork();
  const char *src_path1 = network->pathName(clk_skew1.srcPath()->pin(sta));
  const char *src_path2 = network->pathName(clk_skew2.srcPath()->pin(sta));
  const char *tgt_path1 = network->pathName(clk_skew1.tgtPath()->pin(sta));
  const char *tgt_path2 = network->pathName(clk_skew2.tgtPath()->pin(sta));
  return stringLess(src_path1, src_path2)
    || (stringEqual(src_path1, src_path2)
        && stringEqual(tgt_path1, tgt_path2));
}


////////////////////////////////////////////////////////////////

ClkSkews::ClkSkews(StaState *sta) :
  StaState(sta),
  fanout_pred_(sta)
{
}

void
ClkSkews::reportClkSkew(ConstClockSeq &clks,
			const Corner *corner,
			const SetupHold *setup_hold,
                        bool include_internal_latency,
			int digits)
{
  ClkSkewMap skews = findClkSkew(clks, corner, setup_hold,
                                 include_internal_latency);

  // Sort the clocks to report in a stable order.
  ConstClockSeq sorted_clks;
  for (const Clock *clk : clks)
    sorted_clks.push_back(clk);
  std::sort(sorted_clks.begin(), sorted_clks.end(), ClkNameLess());

  for (const Clock *clk : sorted_clks) {
    report_->reportLine("Clock %s", clk->name());
    auto skew_itr = skews.find(clk);
    if (skew_itr != skews.end())
      reportClkSkew(skew_itr->second, digits);
    else
      report_->reportLine("No launch/capture paths found.");
    report_->reportBlankLine();
  }
}

void
ClkSkews::reportClkSkew(ClkSkew &clk_skew,
			int digits)
{
  Unit *time_unit = units_->timeUnit();
  PathVertex *src_path = clk_skew.srcPath();
  PathVertex *tgt_path = clk_skew.tgtPath();
  float src_latency = clk_skew.srcLatency(this);
  float tgt_latency = clk_skew.tgtLatency(this);
  float src_internal_clk_latency = clk_skew.srcInternalClkLatency(this);
  float tgt_internal_clk_latency = clk_skew.tgtInternalClkLatency(this);
  float uncertainty = clk_skew.uncertainty(this);

  if (src_internal_clk_latency != 0.0)
    src_latency -= src_internal_clk_latency;
  report_->reportLine("%7s source latency %s %s",
                      time_unit->asString(src_latency, digits),
                      sdc_network_->pathName(src_path->pin(this)),
                      src_path->transition(this)->asString());
  if (src_internal_clk_latency != 0.0)
    report_->reportLine("%7s source internal clock delay",
                        time_unit->asString(src_internal_clk_latency, digits));

  if (tgt_internal_clk_latency != 0.0)
    tgt_latency -= tgt_internal_clk_latency;
  report_->reportLine("%7s target latency %s %s",
                      time_unit->asString(-tgt_latency, digits),
                      sdc_network_->pathName(tgt_path->pin(this)),
                      tgt_path->transition(this)->asString());
  if (tgt_internal_clk_latency != 0.0)
    report_->reportLine("%7s target internal clock delay",
                        time_unit->asString(-tgt_internal_clk_latency, digits));
  if (uncertainty != 0.0)
    report_->reportLine("%7s clock uncertainty",
                        time_unit->asString(uncertainty, digits));
  report_->reportLine("%7s CRPR",
                      time_unit->asString(delayAsFloat(-clk_skew.crpr(this)),
                                          digits));
  report_->reportLine("--------------");
  report_->reportLine("%7s %s skew",
                      time_unit->asString(clk_skew.skew(), digits),
                      src_path->minMax(this) == MinMax::max() ? "setup" : "hold");
}

float
ClkSkews::findWorstClkSkew(const Corner *corner,
                           const SetupHold *setup_hold,
                           bool include_internal_latency)
{
  ConstClockSeq clks;
  for (const Clock *clk : *sdc_->clocks())
    clks.push_back(clk);
  ClkSkewMap skews = findClkSkew(clks, corner, setup_hold, include_internal_latency);
  float worst_skew = 0.0;
  for (const auto& [clk, clk_skew] : skews) {
    float skew = clk_skew.skew();
    if (abs(skew) > abs(worst_skew))
      worst_skew = skew;
  }
  return worst_skew;
}

ClkSkewMap
ClkSkews::findClkSkew(ConstClockSeq &clks,
		      const Corner *corner,
		      const SetupHold *setup_hold,
                      bool include_internal_latency)
{	      
  ClkSkewMap skews;
  corner_ = corner;
  setup_hold_ = setup_hold;
  include_internal_latency_ = include_internal_latency;

  clk_set_.clear();
  for (const Clock *clk : clks)
    clk_set_.insert(clk);

  if (thread_count_ > 1) {
    std::vector<ClkSkewMap> partial_skews(thread_count_, skews);
    for (Vertex *src_vertex : *graph_->regClkVertices()) {
      if (hasClkPaths(src_vertex)) {
        dispatch_queue_->dispatch([this, src_vertex, &partial_skews](int i) {
          findClkSkewFrom(src_vertex, partial_skews[i]);
        });
      }
    }
    dispatch_queue_->finishTasks();

    // Reduce skews from each register source.
    for (size_t i = 0; i < partial_skews.size(); i++) {
      for (auto& [clk, partial_skew] : partial_skews[i]) {
        auto ins = skews.insert(std::make_pair(clk, partial_skew));
        if (!ins.second) {
          ClkSkew &final_skew = ins.first->second;
          if (abs(partial_skew.skew()) > abs(final_skew.skew())
              || (fuzzyEqual(abs(partial_skew.skew()), abs(final_skew.skew()))
                  // Break ties based on source/target path names.
                  && ClkSkew::srcTgtPathNameLess(partial_skew, final_skew, this)))
            final_skew = partial_skew;
        }
      }
    }
  }
  else {
    for (Vertex *src_vertex : *graph_->regClkVertices()) {
      if (hasClkPaths(src_vertex))
        findClkSkewFrom(src_vertex, skews);
    }
  }
  return skews;
}

bool
ClkSkews::hasClkPaths(Vertex *vertex)
{
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    const Clock *path_clk = path->clock(this);
    if (clk_set_.find(path_clk) != clk_set_.end())
      return true;
  }
  return false;
}

void
ClkSkews::findClkSkewFrom(Vertex *src_vertex,
                          ClkSkewMap &skews)
{
  VertexOutEdgeIterator edge_iter(src_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->role()->genericRole() == TimingRole::regClkToQ()) {
      Vertex *q_vertex = edge->to(graph_);
      const RiseFall *rf = edge->timingArcSet()->isRisingFallingEdge();
      const RiseFallBoth *src_rf = rf
        ? rf->asRiseFallBoth()
        : RiseFallBoth::riseFall();
      findClkSkewFrom(src_vertex, q_vertex, src_rf, skews);
    }
  }
}

void
ClkSkews::findClkSkewFrom(Vertex *src_vertex,
			  Vertex *q_vertex,
			  const RiseFallBoth *src_rf,
			  ClkSkewMap &skews)
{
  VertexSet endpoints = findFanout(q_vertex);
  for (Vertex *end : endpoints) {
    VertexInEdgeIterator edge_iter(end, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      TimingRole *role = edge->role();
      if (role->isTimingCheck()
	  && ((setup_hold_ == SetupHold::max()
	       && role->genericRole() == TimingRole::setup())
	      || ((setup_hold_ == SetupHold::min()
		   && role->genericRole() == TimingRole::hold())))) {
	Vertex *tgt_vertex = edge->from(graph_);
	const RiseFall *tgt_rf1 = edge->timingArcSet()->isRisingFallingEdge();
	const RiseFallBoth *tgt_rf = tgt_rf1
	  ? tgt_rf1->asRiseFallBoth()
	  : RiseFallBoth::riseFall();
	findClkSkew(src_vertex, src_rf, tgt_vertex, tgt_rf, skews);
      }
    }
  }
}

void
ClkSkews::findClkSkew(Vertex *src_vertex,
		      const RiseFallBoth *src_rf,
		      Vertex *tgt_vertex,
		      const RiseFallBoth *tgt_rf,
                      ClkSkewMap &skews)
{
  Unit *time_unit = units_->timeUnit();
  const SetupHold *tgt_min_max = setup_hold_->opposite();
  VertexPathIterator src_iter(src_vertex, this);
  while (src_iter.hasNext()) {
    PathVertex *src_path = src_iter.next();
    const Clock *src_clk = src_path->clock(this);
    if (src_rf->matches(src_path->transition(this))
	&& src_path->minMax(this) == setup_hold_
	&& clk_set_.find(src_clk) != clk_set_.end()) {
      Corner *src_corner = src_path->pathAnalysisPt(this)->corner();
      if (corner_ == nullptr
	  || src_corner == corner_) {
	VertexPathIterator tgt_iter(tgt_vertex, this);
	while (tgt_iter.hasNext()) {
	  PathVertex *tgt_path = tgt_iter.next();
	  const Clock *tgt_clk = tgt_path->clock(this);
	  if (tgt_clk == src_clk
	      && tgt_path->isClock(this)
	      && tgt_rf->matches(tgt_path->transition(this))
	      && tgt_path->minMax(this) == tgt_min_max
	      && tgt_path->pathAnalysisPt(this)->corner() == src_corner) {
	    ClkSkew probe(src_path, tgt_path, include_internal_latency_, this);
	    ClkSkew &clk_skew = skews[src_clk];
	    debugPrint(debug_, "clk_skew", 2,
                       "%s %s %s -> %s %s %s crpr = %s skew = %s",
                       network_->pathName(src_path->pin(this)),
                       src_path->transition(this)->asString(),
                       time_unit->asString(probe.srcLatency(this)),
                       network_->pathName(tgt_path->pin(this)),
                       tgt_path->transition(this)->asString(),
                       time_unit->asString(probe.tgtLatency(this)),
                       delayAsString(probe.crpr(this), this),
                       time_unit->asString(probe.skew()));
	    if (clk_skew.srcPath()->isNull()
                || abs(probe.skew()) > abs(clk_skew.skew()))
	      clk_skew = probe;
	  }
	}
      }
    }
  }
}

VertexSet
ClkSkews::findFanout(Vertex *from)
{
  VertexSet endpoints(graph_);
  UnorderedSet<Vertex*> visited;
  findFanout1(from, visited, endpoints);
  return endpoints;
}

void
ClkSkews::findFanout1(Vertex *from,
                      UnorderedSet<Vertex*> &visited,
                      VertexSet &endpoints)
{
  visited.insert(from);
  if (from->hasChecks())
    endpoints.insert(from);
  if (fanout_pred_.searchFrom(from)) {
    VertexOutEdgeIterator edge_iter(from, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to = edge->to(graph_);
      if (fanout_pred_.searchThru(edge)
          && fanout_pred_.searchTo(to)
          // Do not revisit downstream fanout cones.
          && visited.insert(to).second)
        findFanout1(to, visited, endpoints);
    }
  }
}

////////////////////////////////////////////////////////////////

FanOutSrchPred::FanOutSrchPred(const StaState *sta) :
  SearchPred1(sta)
{
}

bool
FanOutSrchPred::searchThru(Edge *edge)
{
  TimingRole *role = edge->role();
  return SearchPred1::searchThru(edge)
    && (role == TimingRole::wire()
        || role == TimingRole::combinational()
        || role == TimingRole::tristateEnable()
        || role == TimingRole::tristateDisable());
}

} // namespace
