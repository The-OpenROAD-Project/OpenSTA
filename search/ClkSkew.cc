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
#include <stack>

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

namespace sta {

using std::abs;

// Source/target clock skew.
class ClkSkew
{
public:
  ClkSkew();
  ClkSkew(PathVertex *src_path,
	  PathVertex *tgt_path,
	  StaState *sta);
  ClkSkew(const ClkSkew &clk_skew);
  void operator=(const ClkSkew &clk_skew);
  PathVertex *srcPath() { return &src_path_; }
  PathVertex *tgtPath() { return &tgt_path_; }
  float srcLatency(StaState *sta);
  float tgtLatency(StaState *sta);
  float srcClkTreeDelay(StaState *sta);
  float tgtClkTreeDelay(StaState *sta);
  Crpr crpr(StaState *sta);
  float skew() const { return skew_; }

private:
  float clkTreeDelay(PathVertex &clk_path,
                     StaState *sta);

  PathVertex src_path_;
  PathVertex tgt_path_;
  float skew_;
};

ClkSkew::ClkSkew() :
  skew_(0.0)
{
}

ClkSkew::ClkSkew(PathVertex *src_path,
		 PathVertex *tgt_path,
		 StaState *sta)
{
  src_path_ = src_path;
  tgt_path_ = tgt_path;
  skew_ = srcLatency(sta) - tgtLatency(sta) - delayAsFloat(crpr(sta));
}

ClkSkew::ClkSkew(const ClkSkew &clk_skew)
{
  src_path_ = clk_skew.src_path_;
  tgt_path_ = clk_skew.tgt_path_;
  skew_ = clk_skew.skew_;
}

void
ClkSkew::operator=(const ClkSkew &clk_skew)
{
  src_path_ = clk_skew.src_path_;
  tgt_path_ = clk_skew.tgt_path_;
  skew_ = clk_skew.skew_;
}

float
ClkSkew::srcLatency(StaState *sta)
{
  Arrival src_arrival = src_path_.arrival(sta);
  return delayAsFloat(src_arrival) - src_path_.clkEdge(sta)->time()
    + clkTreeDelay(src_path_, sta);
}

float
ClkSkew::srcClkTreeDelay(StaState *sta)
{
  return clkTreeDelay(src_path_, sta);
}

float
ClkSkew::tgtLatency(StaState *sta)
{
  Arrival tgt_arrival = tgt_path_.arrival(sta);
  return delayAsFloat(tgt_arrival) - tgt_path_.clkEdge(sta)->time()
    + clkTreeDelay(tgt_path_, sta);
}

float
ClkSkew::tgtClkTreeDelay(StaState *sta)
{
  return clkTreeDelay(tgt_path_, sta);
}

float
ClkSkew::clkTreeDelay(PathVertex &clk_path,
                      StaState *sta)
{
  const Vertex *vertex = clk_path.vertex(sta);
  const Pin *pin = vertex->pin();
  const LibertyPort *port = sta->network()->libertyPort(pin);
  const MinMax *min_max = clk_path.minMax(sta);
  const RiseFall *rf = clk_path.transition(sta);
  Slew slew = clk_path.slew(sta);
  return port->clkTreeDelay(slew, rf, min_max);
}

Crpr
ClkSkew::crpr(StaState *sta)
{
  CheckCrpr *check_crpr = sta->search()->checkCrpr();
  return check_crpr->checkCrpr(&src_path_, &tgt_path_);
}

////////////////////////////////////////////////////////////////

ClkSkews::ClkSkews(StaState *sta) :
  StaState(sta)
{
}

void
ClkSkews::reportClkSkew(ConstClockSeq clks,
			const Corner *corner,
			const SetupHold *setup_hold,
			int digits)
{
  ClkSkewMap skews = findClkSkew(clks, corner, setup_hold);

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
  float src_clk_tree_delay = clk_skew.srcClkTreeDelay(this);
  float tgt_clk_tree_delay = clk_skew.tgtClkTreeDelay(this);

  if (src_clk_tree_delay != 0.0)
    src_latency -= src_clk_tree_delay;
  report_->reportLine("%7s source latency %s %s",
                      time_unit->asString(src_latency, digits),
                      sdc_network_->pathName(src_path->pin(this)),
                      src_path->transition(this)->asString());
  if (src_clk_tree_delay != 0.0)
    report_->reportLine("%7s source clock tree delay",
                        time_unit->asString(src_clk_tree_delay, digits));

  if (tgt_clk_tree_delay != 0.0)
    tgt_latency -= tgt_clk_tree_delay;
  report_->reportLine("%7s target latency %s %s",
                      time_unit->asString(-tgt_latency, digits),
                      sdc_network_->pathName(tgt_path->pin(this)),
                      tgt_path->transition(this)->asString());
  if (tgt_clk_tree_delay != 0.0)
    report_->reportLine("%7s target clock tree delay",
                        time_unit->asString(-tgt_clk_tree_delay, digits));

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
                           const SetupHold *setup_hold)
{
  ConstClockSeq clks;
  for (const Clock *clk : *sdc_->clocks())
    clks.push_back(clk);
  ClkSkewMap skews = findClkSkew(clks, corner, setup_hold);
  float worst_skew = 0.0;
  for (auto clk_skew_itr : skews) {
    ClkSkew &clk_skew = clk_skew_itr.second;
    float skew = clk_skew.skew();
    if (abs(skew) > abs(worst_skew))
      worst_skew = skew;
  }
  return worst_skew;
}

ClkSkewMap
ClkSkews::findClkSkew(ConstClockSeq &clks,
		      const Corner *corner,
		      const SetupHold *setup_hold)
{	      
  ClkSkewMap skews;

  ConstClockSet clk_set;
  for (const Clock *clk : clks)
    clk_set.insert(clk);
  const auto findClkSkew = [this, corner, setup_hold, &clk_set](Vertex *src_vertex, ClkSkewMap &skews) {
    VertexOutEdgeIterator edge_iter(src_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role()->genericRole() != TimingRole::regClkToQ()) continue;
      Vertex *q_vertex = edge->to(graph_);
      RiseFall *rf = edge->timingArcSet()->isRisingFallingEdge();
      RiseFallBoth *src_rf = rf
          ? rf->asRiseFallBoth()
          : RiseFallBoth::riseFall();
      findClkSkewFrom(src_vertex, q_vertex, src_rf, clk_set, corner, setup_hold, skews);
    }
  };

