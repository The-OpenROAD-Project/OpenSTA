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
#include "Path.hh"

namespace sta {

// Implements Path API for paths returned by PathEnum.
class PathEnumed : public Path
{
public:
  PathEnumed(VertexId vertex_id,
	     TagIndex tag_index,
	     Arrival arrival,
	     PathEnumed *prev_path,
	     TimingArc *prev_arc);
  virtual void setRef(PathRef *ref) const;
  virtual bool isNull() const { return vertex_id_ == 0; }
  virtual Vertex *vertex(const StaState *sta) const;
  virtual VertexId vertexId(const StaState *sta) const;
  virtual Tag *tag(const StaState *sta) const;
  virtual const RiseFall *transition(const StaState *sta) const;
  virtual int trIndex(const StaState *) const;
  virtual PathAnalysisPt *pathAnalysisPt(const StaState *sta) const;
  virtual PathAPIndex pathAnalysisPtIndex(const StaState *sta) const;
  virtual Arrival arrival(const StaState *sta) const;
  virtual void setArrival(Arrival arrival, const StaState *sta);
  virtual const Required &required(const StaState *sta) const;
  virtual void setRequired(const Required &required,
			   const StaState *sta);
  virtual Path *prevPath(const StaState *sta) const;
  virtual void prevPath(const StaState *sta,
			// Return values.
			PathRef &prev_path,
			TimingArc *&prev_arc) const;
  virtual TimingArc *prevArc(const StaState *sta) const;
  PathEnumed *prevPathEnumed() const;
  void setPrevPath(PathEnumed *prev);
  void setPrevArc(TimingArc *arc);
  void setTag(Tag *tag);

  using Path::setRef;
  using Path::prevPath;

protected:
  // Pointer to previous path.
  // A path is traversed by following prev_path/arcs.
  PathEnumed *prev_path_;
  TimingArc *prev_arc_;
  Arrival arrival_;
  VertexId vertex_id_;
  unsigned int tag_index_:tag_index_bits;

private:
  DISALLOW_COPY_AND_ASSIGN(PathEnumed);
};

void deletePathEnumed(PathEnumed *path);

} // namespace
