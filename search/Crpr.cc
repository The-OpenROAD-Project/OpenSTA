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

#include <cmath> // abs
#include <stdio.h>
#include "Machine.hh"
#include "Debug.hh"
#include "Vector.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "PathVertex.hh"
#include "PathVertexRep.hh"
#include "Path.hh"
#include "PathAnalysisPt.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "VisitPathEnds.hh"
#include "PathEnd.hh"
#include "Search.hh"
#include "Genclks.hh"
#include "Crpr.hh"

namespace sta {

using std::min;
using std::abs;

CheckCrpr::CheckCrpr(StaState *sta) :
  StaState(sta)
{
}

PathVertex *
CheckCrpr::clkPathPrev(const PathVertex *path,
		       PathVertex &tmp)

{
  Vertex *vertex = path->vertex(this);
  int arrival_index;
  bool exists;
  path->arrivalIndex(arrival_index, exists);
  return clkPathPrev(vertex, arrival_index, tmp);
}

PathVertex *
CheckCrpr::clkPathPrev(Vertex *vertex,
		       int arrival_index,
		       PathVertex &tmp)
{
  PathVertexRep *prevs = graph_->prevPaths(vertex);
  if (prevs) {
    PathVertexRep *prev = &prevs[arrival_index];
    if (prev->isNull())
      return nullptr;
    else {
      tmp.init(graph_->vertex(prev->vertexId()),
	       search_->tag(prev->tagIndex()), this);
      return &tmp;
    }
  }
  else
    internalError("missing prev paths");
}

////////////////////////////////////////////////////////////////

// Find the maximum possible crpr (clock min/max delta delay) for a
// path from it's ClkInfo.
Arrival
CheckCrpr::maxCrpr(ClkInfo *clk_info)
{
  const PathVertexRep &crpr_clk_path = clk_info->crprClkPath();
  if (!crpr_clk_path.isNull()) {
    PathVertex crpr_clk_vpath(crpr_clk_path, this);
    if (!crpr_clk_vpath.isNull()) {
      Arrival other_arrival = otherMinMaxArrival(&crpr_clk_vpath);
      float crpr_diff = abs(delayAsFloat(crpr_clk_vpath.arrival(this),
					 EarlyLate::late(),
					 this)
			    - delayAsFloat(other_arrival, EarlyLate::early(),
					   this));
      return crpr_diff;
    }
  }
  return 0.0F;
}

Arrival
CheckCrpr::otherMinMaxArrival(const PathVertex *path)
{
  PathAnalysisPt *other_ap = path->pathAnalysisPt(this)->tgtClkAnalysisPt();
  Tag *tag = path->tag(this);
  VertexPathIterator other_iter(path->vertex(this),
				path->transition(this),
				other_ap, this);
  while (other_iter.hasNext()) {
    PathVertex *other = other_iter.next();
    if (tagMatchCrpr(other->tag(this), tag))
      return other->arrival(this);
  }
  // No corresponding path found.
  // Match the arrival so the difference is zero.
  return path->arrival(this);
}

Crpr
CheckCrpr::checkCrpr(const Path *src_path,
		     const PathVertex *tgt_clk_path)
{
  Crpr crpr;
  Pin *crpr_pin;
  checkCrpr(src_path, tgt_clk_path, crpr, crpr_pin);
  return crpr;
}

void
CheckCrpr::checkCrpr(const Path *src_path,
		     const PathVertex *tgt_clk_path,
		     // Return values.
		     Crpr &crpr,
		     Pin *&crpr_pin)
{
  crpr = 0.0;
  crpr_pin = nullptr;
  if (sdc_->crprActive()
      && src_path && tgt_clk_path) {
    bool same_pin = (sdc_->crprMode() == CrprMode::same_pin);
    checkCrpr1(src_path, tgt_clk_path, same_pin, crpr, crpr_pin);
  }
}

void
CheckCrpr::checkCrpr1(const Path *src_path,
		      const PathVertex *tgt_clk_path,
		      bool same_pin,
		      // Return values.
		      Crpr &crpr,
		      Pin *&crpr_pin)
{
  crpr = 0.0;
  crpr_pin = nullptr;
  ClkInfo *src_clk_info = src_path->tag(this)->clkInfo();
  ClkInfo *tgt_clk_info = tgt_clk_path->tag(this)->clkInfo();
  Clock *src_clk = src_clk_info->clock();
  Clock *tgt_clk = tgt_clk_info->clock();
  const PathVertex src_clk_path1(src_clk_info->crprClkPath(), this);
  const PathVertex *src_clk_path =
    src_clk_path1.isNull() ? nullptr : &src_clk_path1;
  const MinMax *src_clk_min_max =
    src_clk_path ? src_clk_path->minMax(this) : src_path->minMax(this);
  if (crprPossible(src_clk, tgt_clk)
      // Note that crpr clk min/max is NOT the same as the path min max.
      // For path from latches that are borrowing the enable path
      // is from the opposite min/max of the data.
      && src_clk_min_max != tgt_clk_path->minMax(this)
      && (src_clk_path != nullptr
	  || src_clk->isGenerated())) {
    // Src path from input port clk path can only be from generated clk path.
    PathVertex port_clk_path;
    if (src_clk_path == nullptr) {
      portClkPath(src_clk_info->clkEdge(),
		  src_clk_info->clkSrc(),
		  src_path->pathAnalysisPt(this),
		  port_clk_path);
      src_clk_path = &port_clk_path;
    }
    findCrpr(src_clk_path, tgt_clk_path, same_pin, crpr, crpr_pin);
  }
}

// Find the clk path for an input/output port.
void
CheckCrpr::portClkPath(const ClockEdge *clk_edge,
		       const Pin *clk_src_pin,
		       const PathAnalysisPt *path_ap,
		       // Return value.
		       PathVertex &genclk_path)
{
  Vertex *clk_vertex = graph_->pinDrvrVertex(clk_src_pin);
  VertexPathIterator path_iter(clk_vertex, clk_edge->transition(),
			       path_ap, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    if (path->clkEdge(this) == clk_edge
	&& path->isClock(this)) {
      genclk_path = path;
      break;
    }
  }
}

void
CheckCrpr::findCrpr(const PathVertex *src_clk_path,
		    const PathVertex *tgt_clk_path,
		    bool same_pin,
		    // Return values.
		    Crpr &crpr,
		    Pin *&crpr_pin)
{
  crpr = 0.0;
  crpr_pin = nullptr;
  const PathVertex *src_clk_path1 = src_clk_path;
  const PathVertex *tgt_clk_path1 = tgt_clk_path;
  PathVertexSeq src_gclk_paths, tgt_gclk_paths;
  if (src_clk_path1->clkInfo(this)->clkSrc()
      != tgt_clk_path1->clkInfo(this)->clkSrc()) {
    // Push src/tgt genclk src paths into a vector,
    // The last genclk src path is at index 0.
    genClkSrcPaths(src_clk_path1, src_gclk_paths);
    genClkSrcPaths(tgt_clk_path1, tgt_gclk_paths);
    // Search from the first gen clk toward the end
    // of the path to find a common root pin.
    int i = src_gclk_paths.size() - 1;
    int j = tgt_gclk_paths.size() - 1;
    for (; i >= 0 && j >= 0; i--, j--) {
      PathVertex &src_path = src_gclk_paths[i];
      PathVertex &tgt_path = tgt_gclk_paths[j];
      if (src_path.clkInfo(this)->clkSrc()
	  == tgt_path.clkInfo(this)->clkSrc()) {
	src_clk_path1 = &src_gclk_paths[i];
	tgt_clk_path1 = &tgt_gclk_paths[j];
      }
      else
	break;
    }
  }
  const PathVertex *src_clk_path2 = src_clk_path1;
  const PathVertex *tgt_clk_path2 = tgt_clk_path1;
  PathVertex tmp1, tmp2;
  // src_clk_path and tgt_clk_path are now in the same (gen)clk src path.
  // Use the vertex levels to back up the deeper path to see if they
  // overlap.
  while (src_clk_path2 && tgt_clk_path2
	 && src_clk_path2->pin(this) != tgt_clk_path2->pin(this)) {
    Level src_level = src_clk_path2->vertex(this)->level();
    Level tgt_level = tgt_clk_path2->vertex(this)->level();
    if (src_level >= tgt_level)
      src_clk_path2 = clkPathPrev(src_clk_path2, tmp1);
    if (tgt_level >= src_level)
      tgt_clk_path2 = clkPathPrev(tgt_clk_path2, tmp2);
  }
  if (src_clk_path2 && tgt_clk_path2
      && (src_clk_path2->transition(this) == tgt_clk_path2->transition(this)
	  || same_pin)) {
    debugPrint1(debug_, "crpr", 2, "crpr pin %s\n",
		network_->pathName(src_clk_path2->pin(this)));
    crpr = findCrpr1(src_clk_path2, tgt_clk_path2);
    crpr_pin = src_clk_path2->pin(this);
  }
}

void
CheckCrpr::genClkSrcPaths(const PathVertex *path,
			  PathVertexSeq &gclk_paths)
{
  ClkInfo *clk_info = path->clkInfo(this);
  ClockEdge *clk_edge = clk_info->clkEdge();
  const Pin *clk_src = clk_info->clkSrc();
  PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
  gclk_paths.push_back(path);
  while (clk_edge->clock()->isGenerated()) {
    PathVertex genclk_path;
    search_->genclks()->srcPath(clk_edge, clk_src, path_ap, genclk_path);
    if (genclk_path.isNull())
      break;
    clk_info = genclk_path.clkInfo(this);
    clk_src = clk_info->clkSrc();
    clk_edge = clk_info->clkEdge();
    gclk_paths.push_back(genclk_path);
  }
}

Crpr
CheckCrpr::findCrpr1(const PathVertex *src_clk_path,
		     const PathVertex *tgt_clk_path)
{
  if (pocv_enabled_) {
    // Remove variation on the common path.
    // Note that the crpr sigma is negative to offset the
    // sigma of the common clock path.
    const EarlyLate *src_el = src_clk_path->minMax(this);
    const EarlyLate *tgt_el = tgt_clk_path->minMax(this);
    Arrival src_arrival = src_clk_path->arrival(this);
    Arrival tgt_arrival = tgt_clk_path->arrival(this);
    float src_clk_time = src_clk_path->clkEdge(this)->time();
    float tgt_clk_time = tgt_clk_path->clkEdge(this)->time();
    float crpr_mean = abs(delayAsFloat(src_arrival) - src_clk_time
			  - (delayAsFloat(tgt_arrival) - tgt_clk_time));
    float crpr_sigma2 = delaySigma2(src_arrival, src_el)
      + delaySigma2(tgt_arrival, tgt_el);
    return makeDelay2(crpr_mean, -crpr_sigma2, -crpr_sigma2);
  }
  else {
    // The source and target edges are different so the crpr
    // is the min of the source and target max-min delay.
    float src_delta = crprArrivalDiff(src_clk_path);
    float tgt_delta = crprArrivalDiff(tgt_clk_path);
    debugPrint1(debug_, "crpr", 2, " src delta %s\n",
		delayAsString(src_delta, this));
    debugPrint1(debug_, "crpr", 2, " tgt delta %s\n",
		delayAsString(tgt_delta, this));
    float common_delay = min(src_delta, tgt_delta);
    debugPrint2(debug_, "crpr", 2, " %s delta %s\n",
		network_->pathName(src_clk_path->pin(this)),
		delayAsString(common_delay, this));
    return common_delay;
  }
}

float
CheckCrpr::crprArrivalDiff(const PathVertex *path)
{
  Arrival other_arrival = otherMinMaxArrival(path);
  float crpr_diff = abs(delayAsFloat(path->arrival(this))
			- delayAsFloat(other_arrival));
  return crpr_diff;
}

Crpr
CheckCrpr::outputDelayCrpr(const Path *src_clk_path,
			   const ClockEdge *tgt_clk_edge)
{
  Crpr crpr;
  Pin *crpr_pin;
  outputDelayCrpr(src_clk_path, tgt_clk_edge, crpr, crpr_pin);
  return crpr;
}

void
CheckCrpr::outputDelayCrpr(const Path *src_path,
			   const ClockEdge *tgt_clk_edge,
			   // Return values.
			   Crpr &crpr,
			   Pin *&crpr_pin)
{
  crpr = 0.0;
  crpr_pin = nullptr;
  if (sdc_->crprActive()) {
    const PathAnalysisPt *path_ap = src_path->pathAnalysisPt(this);
    const PathAnalysisPt *tgt_path_ap = path_ap->tgtClkAnalysisPt();
    bool same_pin = (sdc_->crprMode() == CrprMode::same_pin);
    outputDelayCrpr1(src_path,tgt_clk_edge,tgt_path_ap, same_pin,
		     crpr, crpr_pin);
  }
}

void
CheckCrpr::outputDelayCrpr1(const Path *src_path,
			    const ClockEdge *tgt_clk_edge,
			    const PathAnalysisPt *tgt_path_ap,
			    bool same_pin,
			    // Return values.
			    Crpr &crpr,
			    Pin *&crpr_pin)
{
  crpr = 0.0;
  crpr_pin = nullptr;
  Clock *tgt_clk = tgt_clk_edge->clock();
  Clock *src_clk = src_path->clock(this);
  if (tgt_clk->isGenerated()
      && crprPossible(src_clk, tgt_clk)) {
    PathVertex tgt_genclk_path;
    portClkPath(tgt_clk_edge, tgt_clk_edge->clock()->defaultPin(), tgt_path_ap,
		tgt_genclk_path);
    PathVertex src_clk_path(src_path->clkInfo(this)->crprClkPath(), this);
    if (!src_clk_path.isNull()) {
      findCrpr(&src_clk_path, &tgt_genclk_path, same_pin, crpr, crpr_pin);
    }
  }
}

bool
CheckCrpr::crprPossible(Clock *clk1,
			Clock *clk2)
{
  return clk1 && clk2
    && !clk1->isVirtual()
    && !clk2->isVirtual()
    // Generated clock can have crpr in the source path.
    && (clk1 == clk2
	|| clk1->isGenerated()
	|| clk2->isGenerated()
	// Different non-generated clocks with the same source pins (using -add).
	|| PinSet::intersects(clk1->pins(), clk2->pins()));
}

} // namespace