  if (threadCount() > 1) {
    std::vector<ClkSkewMap> partial_skews(thread_count_, skews);
    for (Vertex *src_vertex : *graph_->regClkVertices()) {
      if (!hasClkPaths(src_vertex, clk_set)) continue;
      dispatch_queue_->dispatch([findClkSkew, src_vertex, &partial_skews](int i) {
        findClkSkew(src_vertex, partial_skews[i]);
      });
    }
    dispatch_queue_->finishTasks();
    for (size_t i = 0; i < partial_skews.size(); i++) {
      for (auto [clk, partial_skew] : partial_skews[i]) {
        auto ins = skews.insert(std::make_pair(clk, partial_skew));
        if (ins.second) continue;
        ClkSkew &final_skew = ins.first->second;
        if (abs(partial_skew.skew()) > abs(final_skew.skew())) {
          final_skew = partial_skew;
        }
      }
    }
  }
  else {
    for (Vertex *src_vertex : *graph_->regClkVertices()) {
      if (!hasClkPaths(src_vertex, clk_set)) continue;
      findClkSkew(src_vertex, skews);
    }
  }
  return skews;
}

bool
ClkSkews::hasClkPaths(Vertex *vertex,
		      ConstClockSet &clks)
{
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    const Clock *path_clk = path->clock(this);
    if (clks.find(path_clk) != clks.end())
      return true;
  }
  return false;
}

