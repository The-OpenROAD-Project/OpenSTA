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

#include "Machine.hh"
#include "Debug.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "ExceptionPath.hh"
#include "PortDelay.hh"
#include "Sdc.hh"
#include "TimingArc.hh"
#include "Graph.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "PathVertex.hh"
#include "PathAnalysisPt.hh"
#include "PathEnd.hh"
#include "Search.hh"
#include "GatedClk.hh"
#include "VisitPathEnds.hh"

namespace sta {

VisitPathEnds::VisitPathEnds(const StaState *sta) :
  StaState(sta)
{
}

void
VisitPathEnds::visitPathEnds(Vertex *vertex,
			     PathEndVisitor *visitor)
{
  visitPathEnds(vertex, nullptr, MinMaxAll::all(), false, visitor);
}

void
VisitPathEnds::visitPathEnds(Vertex *vertex,
			     const Corner *corner,
			     const MinMaxAll *min_max,
			     bool filtered,
			     PathEndVisitor *visitor)
{
  // Ignore slack on bidirect driver vertex.  The load vertex gets the slack.
  if (!vertex->isBidirectDriver()) {
    const Pin *pin = vertex->pin();
    debugPrint1(debug_, "search", 2, "find end slack %s\n",
		vertex->name(sdc_network_));
    visitor->vertexBegin(vertex);
    bool is_constrained = false;
    visitClkedPathEnds(pin, vertex, corner, min_max, filtered, visitor,
		       is_constrained);
    if (search_->unconstrainedPaths()
	&& !is_constrained
	&& !vertex->isDisabledConstraint())
      visitUnconstrainedPathEnds(pin, vertex, corner, min_max, filtered,
				 visitor);
    visitor->vertexEnd(vertex);
  }
}

void
VisitPathEnds::visitClkedPathEnds(const Pin *pin,
				  Vertex *vertex,
				  const Corner *corner,
				  const MinMaxAll *min_max,
				  bool filtered,
				  PathEndVisitor *visitor,
				  bool &is_constrained)
{
  bool is_segment_start = search_->isSegmentStart(pin);
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
    const MinMax *path_min_max = path_ap->pathMinMax();
    const RiseFall *end_rf = path->transition(this);
    Tag *tag = path->tag(this);
    if ((corner == nullptr
	 || path_ap->corner() == corner)
	&& min_max->matches(path_min_max)
	// Ignore generated clock source paths.
	&& !path->clkInfo(this)->isGenClkSrcPath()
	&& !falsePathTo(path, pin, end_rf, path_min_max)
	// Ignore segment startpoint paths.
	&& (!is_segment_start
	    || !tag->isSegmentStart())) {
      // set_output_delay to timing check has precidence.
      if (sdc_->hasOutputDelay(pin))
	visitOutputDelayEnd(pin, path, end_rf, path_ap, filtered, visitor,
			    is_constrained);
      else if (vertex->hasChecks())
	visitCheckEnd(pin, vertex, path, end_rf, path_ap, filtered, visitor,
		      is_constrained);
      else if (!sdc_->exceptionToInvalid(pin)
	       && (!filtered
		   || search_->matchesFilter(path, nullptr))) {
	PathDelay *path_delay = pathDelayTo(path, pin, end_rf, path_min_max);
	if (path_delay) {
	  PathEndPathDelay path_end(path_delay, path, this);
	  visitor->visit(&path_end);
	  is_constrained = true;
	}
      }
      if (sdc_->gatedClkChecksEnabled())
	visitGatedClkEnd(pin, vertex, path, end_rf, path_ap, filtered, visitor,
			 is_constrained);
      visitDataCheckEnd(pin, path, end_rf, path_ap, filtered, visitor,
			is_constrained);
    }
  }
}

