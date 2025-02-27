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

#include "SdcClass.hh"
#include "SearchClass.hh"

namespace sta {

// "Pointer" to a previous path on a vertex (PathVertex) thru an edge/arc.
class PathPrev
{
public:
  PathPrev();
  PathPrev(const PathVertex *path,
           const Edge *prev_edge,
           const TimingArc *prev_arc,
           const StaState *sta);
  void init();
  void init(const PathPrev *path);
  void init(const PathPrev &path);
  void init(const PathVertex *path,
            const Edge *prev_edge,
            const TimingArc *prev_arc,
	    const StaState *sta);
  bool isNull() const;
  const char *name(const StaState *sta) const;
  Vertex *vertex(const StaState *sta) const;
  VertexId vertexId(const StaState *sta) const;
  Edge *prevEdge(const StaState *sta) const;
  TimingArc *prevArc(const StaState *sta) const;
  Tag *tag(const StaState *sta) const;
  TagIndex tagIndex() const { return prev_tag_index_; }
  Arrival arrival(const StaState *sta) const;
  void prevPath(const StaState *sta,
		// Return values.
		PathRef &prev_path,
		TimingArc *&prev_arc) const;

  static bool equal(const PathPrev *path1,
		    const PathPrev *path2);
  static bool equal(const PathPrev &path1,
		    const PathPrev &path2);
  static int cmp(const PathPrev &path1,
		 const PathPrev &path2);

protected:
  EdgeId prev_edge_id_;
  TagIndex prev_tag_index_:tag_index_bit_count;
  unsigned prev_arc_idx_:2;
};

} // namespace
