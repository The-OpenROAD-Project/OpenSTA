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

#include "PathPrev.hh"

#include "Graph.hh"
#include "TimingArc.hh"
#include "SearchClass.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "Search.hh"
#include "PathAnalysisPt.hh"
#include "PathVertex.hh"

namespace sta {

PathPrev::PathPrev()
{
  init();
}

void
PathPrev::init()
{
  prev_edge_id_ = edge_id_null;
  prev_arc_idx_ = 0;
  prev_tag_index_ = tag_index_null;
}

void
PathPrev::init(const PathPrev *path)
{
  if (path) {
    prev_edge_id_ = path->prev_edge_id_;
    prev_arc_idx_ = path->prev_arc_idx_;
    prev_tag_index_ = path->prev_tag_index_;
  }
  else
    init();
}

void
PathPrev::init(const PathPrev &path)
{
  prev_edge_id_ = path.prev_edge_id_;
  prev_arc_idx_ = path.prev_arc_idx_;
  prev_tag_index_ = path.prev_tag_index_;
}

void
PathPrev::init(const PathVertex *path,
                    const Edge *prev_edge,
                    const TimingArc *prev_arc,
                    const StaState *sta)
{
  if (path == nullptr || path->isNull())
    init();
  else {
    const Graph *graph = sta->graph();
    prev_edge_id_ = graph->id(prev_edge);
    prev_arc_idx_ = prev_arc->index();
    prev_tag_index_ = path->tag(sta)->index();
  }
}

const char *
PathPrev::name(const StaState *sta) const
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
PathPrev::isNull() const
{
  return prev_edge_id_ == edge_id_null;
}

VertexId
PathPrev::vertexId(const StaState *sta) const
{
  if (prev_edge_id_ == edge_id_null)
    return vertex_id_null;
  else {
    const Graph *graph = sta->graph();
    const Edge *edge = graph->edge(prev_edge_id_);
    return edge->from();
  }
}

Vertex *
PathPrev::vertex(const StaState *sta) const
{
  if (prev_edge_id_ == edge_id_null)
    return nullptr;
  else {
    const Graph *graph = sta->graph();
    const Edge *edge = graph->edge(prev_edge_id_);
    return edge->from(graph);
  }
}

Edge *
PathPrev::prevEdge(const StaState *sta) const
{
  if (prev_edge_id_ == edge_id_null)
    return nullptr;
  else {
    const Graph *graph = sta->graph();
    return graph->edge(prev_edge_id_);
  }
}

TimingArc *
PathPrev::prevArc(const StaState *sta) const
{
  if (prev_edge_id_ == edge_id_null)
    return nullptr;
  else {
    const Graph *graph = sta->graph();
    const Edge *edge = graph->edge(prev_edge_id_);
    TimingArcSet *arc_set = edge->timingArcSet();
    return arc_set->findTimingArc(prev_arc_idx_);
  }
}

Tag *
PathPrev::tag(const StaState *sta) const
{
  const Search *search = sta->search();
  return search->tag(prev_tag_index_);
}

Arrival
PathPrev::arrival(const StaState *sta) const
{ 
  Graph *graph = sta->graph();
  const Search *search = sta->search();
  Tag *tag = search->tag(prev_tag_index_);
  Vertex *vertex = this->vertex(sta);
  TagGroup *tag_group = search->tagGroup(vertex);
  if (tag_group) {
    int arrival_index;
    bool arrival_exists;
    tag_group->arrivalIndex(tag, arrival_index, arrival_exists);
    if (!arrival_exists)
      sta->report()->critical(1420, "tag group missing tag");
    Arrival *arrivals = graph->arrivals(vertex);
    if (arrivals)
      return arrivals[arrival_index];
    else
      sta->report()->critical(1421, "missing arrivals");
  }
  else
    sta->report()->error(1422, "missing arrivals.");
  return 0.0;
}

void
PathPrev::prevPath(const StaState *sta,
			// Return values.
			PathRef &prev_path,
			TimingArc *&prev_arc) const
{
  PathVertex path_vertex(this, sta);
  path_vertex.prevPath(sta, prev_path, prev_arc);
}

////////////////////////////////////////////////////////////////

bool
PathPrev::equal(const PathPrev *path1,
		     const PathPrev *path2)
{
  return path1->prev_edge_id_ == path2->prev_edge_id_
    && path1->prev_tag_index_ == path2->prev_tag_index_;
}

bool
PathPrev::equal(const PathPrev &path1,
		     const PathPrev &path2)
{
  return path1.prev_edge_id_ == path2.prev_edge_id_
    && path1.prev_tag_index_ == path2.prev_tag_index_;
}

int
PathPrev::cmp(const PathPrev &path1,
		   const PathPrev &path2)
{
  EdgeId edge_id1 = path1.prev_edge_id_;
  EdgeId edge_id2 = path2.prev_edge_id_;
  if (edge_id1 == edge_id2) {
    TagIndex tag_index1 = path1.prev_tag_index_;
    TagIndex tag_index2 = path2.prev_tag_index_;
    if (tag_index1 == tag_index2)
      return 0;
    else if (tag_index1 < tag_index2)
      return -1;
    else
      return 1;
  }
  else if (edge_id1 < edge_id2)
    return -1;
  else
    return 1;
}

} // namespace