void
ClkSkews::findClkSkewFrom(Vertex *src_vertex,
			  Vertex *q_vertex,
			  const RiseFallBoth *src_rf,
                          ConstClockSet &clk_set,
			  const Corner *corner,
			  const SetupHold *setup_hold,
			  ClkSkewMap &skews)
{
  VertexSet endpoints = findFanout(q_vertex);
  for (Vertex *end : endpoints) {
    VertexInEdgeIterator edge_iter(end, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      TimingRole *role = edge->role();
      if (role->isTimingCheck()
	  && ((setup_hold == SetupHold::max()
	       && role->genericRole() == TimingRole::setup())
	      || ((setup_hold == SetupHold::min()
		   && role->genericRole() == TimingRole::hold())))) {
	Vertex *tgt_vertex = edge->from(graph_);
	const RiseFall *tgt_rf1 = edge->timingArcSet()->isRisingFallingEdge();
	const RiseFallBoth *tgt_rf = tgt_rf1
	  ? tgt_rf1->asRiseFallBoth()
	  : RiseFallBoth::riseFall();
	findClkSkew(src_vertex, src_rf, tgt_vertex, tgt_rf,
		    clk_set, corner, setup_hold, skews);
      }
    }
  }
}

void
ClkSkews::findClkSkew(Vertex *src_vertex,
		      const RiseFallBoth *src_rf,
		      Vertex *tgt_vertex,
		      const RiseFallBoth *tgt_rf,
                      ConstClockSet &clk_set,
		      const Corner *corner,
		      const SetupHold *setup_hold,
                      ClkSkewMap &skews)
{
  Unit *time_unit = units_->timeUnit();
  const SetupHold *tgt_min_max = setup_hold->opposite();
  VertexPathIterator src_iter(src_vertex, this);
  while (src_iter.hasNext()) {
    PathVertex *src_path = src_iter.next();
    const Clock *src_clk = src_path->clock(this);
    if (src_rf->matches(src_path->transition(this))
	&& src_path->minMax(this) == setup_hold
	&& clk_set.find(src_clk) != clk_set.end()) {
      Corner *src_corner = src_path->pathAnalysisPt(this)->corner();
      if (corner == nullptr
	  || src_corner == corner) {
	VertexPathIterator tgt_iter(tgt_vertex, this);
	while (tgt_iter.hasNext()) {
	  PathVertex *tgt_path = tgt_iter.next();
	  const Clock *tgt_clk = tgt_path->clock(this);
	  if (tgt_clk == src_clk
	      && tgt_path->isClock(this)
	      && tgt_rf->matches(tgt_path->transition(this))
	      && tgt_path->minMax(this) == tgt_min_max
	      && tgt_path->pathAnalysisPt(this)->corner() == src_corner) {
	    ClkSkew probe(src_path, tgt_path, this);
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

class FanOutSrchPred : public SearchPred1
{
public:
  FanOutSrchPred(const StaState *sta);
  virtual bool searchThru(Edge *edge);
};

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

VertexSet
ClkSkews::findFanout(Vertex *from)
{
  debugPrint(debug_, "fanout", 1, "%s",
             from->name(sdc_network_));
  VertexSet endpoints(graph_);
  FanOutSrchPred pred(this);
  if (threadCount() > 1) {
    // Get the fanout with depth-first search.
    // Breadth-first would work too, but a stack is slightly faster than a queue.
    // This is called from multiple threads, so BfsVisitor cannot be used, as it modifies vertex state.
    thread_local static std::unordered_set<Vertex *> visited;
    thread_local static std::stack<Vertex *, std::vector<Vertex *>> stack;
    visited.clear();
    stack.push(from);
    visited.insert(from);
    while (!stack.empty()) {
      Vertex *fanout = stack.top();
      stack.pop();
      if (fanout->hasChecks()) {
        debugPrint(debug_, "fanout", 1, " endpoint %s",
                   fanout->name(sdc_network_));
        endpoints.insert(fanout);
      }
      if (!pred.searchFrom(fanout)) continue;
      VertexOutEdgeIterator edge_iter(fanout, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        Vertex *to = edge->to(graph_);
        if (pred.searchThru(edge) && pred.searchTo(to) && visited.insert(to).second) {
          stack.push(to);
        }
      }
    }
  }
  else {
    BfsFwdIterator fanout_iter(BfsIndex::other, &pred, this);
    fanout_iter.enqueue(from);
    while (fanout_iter.hasNext()) {
      Vertex *fanout = fanout_iter.next();
      if (fanout->hasChecks()) {
        debugPrint(debug_, "fanout", 1, " endpoint %s",
                   fanout->name(sdc_network_));
        endpoints.insert(fanout);
      }
      fanout_iter.enqueueAdjacentVertices(fanout);
    }
  }
  return endpoints;
}

} // namespace
