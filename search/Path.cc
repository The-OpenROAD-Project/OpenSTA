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
#include "TimingArc.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "Tag.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "PathAnalysisPt.hh"
#include "PathRef.hh"
#include "Path.hh"

namespace sta {

const char *
Path::name(const StaState *sta) const
{
  const Network *network = sta->network();
  Vertex *vertex1 = vertex(sta);
  if (vertex1) {
    const char *vertex_name = vertex1->name(network);
    const char *tr_str = transition(sta)->asString();
    const PathAnalysisPt *path_ap = pathAnalysisPt(sta);
    int ap_index = path_ap->index();
    const char *min_max = path_ap->pathMinMax()->asString();
    TagIndex tag_index = tagIndex(sta);
    return stringPrintTmp("%s %s %s/%d %d",
			  vertex_name, tr_str, min_max,
			  ap_index, tag_index);
  }
  else
    return "NULL";
}

Pin *
Path::pin(const StaState *sta) const
{
  return vertex(sta)->pin();
}

TagIndex
Path::tagIndex(const StaState *sta) const
{
  return tag(sta)->index();
}

ClkInfo *
Path::clkInfo(const StaState *sta) const
{
  return tag(sta)->clkInfo();
}

ClockEdge *
Path::clkEdge(const StaState *sta) const
{
  return tag(sta)->clkEdge();
}

Clock *
Path::clock(const StaState *sta) const
{
  return tag(sta)->clock();
}

bool
Path::isClock(const StaState *sta) const
{
  return tag(sta)->isClock();
}

const MinMax *
Path::minMax(const StaState *sta) const
{
  return pathAnalysisPt(sta)->pathMinMax();
}

PathAPIndex
Path::pathAnalysisPtIndex(const StaState *sta) const
{
  return pathAnalysisPt(sta)->index();
}

DcalcAnalysisPt *
Path::dcalcAnalysisPt(const StaState *sta) const
{
  return pathAnalysisPt(sta)->dcalcAnalysisPt();
}

Slew
Path::slew(const StaState *sta) const
{
  return sta->graph()->slew(vertex(sta), transition(sta),
			    dcalcAnalysisPt(sta)->index());
}

int
Path::rfIndex(const StaState *sta) const
{
  return transition(sta)->index();
}

void
Path::initArrival(const StaState *sta)
{
  setArrival(delayInitValue(minMax(sta)), sta);
}

bool
Path::arrivalIsInitValue(const StaState *sta) const
{
  return delayIsInitValue(arrival(sta), minMax(sta));
}

void
Path::initRequired(const StaState *sta)
{
  setRequired(delayInitValue(minMax(sta)->opposite()), sta);
}

bool
Path::requiredIsInitValue(const StaState *sta) const
{
  return delayIsInitValue(required(sta), minMax(sta)->opposite());
}

Slack
Path::slack(const StaState *sta) const
{
  if (minMax(sta) == MinMax::max())
    return required(sta) - arrival(sta);
  else
    return arrival(sta) - required(sta);
}

void
Path::prevPath(const StaState *sta,
	       // Return values.
	       PathRef &prev_path) const
{
  TimingArc *prev_arc;
  prevPath(sta, prev_path, prev_arc);
}

TimingArc *
Path::prevArc(const StaState *sta) const
{
  PathRef prev_path;
  TimingArc *prev_arc;
  prevPath(sta, prev_path, prev_arc);
  return prev_arc;
}

Edge *
Path::prevEdge(const TimingArc *prev_arc,
	       const StaState *sta) const
{
  if (prev_arc) {
    TimingArcSet *arc_set = prev_arc->set();
    VertexInEdgeIterator edge_iter(vertex(sta), sta->graph());
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->timingArcSet() == arc_set)
	return edge;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

int
Path::cmpPinTrClk(const Path *path1,
		  const Path *path2,
		  const StaState *sta)
{
  if (path1 && path2) {
    const Pin *pin1 = path1->pin(sta);
    const Pin *pin2 = path2->pin(sta);
    const Network *network = sta->network();
    if (pin1 == pin2) {
      int tr_index1 = path1->rfIndex(sta);
      int tr_index2 = path2->rfIndex(sta);
      if (tr_index1 == tr_index2)
	return cmpClk(path1, path2, sta);
      else if (tr_index1 < tr_index2)
	return -1;
      else
	return 1;
    }
    else if (network->pathNameLess(pin1, pin2))
      return -1;
    else
      return 1;
  }
  else if (path1 == nullptr && path2 == nullptr)
    return 0;
  else if (path1 == nullptr)
    return -1;
  else
    return 1;
}

int
Path::cmpClk(const Path *path1,
	     const Path *path2,
	     const StaState *sta)
{
  ClockEdge *clk_edge1 = path1->clkEdge(sta);
  ClockEdge *clk_edge2 = path2->clkEdge(sta);
  if (clk_edge1 && clk_edge2) {
    int index1 = clk_edge1->index();
    int index2 = clk_edge2->index();
    if (index1 == index2)
      return 0;
    else if (index1 < index2)
      return -1;
    else
      return 1;
  }
  else if (clk_edge1 == nullptr
	   && clk_edge2 == nullptr)
    return 0;
  else if (clk_edge2)
    return -1;
  else
    return 1;
}

bool
Path::equal(const Path *path1,
	    const Path *path2,
	    const StaState *sta)
{
  bool path1_null = (path1 == nullptr || path1->isNull());
  bool path2_null = (path2 == nullptr || path2->isNull());
  return (path1_null && path2_null)
    || (!path1_null
	&& !path2_null
	&& path1->vertexId(sta) == path2->vertexId(sta)
	// Tag equal implies transition and path ap equal.
	&& path1->tagIndex(sta) == path2->tagIndex(sta));
}

////////////////////////////////////////////////////////////////

PathLess::PathLess(const StaState *sta) :
  sta_(sta)
{
}

bool
PathLess::operator()(const Path *path1,
		     const Path *path2) const
{
  return Path::less(path1, path2, sta_);
}

bool
Path::less(const Path *path1,
	   const Path *path2,
	   const StaState *sta)
{
  return cmp(path1, path2, sta) < 0;
}

int
Path::cmp(const Path *path1,
	  const Path *path2,
	  const StaState *sta)
{
  if (path1 && path2) {
    VertexId vertex_id1 = path1->vertexId(sta);
    VertexId vertex_id2 = path2->vertexId(sta);
    if (vertex_id1 == vertex_id2) {
      TagIndex tag_index1 = path1->tagIndex(sta);
      TagIndex tag_index2 = path2->tagIndex(sta);
      if (tag_index1 == tag_index2)
	return 0;
      else if (tag_index1 < tag_index2)
	return -1;
      else
	return 1;
    }
    else if (vertex_id1 < vertex_id2)
      return -1;
    else
      return 1;
  }
  else if (path1 == nullptr
	   && path2 == nullptr)
    return 0;
  else if (path1 == nullptr)
    return -1;
  else
    return 1;
}

int
Path::cmpNoCrpr(const Path *path1,
		const Path *path2,
		const StaState *sta)
{
  VertexId vertex_id1 = path1->vertexId(sta);
  VertexId vertex_id2 = path2->vertexId(sta);
  if (vertex_id1 == vertex_id2)
    return tagMatchCmp(path1->tag(sta), path2->tag(sta), false, sta);
  else if (vertex_id1 < vertex_id2)
    return -1;
  else
    return 1;
}

int
Path::cmpAll(const Path *path1,
	     const Path *path2,
	     const StaState *sta)
{
  PathRef p1(path1);
  PathRef p2(path2);
  while (!p1.isNull()
	 && !p2.isNull()) {
    int cmp = Path::cmp(&p1, &p2, sta);
    if (cmp != 0)
      return cmp;

    p1.prevPath(sta, p1);
    p2.prevPath(sta, p2);
    if (equal(&p1, path1, sta))
      // Equivalent latch loops.
      return 0;
  }
  if (p1.isNull() && p2.isNull())
    return 0;
  else if (p1.isNull() && !p2.isNull())
    return -1;
  else
    return 1;
}

bool
Path::lessAll(const Path *path1,
	      const Path *path2,
	      const StaState *sta)
{
  return cmpAll(path1, path2, sta) < 0;
}

} // namespace
