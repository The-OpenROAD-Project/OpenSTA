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
#include "SearchClass.hh"
#include "Path.hh"

namespace sta {

class PathVertexRep;

// Implements Path API for a vertex.
class PathVertex : public Path
{
public:
  PathVertex();
  PathVertex(const PathVertex &path);
  PathVertex(const PathVertex *path);
  PathVertex(const PathVertexRep *path,
	     const StaState *sta);
  PathVertex(const PathVertexRep &path,
	     const StaState *sta);
  // If tag is not in the vertex tag group isNull() is true.
  PathVertex(Vertex *vertex,
	     Tag *tag,
	     const StaState *sta);
  PathVertex(Vertex *vertex,
	     Tag *tag,
	     int arrival_index);
  void init();
  void init(const PathVertexRep *path,
	    const StaState *sta);
  void init(const PathVertexRep &path,
	    const StaState *sta);
  void init(Vertex *vertex,
	    Tag *tag,
	    const StaState *sta);
  void init(Vertex *vertex,
	    Tag *tag,
	    int arrival_index);
  void operator=(const PathVertex &path);
  virtual bool isNull() const;
  virtual void setRef(PathRef *ref) const;
  virtual Vertex *vertex(const StaState *) const { return vertex_; }
  virtual VertexId vertexId(const StaState *sta) const;
  virtual Tag *tag(const StaState *) const { return tag_; }
  virtual TagIndex tagIndex(const StaState *sta) const;
  virtual const RiseFall *transition(const StaState *) const;
  virtual int rfIndex(const StaState *sta) const;
  virtual PathAnalysisPt *pathAnalysisPt(const StaState *sta) const;
  virtual PathAPIndex pathAnalysisPtIndex(const StaState *sta) const;
  void arrivalIndex(int &arrival_index,
		    bool &arrival_exists) const;
  void setArrivalIndex(int arrival_index);
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
  void prevPath(const StaState *sta,
		// Return values.
		PathVertex &prev_path) const;
  void prevPath(const StaState *sta,
		// Return values.
		PathVertex &prev_path,
		TimingArc *&prev_arc) const;
  static void deleteRequireds(Vertex *vertex,
			      const StaState *sta);
  static bool equal(const PathVertex *path1,
		    const PathVertex *path2);

  using Path::setRef;
  using Path::prevPath;

protected:
  Vertex *vertex_;
  Tag *tag_;
  int arrival_index_;

private:
  friend class PathRef;
  friend class VertexPathIterator;
  friend bool pathVertexEqual(const PathVertex *path1,
			      const PathVertex *path2);
};

// Iterator for vertex paths.
class VertexPathIterator : public Iterator<PathVertex*>
{
public:
  // Iterate over all vertex paths.
  VertexPathIterator(Vertex *vertex,
		     const StaState *sta);
  // Iterate over vertex paths with the same transition and
  // analysis pt but different tags.
  VertexPathIterator(Vertex *vertex,
		     const RiseFall *rf,
		     const PathAnalysisPt *path_ap,
		     const StaState *sta);
  // Iterate over vertex paths with the same transition and
  // analysis pt min/max but different tags.
  VertexPathIterator(Vertex *vertex,
		     const RiseFall *rf,
		     const MinMax *min_max,
		     const StaState *sta);
  virtual ~VertexPathIterator();
  virtual bool hasNext();
  virtual PathVertex *next();

private:
  DISALLOW_COPY_AND_ASSIGN(VertexPathIterator);
  void findNext();

  const Search *search_;
  Vertex *vertex_;
  const RiseFall *rf_;
  const PathAnalysisPt *path_ap_;
  const MinMax *min_max_;
  ArrivalMap::Iterator arrival_iter_;
  PathVertex path_;
  PathVertex next_;
};

} // namespace
