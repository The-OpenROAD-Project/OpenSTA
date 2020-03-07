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
#include "Vector.hh"
#include "SearchClass.hh"
#include "Path.hh"
#include "PathVertex.hh"

namespace sta {

// Path reference to either a PathVertex or PathEnum.
// This "could" be made smaller by using a union for 
// path_vertex_.vertex_ and path_enumed_ and a non-legal
// value for path_vertex_.arrival_index_ (because a nullptr tag 
// in PathVertex is valid).
class PathRef : public Path
{
public:
  PathRef();
  PathRef(const Path *path);
  PathRef(const PathRef &path);
  PathRef(const PathRef *path);
  PathRef(const PathVertex &path);
  void init();
  void init(const PathRef &path);
  void init(const PathRef *path);
  void init(const PathVertex &path);
  void init(const PathVertex *path);
  void init(Vertex *vertex,
	    Tag *tag,
	    int arrival_index);
  void init(PathEnumed *path);
  virtual void setRef(PathRef *ref) const;
  virtual bool isNull() const;
  virtual Vertex *vertex(const StaState *sta) const;
  virtual VertexId vertexId(const StaState *sta) const;
  virtual Tag *tag(const StaState *sta) const;
  virtual TagIndex tagIndex(const StaState *sta) const;
  virtual const RiseFall *transition(const StaState *sta) const;
  virtual int rfIndex(const StaState *sta) const;
  virtual PathAnalysisPt *pathAnalysisPt(const StaState *sta) const;
  virtual PathAPIndex pathAnalysisPtIndex(const StaState *sta) const;
  void arrivalIndex(int &arrival_index,
		    bool &arrival_exists) const;
  virtual Arrival arrival(const StaState *sta) const;
  virtual void setArrival(Arrival arrival,
			  const StaState *sta);
  virtual const Required &required(const StaState *sta) const;
  virtual void setRequired(const Required &required,
			   const StaState *sta);
  virtual void prevPath(const StaState *sta,
			// Return values.
			PathRef &prev_path,
			TimingArc *&prev_arc) const;
  void deleteRep();

  using Path::setRef;
  using Path::prevPath;

protected:
  PathVertex path_vertex_;
  PathEnumed *path_enumed_;

private:
  friend class PathVertex;
  friend class PathEnumed;
};

} // namespace
