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
#include "GraphClass.hh"
#include "SearchClass.hh"
#include "TimingArc.hh"
#include "StaState.hh"

namespace sta {

class PathExpanded
{
public:
  PathExpanded(const StaState *sta);
  // Expand path for lookup by index.
  PathExpanded(const Path *path,
	       const StaState *sta);
  PathExpanded(const Path *path,
	       // Expand generated clk source paths.
	       bool expand_genclks,
	       const StaState *sta);
  void expand(const Path *path,
	      bool expand_genclks);
  size_t size() const { return paths_.size(); }
  // path(0) is the startpoint.
  // path(size()-1) is the endpoint.
  PathRef *path(size_t index);
  TimingArc *prevArc(size_t index);
  // Returns the path start point.
  //  Register/Latch Q pin
  //  Input pin
  PathRef *startPath();
  PathRef *startPrevPath();
  PathRef *endPath();
  TimingArc *startPrevArc();
  size_t startIndex() const;
  void clkPath(PathRef &clk_path);
  void latchPaths(// Return values.
		  PathRef *&d_path,
		  PathRef *&q_path,
		  Edge *&d_q_edge);

protected:
  void expandGenclk(PathRef *clk_path);
  // Convert external index that starts at the path root
  // and increases to an index for paths_ (reversed).
  size_t pathsIndex(size_t index) const;

  // The PathRefs in paths_ are in reverse order.
  //  paths_[0] is the endpoint.
  //  paths_[size-1] is the beginning of the path.
  PathRefSeq paths_;
  TimingArcSeq prev_arcs_;
  // Index of the startpoint.
  size_t start_index_;
  const StaState *sta_;

private:
  DISALLOW_COPY_AND_ASSIGN(PathExpanded);
};

} // namespace
