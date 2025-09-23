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

#include "Crpr.hh"

#include <cmath> // abs
#include <stdio.h>

#include "Debug.hh"
#include "Vector.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Path.hh"
#include "PathAnalysisPt.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "VisitPathEnds.hh"
#include "PathEnd.hh"
#include "Search.hh"
#include "Genclks.hh"
#include "Variables.hh"

namespace sta {

using std::min;
using std::abs;

CheckCrpr::CheckCrpr(StaState *sta) :
  StaState(sta)
{
}

// Find the maximum possible crpr (clock min/max delta delay) for a
// path from it's ClkInfo.
Arrival
CheckCrpr::maxCrpr(const ClkInfo *clk_info)
{
  const Path *crpr_clk_path = clk_info->crprClkPath(this);
  if (crpr_clk_path) {
    Arrival other_arrival = otherMinMaxArrival(crpr_clk_path);
    float crpr_diff = abs(delayAsFloat(crpr_clk_path->arrival(),
                                       EarlyLate::late(),
                                       this)
                          - delayAsFloat(other_arrival, EarlyLate::early(),
                                         this));
    return crpr_diff;
  }
  return 0.0F;
}

Arrival
CheckCrpr::otherMinMaxArrival(const Path *path)
{
  PathAnalysisPt *other_ap = path->pathAnalysisPt(this)->tgtClkAnalysisPt();
  Tag *tag = path->tag(this);
  VertexPathIterator other_iter(path->vertex(this),
				path->transition(this),
				other_ap, this);
  while (other_iter.hasNext()) {
    Path *other = other_iter.next();
    if (Tag::matchCrpr(other->tag(this), tag))
      return other->arrival();
  }
  // No corresponding path found.
  // Match the arrival so the difference is zero.
  return path->arrival();
}

Crpr
CheckCrpr::checkCrpr(const Path *src_path,
		     const Path *tgt_clk_path)
{
  Crpr crpr;
  Pin *crpr_pin;
  checkCrpr(src_path, tgt_clk_path, crpr, crpr_pin);
  return crpr;
}

void
CheckCrpr::checkCrpr(const Path *src_path,
		     const Path *tgt_clk_path,
		     // Return values.
		     Crpr &crpr,
		     Pin *&crpr_pin)
{
  crpr = 0.0;
  crpr_pin = nullptr;
  if (crprActive()
      && src_path && tgt_clk_path) {
    bool same_pin = (variables_->crprMode() == CrprMode::same_pin);
    checkCrpr1(src_path, tgt_clk_path, same_pin, crpr, crpr_pin);
  }
}

void
CheckCrpr::checkCrpr1(const Path *src_path,
		      const Path *tgt_clk_path,
		      bool same_pin,
		      // Return values.
		      Crpr &crpr,
		      Pin *&crpr_pin)
{
  crpr = 0.0;
  crpr_pin = nullptr;
  const Tag *src_tag = src_path->tag(this);
  const ClkInfo *src_clk_info = src_tag->clkInfo();
  const ClkInfo *tgt_clk_info = tgt_clk_path->tag(this)->clkInfo();
  const Clock *src_clk = src_clk_info->clock();
  const Clock *tgt_clk = tgt_clk_info->clock();
  const Path *src_clk_path = nullptr;
  if (src_tag->isClock())
    src_clk_path = src_path;
  else
    src_clk_path = src_clk_info->crprClkPath(this);
  const MinMax *src_clk_min_max =
    src_clk_path ? src_clk_path->minMax(this) : src_path->minMax(this);
  if (src_clk && tgt_clk
      && crprPossible(src_clk, tgt_clk)
      && src_clk_info->isPropagated()
      && tgt_clk_info->isPropagated()
      // Note that crpr clk min/max is NOT the same as the path min max.
      // For path from latches that are borrowing the enable path
      // is from the opposite min/max of the data.
      && src_clk_min_max != tgt_clk_path->minMax(this)
      && (src_clk_path
	  || src_clk->isGenerated())) {
    // Src path from input port clk path can only be from generated clk path.
    if (src_clk_path == nullptr) {
      src_clk_path = portClkPath(src_clk_info->clkEdge(),
                                 src_clk_info->clkSrc(),
                                 src_path->pathAnalysisPt(this));
    }
    findCrpr(src_clk_path, tgt_clk_path, same_pin, crpr, crpr_pin);
  }
}

// Find the clk path for an input/output port.
Path *
CheckCrpr::portClkPath(const ClockEdge *clk_edge,
		       const Pin *clk_src_pin,
		       const PathAnalysisPt *path_ap)
{
  Vertex *clk_vertex = graph_->pinDrvrVertex(clk_src_pin);
  VertexPathIterator path_iter(clk_vertex, clk_edge->transition(),
			       path_ap, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    if (path->clkEdge(this) == clk_edge
	&& path->isClock(this)) {
      return path;
    }
  }
  return nullptr;
}

void
CheckCrpr::findCrpr(const Path *src_clk_path,
		    const Path *tgt_clk_path,
		    bool same_pin,
		    // Return values.
		    Crpr &crpr,
		    Pin *&crpr_pin)
{
  crpr = 0.0;
  crpr_pin = nullptr;
  const Path *src_clk_path1 = src_clk_path;
  const Path *tgt_clk_path1 = tgt_clk_path;
  if (src_clk_path1->clkInfo(this)->clkSrc()
      != tgt_clk_path1->clkInfo(this)->clkSrc()) {
    // Push src/tgt genclk src paths into a vector,
    // The last genclk src path is at index 0.
    ConstPathSeq src_gclk_paths = genClkSrcPaths(src_clk_path1);
    ConstPathSeq tgt_gclk_paths = genClkSrcPaths(tgt_clk_path1);
    // Search from the first gen clk toward the end
    // of the path to find a common root pin.
    int i = src_gclk_paths.size() - 1;
    int j = tgt_gclk_paths.size() - 1;
    for (; i >= 0 && j >= 0; i--, j--) {
      const Path *src_path = src_gclk_paths[i];
      const Path *tgt_path = tgt_gclk_paths[j];
      if (src_path->clkInfo(this)->clkSrc()
	  == tgt_path->clkInfo(this)->clkSrc()) {
	src_clk_path1 = src_gclk_paths[i];
	tgt_clk_path1 = tgt_gclk_paths[j];
      }
      else
	break;
    }
  }
  const Path *src_clk_path2 = src_clk_path1;
  const Path *tgt_clk_path2 = tgt_clk_path1;
  // src_clk_path2 and tgt_clk_path2 are now in the same (gen)clk src path.
  // Use the vertex levels to back up the deeper path to see if they
  // overlap.
  int src_level = src_clk_path2->vertex(this)->level();
  int tgt_level = tgt_clk_path2->vertex(this)->level();
  while (src_clk_path2->pin(this) != tgt_clk_path2->pin(this)) {
    int level_diff = src_level - tgt_level;
    if (level_diff >= 0) {
      src_clk_path2 = src_clk_path2->prevPath();
      if (src_clk_path2 == nullptr)
        break;
      src_level = src_clk_path2->vertex(this)->level();
    }
    if (level_diff <= 0) {
      tgt_clk_path2 = tgt_clk_path2->prevPath();
      if (tgt_clk_path2 == nullptr)
        break;
      tgt_level = tgt_clk_path2->vertex(this)->level();
    }
  }
  if (src_clk_path2 && tgt_clk_path2
      && (src_clk_path2->transition(this) == tgt_clk_path2->transition(this)
	  || same_pin)) {
    debugPrint(debug_, "crpr", 2, "crpr pin %s",
               network_->pathName(src_clk_path2->pin(this)));
    crpr = findCrpr1(src_clk_path2, tgt_clk_path2);
    crpr_pin = src_clk_path2->pin(this);
  }
}

ConstPathSeq
CheckCrpr::genClkSrcPaths(const Path *path)
{
  ConstPathSeq gclk_paths;
  const ClkInfo *clk_info = path->clkInfo(this);
  const ClockEdge *clk_edge = clk_info->clkEdge();
  const Pin *clk_src = clk_info->clkSrc();
  PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
  gclk_paths.push_back(path);
  Genclks *genclks = search_->genclks();
  while (clk_edge->clock()->isGenerated()) {
    const Path *genclk_path = genclks->srcPath(clk_edge, clk_src, path_ap);
    if (genclk_path == nullptr)
      break;
    clk_info = genclk_path->clkInfo(this);
    clk_src = clk_info->clkSrc();
    clk_edge = clk_info->clkEdge();
    gclk_paths.push_back(genclk_path);
  }
  return gclk_paths;
}

Crpr
CheckCrpr::findCrpr1(const Path *src_clk_path,
		     const Path *tgt_clk_path)
{
  if (variables_->pocvEnabled()) {
    // Remove variation on the common path.
    // Note that the crpr sigma is negative to offset the
    // sigma of the common clock path.
    const EarlyLate *src_el = src_clk_path->minMax(this);
    const EarlyLate *tgt_el = tgt_clk_path->minMax(this);
    Arrival src_arrival = src_clk_path->arrival();
    Arrival tgt_arrival = tgt_clk_path->arrival();
    float src_clk_time = src_clk_path->clkEdge(this)->time();
    float tgt_clk_time = tgt_clk_path->clkEdge(this)->time();
    float crpr_mean = abs(delayAsFloat(src_arrival) - src_clk_time
			  - (delayAsFloat(tgt_arrival) - tgt_clk_time));
    // Remove the sigma from both source and target path arrivals.
    float crpr_sigma2 = delaySigma2(src_arrival, src_el)
      + delaySigma2(tgt_arrival, tgt_el);
    return makeDelay2(crpr_mean, -crpr_sigma2, -crpr_sigma2);
  }
  else {
    // The source and target edges are different so the crpr
    // is the min of the source and target max-min delay.
    float src_delta = crprArrivalDiff(src_clk_path);
    float tgt_delta = crprArrivalDiff(tgt_clk_path);
    debugPrint(debug_, "crpr", 2, " src delta %s",
               delayAsString(src_delta, this));
    debugPrint(debug_, "crpr", 2, " tgt delta %s",
               delayAsString(tgt_delta, this));
    float common_delay = min(src_delta, tgt_delta);
    debugPrint(debug_, "crpr", 2, " %s delta %s",
               network_->pathName(src_clk_path->pin(this)),
               delayAsString(common_delay, this));
    return common_delay;
  }
}

float
CheckCrpr::crprArrivalDiff(const Path *path)
{
  Arrival other_arrival = otherMinMaxArrival(path);
  float crpr_diff = abs(delayAsFloat(path->arrival())
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
  if (crprActive()) {
    const PathAnalysisPt *path_ap = src_path->pathAnalysisPt(this);
    const PathAnalysisPt *tgt_path_ap = path_ap->tgtClkAnalysisPt();
    bool same_pin = (variables_->crprMode() == CrprMode::same_pin);
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
  const ClkInfo *src_clk_info = src_path->tag(this)->clkInfo();
  const Clock *tgt_clk = tgt_clk_edge->clock();
  const Clock *src_clk = src_path->clock(this);
  if (src_clk && tgt_clk
      && src_clk_info->isPropagated()
      && tgt_clk->isGenerated()
      && tgt_clk->isPropagated()
      && crprPossible(src_clk, tgt_clk)) {
    Path *tgt_genclk_path = portClkPath(tgt_clk_edge,
                                        tgt_clk_edge->clock()->defaultPin(),
                                        tgt_path_ap);
    const Path *src_clk_path = src_path->clkInfo(this)->crprClkPath(this);
    if (src_clk_path)
      findCrpr(src_clk_path, tgt_genclk_path, same_pin, crpr, crpr_pin);
  }
}

bool
CheckCrpr::crprPossible(const Clock *clk1,
			const Clock *clk2)
{
  return clk1 && clk2
    && !clk1->isVirtual()
    && !clk2->isVirtual()
    // Generated clocks can have crpr in the source path.
    && (clk1 == clk2
	|| clk1->isGenerated()
	|| clk2->isGenerated()
	// Different non-generated clocks with the same source pins (using -add).
	|| PinSet::intersects(&clk1->pins(), &clk2->pins(), network_));
}

} // namespace
