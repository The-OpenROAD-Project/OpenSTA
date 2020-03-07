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
#include "TimingRole.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Clock.hh"
#include "Search.hh"
#include "PathRef.hh"
#include "Latches.hh"
#include "Genclks.hh"
#include "PathExpanded.hh"

namespace sta {

PathExpanded::PathExpanded(const StaState *sta) :
  sta_(sta)
{
}

PathExpanded::PathExpanded(const Path *path,
			   // Expand generated clk source paths.
			   bool expand_genclks,
			   const StaState *sta) :
  sta_(sta)
{
  expand(path, expand_genclks);
}

PathExpanded::PathExpanded(const Path *path,
			   const StaState *sta) :
  sta_(sta)
{
  expand(path, false);
}

void
PathExpanded::expand(const Path *path,
		     bool expand_genclks)
{
  const Latches *latches = sta_->latches();
  // Push the paths from the end into an array of PathRefs.
  PathRef p(path);
  PathRef last_path;
  size_t i = 0;
  bool found_start = false;
  while (!p.isNull()) {
    PathRef prev_path;
    TimingArc *prev_arc;
    p.prevPath(sta_, prev_path, prev_arc);

    if (!found_start) {
      if (prev_arc) {
	TimingRole *prev_role = prev_arc->role();
	if (prev_role == TimingRole::regClkToQ()
	    || prev_role == TimingRole::latchEnToQ()) {
	  start_index_ = i;
	  found_start = true;
	}
	else if (prev_role == TimingRole::latchDtoQ()) {
	  Edge *prev_edge = p.prevEdge(prev_arc, sta_);
	  if (latches->isLatchDtoQ(prev_edge)) {
	    start_index_ = i;
	    found_start = true;

	    paths_.push_back(p);
	    prev_arcs_.push_back(prev_arc);
	    // Push latch D path.
	    paths_.push_back(prev_path);
	    prev_arcs_.push_back(nullptr);
	    // This breaks latch loop paths.
	    break;
	  }
	}
      }
    }
    paths_.push_back(p);
    prev_arcs_.push_back(prev_arc);
    last_path.init(p);
    p.init(prev_path);
    i++;
  }
  if (!found_start)
    start_index_ = i - 1;

  if (expand_genclks)
    expandGenclk(&last_path);
}

void
PathExpanded::expandGenclk(PathRef *clk_path)
{
  if (!clk_path->isNull()) {
    Clock *src_clk = clk_path->clock(sta_);
    if (src_clk && src_clk->isGenerated()) {
      PathVertex src_path;
      sta_->search()->genclks()->srcPath(clk_path, src_path);
      if (!src_path.isNull()) {
	// The head of the genclk src path is already in paths_,
	// so skip past it.
	PathRef prev_path;
	TimingArc *prev_arc;
	src_path.prevPath(sta_, prev_path, prev_arc);

	PathRef p(prev_path);
	PathRef last_path;
	while (!p.isNull()) {
	  p.prevPath(sta_, prev_path, prev_arc);

	  paths_.push_back(p);
	  prev_arcs_.push_back(prev_arc);
	  last_path.init(p);
	  p.init(prev_path);
	}
	expandGenclk(&last_path);
      }
    }
  }
}

// Convert external index that starts at the path root
// and increases to an index for paths_ (reversed).
size_t
PathExpanded::pathsIndex(size_t index) const
{
  return paths_.size() - index - 1;
}

size_t
PathExpanded::startIndex() const
{
  return pathsIndex(start_index_);
}

PathRef *
PathExpanded::path(size_t index)
{
  if (index < paths_.size())
    return &paths_[pathsIndex(index)];
  else
    return nullptr;
}

TimingArc *
PathExpanded::prevArc(size_t index)
{
  return prev_arcs_[pathsIndex(index)];
}

PathRef *
PathExpanded::startPath()
{
  return &paths_[start_index_];
}

PathRef *
PathExpanded::endPath()
{
  return &paths_[0];
}

TimingArc *
PathExpanded::startPrevArc()
{
  return prev_arcs_[start_index_];
}

PathRef *
PathExpanded::startPrevPath()
{
  size_t start1 = start_index_ + 1;
  if (start1 < paths_.size())
    return &paths_[start1];
  else
    return nullptr;
}

void
PathExpanded::clkPath(PathRef &clk_path)
{
  const Latches *latches = sta_->latches();
  PathRef *start = startPath();
  TimingArc *prev_arc = startPrevArc();
  if (prev_arc) {
    TimingRole *role = prev_arc->role();
    if (role == TimingRole::latchDtoQ()) {
      Edge *prev_edge = start->prevEdge(prev_arc, sta_);
      if (latches->isLatchDtoQ(prev_edge)) {
	PathVertex enable_path;
	latches->latchEnablePath(start, prev_edge, enable_path);
	clk_path.init(enable_path);
      }
    }
    else if (role == TimingRole::regClkToQ()
	     || role == TimingRole::latchEnToQ())
      clk_path.init(startPrevPath());
  }
  else if (start->isClock(sta_))
    clk_path.init(start);
}

void
PathExpanded::latchPaths(// Return values.
			 PathRef *&d_path,
			 PathRef *&q_path,
			 Edge *&d_q_edge)
{
  d_path = nullptr;
  q_path = nullptr;
  d_q_edge = nullptr;
  PathRef *start = startPath();
  TimingArc *prev_arc = startPrevArc();
  if (prev_arc
      && prev_arc->role() == TimingRole::latchDtoQ()) {
    Edge *prev_edge = start->prevEdge(prev_arc, sta_);
    // This breaks latch loop paths.
    if (sta_->latches()->isLatchDtoQ(prev_edge)) {
      d_path = startPrevPath();
      q_path = start;
      d_q_edge = prev_edge;
    }
  }
}

} // namespace
