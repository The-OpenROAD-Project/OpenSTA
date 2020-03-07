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
#include "Debug.hh"
#include "Graph.hh"
#include "Bfs.hh"
#include "Search.hh"
#include "PathVertex.hh"
#include "PathEnd.hh"
#include "Tag.hh"
#include "VisitPathEnds.hh"
#include "VisitPathGroupVertices.hh"

namespace sta {

typedef Set<PathVertex*, PathLess> PathVertexSet;
typedef Map<Vertex*, PathVertexSet*> VertexPathSetMap;

static void
vertexPathSetMapInsertPath(VertexPathSetMap *matching_path_map,
			   Vertex *vertex,
			   Tag *tag,
			   int arrival_index,
			   const StaState *sta);

// Visit each path end for a vertex and add the worst one in each
// path group to the group.
class VisitPathGroupEnds : public PathEndVisitor
{
public:
  explicit VisitPathGroupEnds(PathGroup *path_group,
			      VertexVisitor *vertex_visitor,
			      VertexPathSetMap *matching_path_map,
			      BfsBkwdIterator *bkwd_iter,
			      StaState *sta);
  virtual PathEndVisitor *copy();
  virtual void visit(PathEnd *path_end);
  virtual void vertexBegin(Vertex *vertex);
  virtual void vertexEnd(Vertex *vertex);

private:
  DISALLOW_COPY_AND_ASSIGN(VisitPathGroupEnds);

