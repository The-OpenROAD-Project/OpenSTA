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
#include "Set.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "Search.hh"
#include "Tag.hh"
#include "PathRef.hh"
#include "PathEnumed.hh"

namespace sta {

PathEnumed:: PathEnumed(VertexId vertex_id,
		       TagIndex tag_index,
		       Arrival arrival,
		       PathEnumed *prev_path,
		       TimingArc *prev_arc) :
  Path(),
  prev_path_(prev_path),
  prev_arc_(prev_arc),
  arrival_(arrival),
  vertex_id_(vertex_id),
  tag_index_(tag_index)
{
}

void
deletePathEnumed(PathEnumed *path)
{
  while (path) {
    PathEnumed *prev = path->prevPathEnumed();
    delete path;
    path = prev;
  }
}

void
PathEnumed::setRef(PathRef *ref) const
{
  ref->init(const_cast<PathEnumed*>(this));
}

Vertex *
PathEnumed::vertex(const StaState *sta) const
{
  const Graph *graph = sta->graph();
  return graph->vertex(vertex_id_);
}

VertexId
PathEnumed::vertexId(const StaState *) const
{
  return vertex_id_;
}

Tag *
PathEnumed::tag(const StaState *sta) const
{
  const Search *search = sta->search();
  return search->tag(tag_index_);
}

void
PathEnumed::setTag(Tag *tag)
{
  tag_index_ = tag->index();
}

const RiseFall *
PathEnumed::transition(const StaState *sta) const
{
  return tag(sta)->transition();
}

int
PathEnumed::trIndex(const StaState *sta) const
{
  return tag(sta)->trIndex();
}

PathAnalysisPt *
PathEnumed::pathAnalysisPt(const StaState *sta) const
{
  const Corners *corners = sta->corners();
  return corners->findPathAnalysisPt(pathAnalysisPtIndex(sta));
}

PathAPIndex
PathEnumed::pathAnalysisPtIndex(const StaState *sta) const
{
  return tag(sta)->pathAPIndex();
}

Arrival
PathEnumed::arrival(const StaState *) const
{
  return arrival_;
}

void
PathEnumed::setArrival(Arrival arrival,
		       const StaState *)
{
  arrival_ = arrival;
}

const Required &
PathEnumed::required(const StaState *) const
{
  // Required times are never needed for enumerated paths.
  internalError("enumerated path required time\n");
}

void
PathEnumed::setRequired(const Required &,
			const StaState *)
{
  // Required times are never needed for enumerated paths.
  internalError("enumerated path required time\n");
}

Path *
PathEnumed::prevPath(const StaState *) const
{
  return prev_path_;
}

void
PathEnumed::prevPath(const StaState *,
		     // Return values.
		     PathRef &prev_path,
		     TimingArc *&prev_arc) const
{
  if (prev_path_) {
    prev_path_->setRef(prev_path);
    prev_arc = prev_arc_;
  }
  else {
    prev_path.init();
    prev_arc = nullptr;
  }
}

TimingArc *
PathEnumed::prevArc(const StaState *) const
{
  return prev_arc_;
}

PathEnumed *
PathEnumed::prevPathEnumed() const
{
  return prev_path_;
}

void
PathEnumed::setPrevPath(PathEnumed *prev)
{
  prev_path_ = prev;
}

void
PathEnumed::setPrevArc(TimingArc *arc)
{
  prev_arc_ = arc;
}

} // namespace