void
VisitPathEnds::visitCheckEnd(const Pin *pin,
			     Vertex *vertex,
			     Path *path,
			     const RiseFall *end_rf,
			     const PathAnalysisPt *path_ap,
			     bool filtered,
			     PathEndVisitor *visitor,
			     bool &is_constrained)
{
  ClockEdge *src_clk_edge = path->clkEdge(this);
  Clock *src_clk = path->clock(this);
  const MinMax *min_max = path_ap->pathMinMax();
  const PathAnalysisPt *tgt_clk_path_ap = path_ap->tgtClkAnalysisPt();
  bool check_clked = false;
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *tgt_clk_vertex = edge->from(graph_);
    const TimingRole *check_role = edge->role();
    if (checkEdgeEnabled(edge)
	&& check_role->pathMinMax() == min_max) {
      TimingArcSet *arc_set = edge->timingArcSet();
      TimingArcSetArcIterator arc_iter(arc_set);
      while (arc_iter.hasNext()) {
	TimingArc *check_arc = arc_iter.next();
	RiseFall *clk_rf = check_arc->fromTrans()->asRiseFall();
	if (check_arc->toTrans()->asRiseFall() == end_rf
	    && clk_rf) {
	  VertexPathIterator tgt_clk_path_iter(tgt_clk_vertex, clk_rf,
					       tgt_clk_path_ap, this);
	  while (tgt_clk_path_iter.hasNext()) {
	    PathVertex *tgt_clk_path = tgt_clk_path_iter.next();
	    ClkInfo *tgt_clk_info = tgt_clk_path->clkInfo(this);
	    const ClockEdge *tgt_clk_edge = tgt_clk_path->clkEdge(this);
	    const Clock *tgt_clk = tgt_clk_path->clock(this);
	    const Pin *tgt_pin = tgt_clk_vertex->pin();
	    ExceptionPath *exception = exceptionTo(path, pin, end_rf,
						   tgt_clk_edge, min_max);
	    // Ignore generated clock source paths.
	    if (!tgt_clk_info->isGenClkSrcPath()
		&& !search_->pathPropagatedToClkSrc(tgt_pin, tgt_clk_path)) {
	      if (tgt_clk_path->isClock(this)) {
		check_clked = true;
		if (!filtered
		    || search_->matchesFilter(path, tgt_clk_edge)) {
		  if (src_clk_edge
		      && tgt_clk != sdc_->defaultArrivalClock()
		      && sdc_->sameClockGroup(src_clk, tgt_clk)
		      && !sdc_->clkStopPropagation(tgt_pin, tgt_clk)
		      && (search_->checkDefaultArrivalPaths()
			  || src_clk_edge
			  != sdc_->defaultArrivalClockEdge())
		      // False paths and path delays override
		      // paths.
		      && (exception == nullptr
			  || exception->isFilter()
			  || exception->isGroupPath()
			  || exception->isMultiCycle())) {
		    MultiCyclePath *mcp=dynamic_cast<MultiCyclePath*>(exception);
		    if (network_->isLatchData(pin)
			&& check_role == TimingRole::setup()) {
		      PathEndLatchCheck path_end(path, check_arc, edge,
						 tgt_clk_path, mcp, nullptr,
						 this);
		      visitor->visit(&path_end);
		      is_constrained = true;
		    }
		    else {
		      PathEndCheck path_end(path, check_arc, edge,
					    tgt_clk_path, mcp, this);
		      visitor->visit(&path_end);
		      is_constrained = true;
		    }
		  }
		  else if (exception
			   && exception->isPathDelay()
			   && (src_clk == nullptr
			       || sdc_->sameClockGroup(src_clk,
							       tgt_clk))) {
		    PathDelay *path_delay = dynamic_cast<PathDelay*>(exception);
		    if (network_->isLatchData(pin)
			&& check_role == TimingRole::setup()) {
		      PathEndLatchCheck path_end(path, check_arc, edge,
						 tgt_clk_path, nullptr,
						 path_delay, this);
		      visitor->visit(&path_end);
		    }
		    else {
		      PathEndPathDelay path_end(path_delay, path, tgt_clk_path,
						check_arc, edge, this);
		      visitor->visit(&path_end);
		      is_constrained = true;
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
  if (!check_clked
      && !sdc_->exceptionToInvalid(pin))
    visitCheckEndUnclked(pin, vertex, path, end_rf, path_ap, filtered,
			 visitor, is_constrained);
}

void
VisitPathEnds::visitCheckEndUnclked(const Pin *pin,
				    Vertex *vertex,
				    Path *path,
				    const RiseFall *end_rf,
				    const PathAnalysisPt *path_ap,
				    bool filtered,
				    PathEndVisitor *visitor,
				    bool &is_constrained)
{
  const MinMax *min_max = path_ap->pathMinMax();
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    const TimingRole *check_role = edge->role();
    if (checkEdgeEnabled(edge)
	&& check_role->pathMinMax() == min_max) {
      TimingArcSet *arc_set = edge->timingArcSet();
      TimingArcSetArcIterator arc_iter(arc_set);
      while (arc_iter.hasNext()) {
	TimingArc *check_arc = arc_iter.next();
	RiseFall *clk_rf = check_arc->fromTrans()->asRiseFall();
	if (check_arc->toTrans()->asRiseFall() == end_rf
	    && clk_rf
	    && (!filtered
		|| search_->matchesFilter(path, nullptr))) {
	  ExceptionPath *exception = exceptionTo(path, pin, end_rf,
						 nullptr, min_max);
	  // False paths and path delays override multicycle paths.
	  if (exception
	      && exception->isPathDelay()) {
	    PathDelay *path_delay = dynamic_cast<PathDelay*>(exception);
	    PathEndPathDelay path_end(path_delay, path, nullptr,
				      check_arc, edge, this);
	    visitor->visit(&path_end);
	    is_constrained = true;
	  }
	}
      }
    }
  }
}

bool
VisitPathEnds::checkEdgeEnabled(Edge *edge) const
{
  const TimingRole *check_role = edge->role();
  return check_role->isTimingCheck()
    && search_->evalPred()->searchFrom(edge->from(graph_))
    && !edge->isDisabledConstraint()
    && !edge->isDisabledCond()
    && !sdc_->isDisabledCondDefault(edge)
    && !((check_role == TimingRole::recovery()
	  || check_role == TimingRole::removal())
	 && !sdc_->recoveryRemovalChecksEnabled());
}

void
VisitPathEnds::visitOutputDelayEnd(const Pin *pin,
				   Path *path,
				   const RiseFall *end_rf,
				   const PathAnalysisPt *path_ap,
				   bool filtered,
				   PathEndVisitor *visitor,
				   bool &is_constrained)
{
  const MinMax *min_max = path_ap->pathMinMax();
  OutputDelaySet *output_delays = sdc_->outputDelaysLeafPin(pin);
  if (output_delays) {
    for (OutputDelay *output_delay : *output_delays) {
      float margin;
      bool exists;
      output_delay->delays()->value(end_rf, min_max, margin, exists);
      if (exists) {
	const Pin *ref_pin = output_delay->refPin();
	ClockEdge *tgt_clk_edge = output_delay->clkEdge();
	if (!filtered
	    || search_->matchesFilter(path, tgt_clk_edge)) {
	  if (ref_pin) {
	    Clock *tgt_clk = output_delay->clock();
	    Vertex *ref_vertex = graph_->pinLoadVertex(ref_pin);
	    RiseFall *ref_rf = output_delay->refTransition();
	    VertexPathIterator ref_path_iter(ref_vertex,ref_rf,path_ap,this);
	    while (ref_path_iter.hasNext()) {
	      PathVertex *ref_path = ref_path_iter.next();
	      if (ref_path->isClock(this)
		  && (tgt_clk == nullptr
		      || ref_path->clock(this) == tgt_clk))
		visitOutputDelayEnd1(output_delay, pin, path, end_rf,
				     ref_path->clkEdge(this), ref_path, min_max,
				     visitor, is_constrained);
	    }
	  }
	  else
	    visitOutputDelayEnd1(output_delay, pin, path, end_rf,
				 tgt_clk_edge, nullptr, min_max,
				 visitor, is_constrained);
	}
      }
    }
  }
}

void
VisitPathEnds::visitOutputDelayEnd1(OutputDelay *output_delay,
				    const Pin *pin,
				    Path *path,
				    const RiseFall *end_rf,
				    const ClockEdge *tgt_clk_edge,
				    PathVertex *ref_path,
				    const MinMax *min_max,
				    PathEndVisitor *visitor,
				    bool &is_constrained)
{
  // Target clk is not required for path delay,
  // but the exception may be -to clk.
  ExceptionPath *exception = exceptionTo(path, pin, end_rf, tgt_clk_edge,
					 min_max);
  if (exception
      && exception->isPathDelay()) {
    PathDelay *path_delay = dynamic_cast<PathDelay*>(exception);
    PathEndPathDelay path_end(path_delay, path, output_delay, this);
    visitor->visit(&path_end);
    is_constrained = true;
  }
  else if (tgt_clk_edge
	   && sdc_->sameClockGroup(path->clock(this), tgt_clk_edge->clock())
	   // False paths and path delays override.
	   && (exception == nullptr
	       || exception->isFilter()
	       || exception->isGroupPath()
	       || exception->isMultiCycle())) {
    MultiCyclePath *mcp = dynamic_cast<MultiCyclePath*>(exception);
    PathEndOutputDelay path_end(output_delay, path, ref_path, mcp, this);
    visitor->visit(&path_end);
    is_constrained = true;
  }
}

////////////////////////////////////////////////////////////////

// Look for clock gating functions where path is the clock enable.
void
VisitPathEnds::visitGatedClkEnd(const Pin *pin,
				Vertex *vertex,
				Path *path,
				const RiseFall *end_rf,
				const PathAnalysisPt *path_ap,
				bool filtered,
				PathEndVisitor *visitor,
				bool &is_constrained)
{
  ClockEdge *src_clk_edge = path->clkEdge(this);
  if (src_clk_edge) {
    GatedClk *gated_clk = search_->gatedClk();
    Clock *src_clk = src_clk_edge->clock();
    bool is_gated_clk_enable;
    const Pin *clk_pin;
    LogicValue logic_active_value;
    gated_clk->isGatedClkEnable(vertex,
				is_gated_clk_enable, clk_pin, logic_active_value);
    if (is_gated_clk_enable) {
      const PathAnalysisPt *clk_path_ap = path_ap->tgtClkAnalysisPt();
      const MinMax *min_max = path_ap->pathMinMax();
      Vertex *clk_vertex = graph_->pinLoadVertex(clk_pin);
      LogicValue active_value =
	sdc_->clockGatingActiveValue(clk_pin, pin);
      RiseFall *clk_rf =
	// Clock active value specified by set_clock_gating_check
	// overrides the library cell function active value.
	gated_clk->gatedClkActiveTrans((active_value == LogicValue::unknown) ?
				       logic_active_value : active_value,
				       min_max);
      VertexPathIterator clk_path_iter(clk_vertex, clk_rf, clk_path_ap, this);
      while (clk_path_iter.hasNext()) {
	PathVertex *clk_path = clk_path_iter.next();
	const ClockEdge *clk_edge = clk_path->clkEdge(this);
	const Clock *clk = clk_edge ? clk_edge->clock() : nullptr;
	if (clk_path->isClock(this)
	    // Ignore unclocked paths (from path delay constraints).
	    && clk_edge
	    && clk_edge != sdc_->defaultArrivalClockEdge()
	    // Ignore generated clock source paths.
	    && !path->clkInfo(this)->isGenClkSrcPath()
	    && !search_->pathPropagatedToClkSrc(clk_pin, clk_path)
	    && !sdc_->clkStopPropagation(pin, clk)
	    && clk_vertex->hasDownstreamClkPin()) {
	  TimingRole *check_role = (min_max == MinMax::max())
	    ? TimingRole::gatedClockSetup()
	    : TimingRole::gatedClockHold();
	  float margin = clockGatingMargin(clk, clk_pin,
					   pin, end_rf, min_max);
	  ExceptionPath *exception = exceptionTo(path, pin, end_rf,
						 clk_edge, min_max);
	  if (sdc_->sameClockGroup(src_clk, clk)
	      // False paths and path delays override.
	      && (exception == nullptr
		  || exception->isFilter()
		  || exception->isGroupPath()
		  || exception->isMultiCycle())
	      && (!filtered
		  || search_->matchesFilter(path, clk_edge))) {
	    MultiCyclePath *mcp =
	      dynamic_cast<MultiCyclePath *>(exception);
	    PathEndGatedClock path_end(path, clk_path, check_role,
				       mcp, margin, this);
	    visitor->visit(&path_end);
	    is_constrained = true;
	  }
	}
      }
    }
  }
}

// Gated clock setup/hold margin respecting precedence rules.
// Look for margin from highest precedence level to lowest.
float
VisitPathEnds::clockGatingMargin(const Clock *clk,
				 const Pin *clk_pin,
				 const Pin *enable_pin,
				 const RiseFall *enable_rf,
				 const SetupHold *setup_hold)
{
  bool exists;
  float margin;
  sdc_->clockGatingMarginEnablePin(enable_pin, enable_rf,
					   setup_hold, exists, margin);
  if (exists)
    return margin;
  Instance *inst = network_->instance(enable_pin);
  sdc_->clockGatingMarginInstance(inst, enable_rf, setup_hold,
					  exists, margin);
  if (exists)
    return margin;
  sdc_->clockGatingMarginClkPin(clk_pin, enable_rf, setup_hold,
					exists, margin);
  if (exists)
    return margin;
  sdc_->clockGatingMarginClk(clk, enable_rf, setup_hold,
				     exists, margin);
  if (exists)
    return margin;
  sdc_->clockGatingMargin(enable_rf, setup_hold,
				  exists, margin);
  if (exists)
    return margin;
  else
    return 0.0;
}

////////////////////////////////////////////////////////////////

void
VisitPathEnds::visitDataCheckEnd(const Pin *pin,
				 Path *path,
				 const RiseFall *end_rf,
				 const PathAnalysisPt *path_ap,
				 bool filtered,
				 PathEndVisitor *visitor,
				 bool &is_constrained)
{
  ClockEdge *src_clk_edge = path->clkEdge(this);
  if (src_clk_edge) {
    DataCheckSet *checks = sdc_->dataChecksTo(pin);
    if (checks) {
      const Clock *src_clk = src_clk_edge->clock();
      const MinMax *min_max = path_ap->pathMinMax();
      const PathAnalysisPt *clk_ap = path_ap->tgtClkAnalysisPt();
      DataCheckSet::Iterator check_iter(checks);
      while (check_iter.hasNext()) {
	DataCheck *check = check_iter.next();
 	const Pin *from_pin = check->from();
 	Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
	for (auto from_rf : RiseFall::range()) {
 	  float margin;
 	  bool margin_exists;
 	  check->margin(from_rf, end_rf, min_max, margin, margin_exists);
 	  if (margin_exists)
	    visitDataCheckEnd1(check, pin, path, src_clk, end_rf,
			       min_max, clk_ap, from_pin, from_vertex,
			       from_rf, filtered, visitor, is_constrained);
	}
      }
    }
  }
}

bool
VisitPathEnds::visitDataCheckEnd1(DataCheck *check,
				  const Pin *pin,
				  Path *path,
				  const Clock *src_clk,
				  const RiseFall *end_rf,
				  const MinMax *min_max,
				  const PathAnalysisPt *clk_ap,
				  const Pin *from_pin,
				  Vertex *from_vertex,
				  RiseFall *from_rf,
				  bool filtered,
				  PathEndVisitor *visitor,
				  bool &is_constrained)
{
  bool found_from_path = false;
  VertexPathIterator tgt_clk_path_iter(from_vertex,from_rf,clk_ap,this);
  while (tgt_clk_path_iter.hasNext()) {
    PathVertex *tgt_clk_path = tgt_clk_path_iter.next();
    const ClockEdge *tgt_clk_edge = tgt_clk_path->clkEdge(this);
    const Clock *tgt_clk = tgt_clk_edge ? tgt_clk_edge->clock() : nullptr;
    ExceptionPath *exception = exceptionTo(path, pin, end_rf,
					   tgt_clk_edge, min_max);
    // Ignore generated clock source paths.
    if (!tgt_clk_path->clkInfo(this)->isGenClkSrcPath()
	&& !search_->pathPropagatedToClkSrc(from_pin, tgt_clk_path)) {
      found_from_path = true;
      if (sdc_->sameClockGroup(src_clk, tgt_clk)
	  && !sdc_->clkStopPropagation(from_pin, tgt_clk)
	  // False paths and path delays override.
	  && (exception == 0
	      || exception->isFilter()
	      || exception->isGroupPath()
	      || exception->isMultiCycle())
	  && (!filtered
	      || search_->matchesFilter(path, tgt_clk_edge))) {
	// No mcp for data checks.
	PathEndDataCheck path_end(check, path, tgt_clk_path, nullptr, this);
	visitor->visit(&path_end);
	is_constrained = true;
      }
    }
  }
  return found_from_path;
}

////////////////////////////////////////////////////////////////

void
VisitPathEnds::visitUnconstrainedPathEnds(const Pin *pin,
					  Vertex *vertex,
					  const Corner *corner,
					  const MinMaxAll *min_max,
					  bool filtered,
					  PathEndVisitor *visitor)
{
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
    const MinMax *path_min_max = path_ap->pathMinMax();
    if ((corner == nullptr
	 || path_ap->corner() == corner)
	&& min_max->matches(path_min_max)
	// Ignore generated clock source paths.
	&& !path->clkInfo(this)->isGenClkSrcPath()
 	&& !search_->pathPropagatedToClkSrc(pin, path)
	&& (!filtered
	    || search_->matchesFilter(path, nullptr))
	&& !falsePathTo(path, pin, path->transition(this),
			path->minMax(this))) {
      PathEndUnconstrained path_end(path);
      visitor->visit(&path_end);
    }
  }
}

////////////////////////////////////////////////////////////////

bool
VisitPathEnds::falsePathTo(Path *path,
			   const Pin *pin,
			   const RiseFall *rf,
			   const MinMax *min_max)
{
  ExceptionPath *exception = search_->exceptionTo(ExceptionPathType::false_path, path,
						  pin, rf, nullptr, min_max,
						  false, false);
  return exception != nullptr;
}

PathDelay *
VisitPathEnds::pathDelayTo(Path *path,
			   const Pin *pin,
			   const RiseFall *rf,
			   const MinMax *min_max)
{
  ExceptionPath *exception = search_->exceptionTo(ExceptionPathType::path_delay,
						  path, pin, rf, nullptr,
						  min_max, false,
						  // Register clk pins only
						  // match with -to pin.
						  network_->isRegClkPin(pin));
  return dynamic_cast<PathDelay*>(exception);
}

ExceptionPath *
VisitPathEnds::exceptionTo(const Path *path,
			   const Pin *pin,
			   const RiseFall *rf,
			   const ClockEdge *clk_edge,
			   const MinMax *min_max) const
{
  return search_->exceptionTo(ExceptionPathType::any, path, pin, rf, clk_edge,
			      min_max, false, false);
}

} // namespace
