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
#include "TagGroup.hh"
#include "PathEnumed.hh"
#include "PathVertex.hh"
#include "Search.hh"
#include "PathRef.hh"

namespace sta {

PathRef::PathRef() :
  path_enumed_(nullptr)
{
}

PathRef::PathRef(const Path *path) :
  path_enumed_(nullptr)
{
  if (path)
    path->setRef(this);
}

PathRef::PathRef(const PathRef &path) :
  path_vertex_(path.path_vertex_),
  path_enumed_(path.path_enumed_)
{
}

PathRef::PathRef(const PathRef *path) :
  path_vertex_(path->path_vertex_),
  path_enumed_(path->path_enumed_)
{
}

PathRef::PathRef(const PathVertex &path) :
  path_vertex_(&path),
  path_enumed_(nullptr)
{
}

void
PathRef::init()
{
  path_vertex_.init();
  path_enumed_ = nullptr;
}

void
PathRef::init(const PathRef &path)
{
  path_vertex_ = path.path_vertex_;
  path_enumed_ = path.path_enumed_;
}

void
PathRef::init(const PathRef *path)
{
  path_vertex_ = path->path_vertex_;
  path_enumed_ = path->path_enumed_;
}

void
PathRef::init(const PathVertex *path)
{
  path_vertex_ = path;
}

void
PathRef::init(const PathVertex &path)
{
  path_vertex_ = path;
}

void
PathRef::init(Vertex *vertex,
	      Tag *tag,
	      int arrival_index)
{
  path_vertex_.init(vertex, tag, arrival_index);
  path_enumed_ = nullptr;
}

void
PathRef::init(PathEnumed *path)
{
  path_enumed_ = path;
}

void
PathRef::setRef(PathRef *ref) const
{
  ref->path_vertex_ = path_vertex_;
  ref->path_enumed_ = path_enumed_;
}

void
PathRef::deleteRep()
{
  if (path_enumed_)
    deletePathEnumed(path_enumed_);
}

bool
PathRef::isNull() const
{
  return path_enumed_ == nullptr
    && path_vertex_.isNull();
}

Vertex *
PathRef::vertex(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->vertex(sta);
  else
    return path_vertex_.vertex(sta);
}

VertexId
PathRef::vertexId(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->vertexId(sta);
  else
    return path_vertex_.vertexId(sta);
}

Tag *
PathRef::tag(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->tag(sta);
  else
    return path_vertex_.tag(sta);
}

TagIndex
PathRef::tagIndex(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->tagIndex(sta);
  else
    return path_vertex_.tagIndex(sta);
}

const RiseFall *
PathRef::transition(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->transition(sta);
  else
    return path_vertex_.transition(sta);
}

int
PathRef::rfIndex(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->rfIndex(sta);
  else
    return path_vertex_.rfIndex(sta);
}

PathAnalysisPt *
PathRef::pathAnalysisPt(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->pathAnalysisPt(sta);
  else
    return path_vertex_.pathAnalysisPt(sta);
}

PathAPIndex
PathRef::pathAnalysisPtIndex(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->pathAnalysisPtIndex(sta);
  else
    return path_vertex_.pathAnalysisPtIndex(sta);
}

Arrival
PathRef::arrival(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->arrival(sta);
  else
    return path_vertex_.arrival(sta);
}

void
PathRef::setArrival(Arrival arrival,
		    const StaState *sta)
{
  if (path_enumed_)
    return path_enumed_->setArrival(arrival, sta);
  else
    return path_vertex_.setArrival(arrival, sta);
}

const Required &
PathRef::required(const StaState *sta) const
{
  if (path_enumed_)
    return path_enumed_->required(sta);
  else
    return path_vertex_.required(sta);
}

void
PathRef::setRequired(const Required &required, const StaState *sta)
{
  if (path_enumed_)
    return path_enumed_->setRequired(required, sta);
  else
    return path_vertex_.setRequired(required, sta);
}

void
PathRef::prevPath(const StaState *sta,
		  // Return values.
		  PathRef &prev_path, TimingArc *&prev_arc) const
{
  if (path_enumed_)
    path_enumed_->prevPath(sta, prev_path, prev_arc);
  else
    path_vertex_.prevPath(sta, prev_path, prev_arc);
}

void
PathRef::arrivalIndex(int &arrival_index,
		      bool &arrival_exists) const
{
  return path_vertex_.arrivalIndex(arrival_index, arrival_exists);
}

} // namespace
