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

#pragma once

#include "TimingArc.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"
#include "Path.hh"
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
  const Path *path(size_t index) const;
  // Returns the path start point.
  //  Register/Latch Q pin
  //  Input pin
  const Path *startPath() const;
  const Path *startPrevPath() const;
  const Path *endPath() const;
  const TimingArc *startPrevArc() const;
  size_t startIndex() const;
  const Path *clkPath() const;
  void latchPaths(// Return values.
		  const Path *&d_path,
		  const Path *&q_path,
		  Edge *&d_q_edge) const;

protected:
  void expandGenclk(const Path *clk_path);
  // Convert external index that starts at the path root
  // and increases to an index for paths_ (reversed).
  size_t pathsIndex(size_t index) const;

  // The Paths in paths_ are in reverse order.
  //  paths_[0] is the endpoint.
  //  paths_[size-1] is the beginning of the path.
  ConstPathSeq paths_;
  // Index of the startpoint.
  size_t start_index_;
  const StaState *sta_;
};

} // namespace
