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

#include "PathVertexPtr.hh"

#include "Graph.hh"
#include "TimingArc.hh"
#include "SearchClass.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "Search.hh"
#include "PathAnalysisPt.hh"
#include "PathVertex.hh"

namespace sta {

PathVertexPtr::PathVertexPtr() :
  vertex_id_(vertex_id_null),
  tag_index_(tag_index_null)
{
}

PathVertexPtr::PathVertexPtr(const PathVertex *path,
			     const StaState *sta)
{
  init(path, sta);
}

void
PathVertexPtr::init()
{
  vertex_id_ = vertex_id_null;
  tag_index_ = tag_index_null;
}

void
PathVertexPtr::init(const PathVertexPtr *path)
{
  if (path) {
    vertex_id_ = path->vertex_id_;
    tag_index_ = path->tag_index_;
  }
  else {
    vertex_id_ = vertex_id_null;
    tag_index_ = tag_index_null;
  }
}

void
PathVertexPtr::init(const PathVertexPtr &path)
{
  vertex_id_ = path.vertex_id_;
  tag_index_ = path.tag_index_;
}

void
PathVertexPtr::init(const PathVertex *path,
		    const StaState *sta)
{
  if (path == nullptr || path->isNull())
    init();
  else {
    vertex_id_ = path->vertexId(sta);
    tag_index_ = path->tagIndex(sta);
  }
}

const char *
PathVertexPtr::name(const StaState *sta) const
{
  const Network *network = sta->network();
  const Search *search = sta->search();
  Vertex *vertex = this->vertex(sta);
  if (vertex) {
    const char *vertex_name = vertex->name(network);
    const Tag *tag = this->tag(search);
    const RiseFall *rf = tag->transition();
    const char *rf_str = rf->asString();
    const PathAnalysisPt *path_ap = tag->pathAnalysisPt(sta);
    int ap_index = path_ap->index();
    const char *min_max = path_ap->pathMinMax()->asString();
    TagIndex tag_index = tag->index();
    return stringPrintTmp("%s %s %s/%d %d",
			  vertex_name, rf_str, min_max,
			  ap_index, tag_index);
  }
  else
    return "NULL";
}

bool
PathVertexPtr::isNull() const
{
  return vertex_id_ == vertex_id_null;
}

Vertex *
PathVertexPtr::vertex(const StaState *sta) const
{
  if (vertex_id_ == vertex_id_null)
    return nullptr;
  else {
    const Graph *graph = sta->graph();
    return graph->vertex(vertex_id_);
  }
}

Tag *
PathVertexPtr::tag(const StaState *sta) const
{
  const Search *search = sta->search();
  return search->tag(tag_index_);
}

Arrival
PathVertexPtr::arrival(const StaState *sta) const
{
  const Vertex *vertex = this->vertex(sta);
  Arrival *arrivals = sta->graph()->arrivals(vertex);
  if (arrivals) {
    const Search *search = sta->search();
    TagGroup *tag_group = search->tagGroup(vertex);
    Tag *tag = this->tag(sta);
    int arrival_index;
    bool arrival_exists;
    tag_group->arrivalIndex(tag, arrival_index, arrival_exists);
    if (arrival_exists)
      return arrivals[arrival_index];
    else {
      sta->report()->error(1403, "missing arrival.");
      return 0.0;
    }
  }
  else {
    sta->report()->error(1404, "missing arrivals.");
    return 0.0;
  }
}

////////////////////////////////////////////////////////////////

bool
PathVertexPtr::equal(const PathVertexPtr *path1,
		     const PathVertexPtr *path2)
{
  return path1->vertex_id_ == path2->vertex_id_
    && path1->tag_index_ == path2->tag_index_;
}

bool
PathVertexPtr::equal(const PathVertexPtr &path1,
		     const PathVertexPtr &path2)
{
  return path1.vertex_id_ == path2.vertex_id_
    && path1.tag_index_ == path2.tag_index_;
}

int
PathVertexPtr::cmp(const PathVertexPtr &path1,
		   const PathVertexPtr &path2)
{
  VertexId vertex_id1 = path1.vertex_id_;
  VertexId vertex_id2 = path2.vertex_id_;
  if (vertex_id1 == vertex_id2) {
    TagIndex tag_index1 = path1.tagIndex();
    TagIndex tag_index2 = path2.tagIndex();
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

} // namespace
