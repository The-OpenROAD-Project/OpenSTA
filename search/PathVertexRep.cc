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
#include "Graph.hh"
#include "SearchClass.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "Search.hh"
#include "PathVertex.hh"
#include "PathVertexRep.hh"

namespace sta {

PathVertexRep::PathVertexRep()
{
  init();
}

PathVertexRep::PathVertexRep(const PathVertexRep *path)
{
  init(path);
}

PathVertexRep::PathVertexRep(const PathVertexRep &path)
{
  init(path);
}

PathVertexRep::PathVertexRep(const PathVertex *path,
			     const StaState *sta)
{
  init(path, sta);
}

PathVertexRep::PathVertexRep(const PathVertex &path,
			     const StaState *sta)
{
  init(path, sta);
}

PathVertexRep::PathVertexRep(VertexId vertex_id,
			     TagIndex tag_index,
			     bool is_enum) :
  vertex_id_(vertex_id),
  tag_index_(tag_index),
  is_enum_(is_enum)
{
}

void
PathVertexRep::init()
{
  vertex_id_ = 0;
  tag_index_ = tag_index_null;
  is_enum_ = false;
}

void
PathVertexRep::init(const PathVertexRep *path)
{
  if (path) {
    vertex_id_ = path->vertex_id_;
    tag_index_ = path->tag_index_;
    is_enum_ = false;
  }
  else
    init();
}

void
PathVertexRep::init(const PathVertexRep &path)
{
  vertex_id_ = path.vertex_id_;
  tag_index_ = path.tag_index_;
  is_enum_ = false;
}

void
PathVertexRep::init(const PathVertex *path,
		    const StaState *sta)
{
  if (path == nullptr || path->isNull())
    init();
  else {
    vertex_id_ = sta->graph()->id(path->vertex(sta));
    tag_index_ = path->tag(sta)->index();
    is_enum_ = false;
  }
}

void
PathVertexRep::init(const PathVertex &path,
		    const StaState *sta)
{
  if (path.isNull())
    init();
  else {
    vertex_id_ = sta->graph()->id(path.vertex(sta));
    tag_index_ = path.tag(sta)->index();
    is_enum_ = false;
  }
}

Vertex *
PathVertexRep::vertex(const StaState *sta) const
{
  const Graph *graph = sta->graph();
  return graph->vertex(vertex_id_);
}

Tag *
PathVertexRep::tag(const StaState *sta) const
{
  const Search *search = sta->search();
  return search->tag(tag_index_);
}

Arrival
PathVertexRep::arrival(const StaState *sta) const
{ 
  const Graph *graph = sta->graph();
  const Search *search = sta->search();
  Tag *tag = search->tag(tag_index_);
  Vertex *vertex = graph->vertex(vertex_id_);
  TagGroup *tag_group = search->tagGroup(vertex);
  int arrival_index;
  bool arrival_exists;
  tag_group->arrivalIndex(tag, arrival_index, arrival_exists);
  if (!arrival_exists)
    internalError("tag group missing tag");
  Arrival *arrivals = graph->arrivals(vertex);
  return arrivals[arrival_index];
}

void
PathVertexRep::prevPath(const StaState *sta,
			// Return values.
			PathRef &prev_path,
			TimingArc *&prev_arc) const
{
  PathVertex path_vertex(this, sta);
  path_vertex.prevPath(sta, prev_path, prev_arc);
}

////////////////////////////////////////////////////////////////

bool
PathVertexRep::equal(const PathVertexRep *path1,
		     const PathVertexRep *path2)
{
  return path1->vertex_id_ == path2->vertex_id_
    && path1->tag_index_ == path2->tag_index_;
}

bool
PathVertexRep::equal(const PathVertexRep &path1,
		     const PathVertexRep &path2)
{
  return path1.vertex_id_ == path2.vertex_id_
    && path1.tag_index_ == path2.tag_index_;
}

int
PathVertexRep::cmp(const PathVertexRep *path1,
		   const PathVertexRep *path2)
{
  if (path1 && path2) {
    VertexId vertex_id1 = path1->vertexId();
    VertexId vertex_id2 = path2->vertexId();
    if (vertex_id1 == vertex_id2) {
      TagIndex tag_index1 = path1->tagIndex();
      TagIndex tag_index2 = path2->tagIndex();
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
PathVertexRep::cmp(const PathVertexRep &path1,
		   const PathVertexRep &path2)
{
  VertexId vertex_id1 = path1.vertexId();
  VertexId vertex_id2 = path2.vertexId();
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
