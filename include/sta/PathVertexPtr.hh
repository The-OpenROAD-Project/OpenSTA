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

#include "SearchClass.hh"

namespace sta {

// "Pointer" to a vertex path because there is no real path object to point to.
class PathVertexPtr
{
public:
  PathVertexPtr();
  PathVertexPtr(const PathVertex *path,
                const StaState *sta);
  void init();
  void init(const PathVertexPtr *path);
  void init(const PathVertexPtr &path);
  void init(const PathVertex *path,
	    const StaState *sta);
  bool isNull() const;
  const char *name(const StaState *sta) const;
  Vertex *vertex(const StaState *sta) const;
  VertexId vertexId() const { return vertex_id_; }
  Tag *tag(const StaState *sta) const;
  TagIndex tagIndex() const { return tag_index_; }
  Arrival arrival(const StaState *sta) const;

  static bool equal(const PathVertexPtr *path1,
		    const PathVertexPtr *path2);
  static bool equal(const PathVertexPtr &path1,
		    const PathVertexPtr &path2);
  static int cmp(const PathVertexPtr &path1,
		 const PathVertexPtr &path2);

protected:
  VertexId vertex_id_;
  TagIndex tag_index_;
};

} // namespace