  PathGroup *path_group_;
  VertexVisitor *vertex_visitor_;
  BfsBkwdIterator *bkwd_iter_;
  VertexPathSetMap *matching_path_map_;
  bool vertex_matches_;
  StaState *sta_;
};

class PathGroupPathVisitor : public PathVisitor
{
public:
  PathGroupPathVisitor(VertexVisitor *visitor,
		       BfsBkwdIterator *bkwd_iter,
		       VertexPathSetMap *matching_path_map,
		       const StaState *sta);
  virtual ~PathGroupPathVisitor();
  virtual VertexVisitor *copy();
  virtual void visit(Vertex *vertex);

protected:
  // Return false to stop visiting.
  virtual bool visitFromToPath(const Pin *from_pin,
			       Vertex *from_vertex,
			       const RiseFall *from_rf,
			       Tag *from_tag,
			       PathVertex *from_path,
			       Edge *edge,
			       TimingArc *arc,
			       ArcDelay arc_delay,
			       Vertex *to_vertex,
			       const RiseFall *to_rf,
			       Tag *to_tag,
			       Arrival &to_arrival,
			       const MinMax *min_max,
			       const PathAnalysisPt *path_ap);
  void fromMatches(Vertex *from_vertex,
		   Tag *from_tag,
		   int from_arrival_index);

private:
  VertexVisitor *visitor_;
  BfsBkwdIterator *bkwd_iter_;
  VertexPathSetMap *matching_path_map_;
  bool vertex_matches_;
};

////////////////////////////////////////////////////////////////

// Visit the fanin vertices for the path group.
// Vertices in the clock network are NOT visited.
void
visitPathGroupVertices(PathGroup *path_group,
		       VertexVisitor *visitor,
		       StaState *sta)
{
  Search *search = sta->search();
  VertexPathSetMap matching_path_map;
  // Do not visit clock network.
  SearchPredNonReg2 srch_non_reg(sta);
  BfsBkwdIterator bkwd_iter(BfsIndex::other, &srch_non_reg, sta);
  // Visit the path ends and filter by path_group to seed the backward search.
  VisitPathGroupEnds end_visitor(path_group, visitor, &matching_path_map,
				 &bkwd_iter, sta);
  VisitPathEnds visit_path_ends(sta);
  VertexSet::Iterator end_iter(search->endpoints());
  while (end_iter.hasNext()) {
    Vertex *vertex = end_iter.next();
    visit_path_ends.visitPathEnds(vertex, &end_visitor);
  }

  // Search backward from the path ends thru vertices that have arrival tags
  // that match path_group end paths.
  PathGroupPathVisitor path_visitor(visitor, &bkwd_iter, &matching_path_map,
				    sta);
  bkwd_iter.visit(0, &path_visitor);

  // Cleanup.
  VertexPathSetMap::Iterator matching_iter(matching_path_map);
  while (matching_iter.hasNext()) {
    PathVertexSet *paths = matching_iter.next();
    PathVertexSet::Iterator path_iter(paths);
    while (path_iter.hasNext()) {
      PathVertex *path = path_iter.next();
      delete path;
    }
    delete paths;
  }
}

////////////////////////////////////////////////////////////////

VisitPathGroupEnds::VisitPathGroupEnds(PathGroup *path_group,
				       VertexVisitor *vertex_visitor,
				       VertexPathSetMap *matching_path_map,
				       BfsBkwdIterator *bkwd_iter,
				       StaState *sta) :
  path_group_(path_group),
  vertex_visitor_(vertex_visitor),
  bkwd_iter_(bkwd_iter),
  matching_path_map_(matching_path_map),
  sta_(sta)
{
}

PathEndVisitor *
VisitPathGroupEnds::copy()
{
  return new VisitPathGroupEnds(path_group_, vertex_visitor_,
				matching_path_map_, bkwd_iter_, sta_);
}

void
VisitPathGroupEnds::vertexBegin(Vertex *)
{
  vertex_matches_ = false;
}

void
VisitPathGroupEnds::visit(PathEnd *path_end)
{
  PathGroup *group = sta_->search()->pathGroup(path_end);
  if (group == path_group_) {
    PathRef path(path_end->pathRef());
    Vertex *vertex = path.vertex(sta_);

    int arrival_index;
    bool arrival_exists;
    path.arrivalIndex(arrival_index, arrival_exists);
    vertexPathSetMapInsertPath(matching_path_map_, vertex, path.tag(sta_),
			       arrival_index, sta_);
    vertex_matches_ = true;
  }
}

static void
vertexPathSetMapInsertPath(VertexPathSetMap *matching_path_map,
			   Vertex *vertex,
			   Tag *tag,
			   int arrival_index,
			   const StaState *sta)
{
  PathVertexSet *matching_paths = matching_path_map->findKey(vertex);
  if (matching_paths == nullptr) {
    PathLess path_less(sta);
    matching_paths = new PathVertexSet(path_less);
    (*matching_path_map)[vertex] = matching_paths;
  }
  PathVertex *vpath = new PathVertex(vertex, tag, arrival_index);
  matching_paths->insert(vpath);
}

void
VisitPathGroupEnds::vertexEnd(Vertex *vertex)
{
  if (vertex_matches_) {
    vertex_visitor_->visit(vertex);
    // Seed backward bfs fanin search.
    bkwd_iter_->enqueueAdjacentVertices(vertex);
  }
}

////////////////////////////////////////////////////////////////

PathGroupPathVisitor::PathGroupPathVisitor(VertexVisitor *visitor,
					   BfsBkwdIterator *bkwd_iter,
					   VertexPathSetMap *matching_path_map,
					   const StaState *sta) :
  PathVisitor(sta),
  visitor_(visitor),
  bkwd_iter_(bkwd_iter),
  matching_path_map_(matching_path_map)
{
}

PathGroupPathVisitor::~PathGroupPathVisitor()
{
}

VertexVisitor *
PathGroupPathVisitor::copy()
{
  return new PathGroupPathVisitor(visitor_, bkwd_iter_, matching_path_map_,
				  sta_);
}

void
PathGroupPathVisitor::visit(Vertex *vertex)
{
  vertex_matches_ = false;
  visitFanoutPaths(vertex);
  if (vertex_matches_) {
    const Debug *debug = sta_->debug();
    debugPrint1(debug, "visit_path_group", 1, "visit %s\n",
		vertex->name(sta_->network()));
    visitor_->visit(vertex);
    bkwd_iter_->enqueueAdjacentVertices(vertex);
  }
}

bool
PathGroupPathVisitor::visitFromToPath(const Pin *,
				      Vertex *from_vertex,
				      const RiseFall *,
				      Tag *from_tag,
				      PathVertex *from_path,
				      Edge *,
				      TimingArc *,
				      ArcDelay ,
				      Vertex *to_vertex,
				      const RiseFall *to_rf,
				      Tag *to_tag,
				      Arrival &,
				      const MinMax *,
				      const PathAnalysisPt *path_ap)
{
  PathVertexSet *matching_paths = matching_path_map_->findKey(to_vertex);
  if (matching_paths) {
    int arrival_index;
    bool arrival_exists;
    from_path->arrivalIndex(arrival_index, arrival_exists);
    PathVertex to_path(to_vertex, to_tag, sta_);
    if (!to_path.isNull()) {
      if (matching_paths->hasKey(&to_path)) {
	const Debug *debug = sta_->debug();
	debugPrint4(debug, "visit_path_group", 2, "match %s %s -> %s %s\n",
		    from_vertex->name(sta_->network()),
		    from_tag->asString(sta_),
		    to_vertex->name(sta_->network()),
		    to_tag->asString(sta_));
	fromMatches(from_vertex, from_tag, arrival_index);
      }
    }
    else {
      VertexPathIterator to_iter(to_vertex, to_rf, path_ap, sta_);
      while (to_iter.hasNext()) {
	PathVertex *to_path = to_iter.next();
	if (tagMatchNoCrpr(to_path->tag(sta_), to_tag)
	    && matching_paths->hasKey(to_path)) {
	  const Debug *debug = sta_->debug();
	  debugPrint4(debug, "visit_path_group", 2, 
		      "match crpr %s %s -> %s %s\n",
		      from_vertex->name(sta_->network()),
		      from_tag->asString(sta_),
		      to_vertex->name(sta_->network()),
		      to_tag->asString(sta_));
	  fromMatches(from_vertex, from_tag, arrival_index);
	}
      }
    }
  }
  return true;
}

void
PathGroupPathVisitor::fromMatches(Vertex *from_vertex,
				  Tag *from_tag,
				  int from_arrival_index)
{
  vertex_matches_ = true;
  vertexPathSetMapInsertPath(matching_path_map_, from_vertex,
			     from_tag, from_arrival_index, sta_);
}

} // namespace
