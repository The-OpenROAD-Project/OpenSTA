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

#include "Levelize.hh"

#include <algorithm>
#include <deque>

#include "Report.hh"
#include "Debug.hh"
#include "Stats.hh"
#include "TimingRole.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "GraphCmp.hh"
#include "SearchPred.hh"
#include "Variables.hh"
#include "GraphDelayCalc.hh"

namespace sta {

using std::max;

Levelize::Levelize(StaState *sta) :
  StaState(sta),
  search_pred_(sta),
  levelized_(false),
  levels_valid_(false),
  max_level_(0),
  level_space_(10),
  roots_(graph_),
  relevelize_from_(graph_),
  observer_(nullptr)
{
}

Levelize::~Levelize()
{
  delete observer_;
  loops_.deleteContents();
}

void
Levelize::setLevelSpace(Level space)
{
  level_space_ = space;
}

void
Levelize::setObserver(LevelizeObserver *observer)
{
  delete observer_;
  observer_ = observer;
}

void
Levelize::clear()
{
  levelized_ = false;
  levels_valid_ = false;
  roots_.clear();
  relevelize_from_.clear();
  clearLoopEdges();
  loops_.deleteContentsClear();
  loop_edges_.clear();
  max_level_ = 0;
}

void
Levelize::clearLoopEdges()
{
  EdgeSet::Iterator edge_iter(disabled_loop_edges_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    edge->setIsDisabledLoop(false);
  }
  disabled_loop_edges_.clear();
}

void
Levelize::ensureLevelized()
{
  if (!levels_valid_) {
    if (levelized_)
      relevelize();
    else
      levelize();
  }
}

#define onPath() visied2()
#define setOnPath(on_path) setVisited2(on_path)

void
Levelize::levelize()
{
  Stats stats(debug_, report_);
  debugPrint(debug_, "levelize", 1, "levelize");
  clear();
  if (observer_)
    observer_->levelsChangedBefore();

  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    // findBackEdges() init
    vertex->setVisited(false);
    vertex->setOnPath(false);
    // assignLevels init
    vertex->setLevel(-1);
  }

  findRoots();
  findBackEdges();
  VertexSeq topo_sorted = findTopologicalOrder();
  assignLevels(topo_sorted);
  ensureLatchLevels();

  // Set level of stranded vertices (constants) to zero.
  VertexIterator vertex_iter2(graph_);
  while (vertex_iter2.hasNext()) {
    Vertex *vertex = vertex_iter2.next();
    if (vertex->level() == -1)
      setLevel(vertex, 0);
    // cleanup
    vertex->setVisited(false);
    vertex->setOnPath(false);
  }
  relevelize_from_.clear();
  levelized_ = true;
  levels_valid_ = true;
  stats.report("Levelize");
}

void
Levelize::findRoots()
{
  roots_.clear();
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (isRoot(vertex)) {
      debugPrint(debug_, "levelize", 2, "root %s%s",
                 vertex->to_string(this).c_str(),
                 hasFanout(vertex) ? " fanout" : "");
      roots_.insert(vertex);
    }
  }
  if (debug_->check("levelize", 1)) {
    size_t fanout_roots = 0;
    for (Vertex *root : roots_) {
      if (hasFanout(root))
          fanout_roots++;
    }
    debugPrint(debug_, "levelize", 1, "Found %zu roots %zu with fanout",
               roots_.size(),
               fanout_roots);
  }
}

// Root vertices have at no non-disabled edges entering them
// and are not disabled and have non-disabled fanout edges.
bool
Levelize::isRoot(Vertex *vertex)
{
  if (search_pred_.searchTo(vertex)) {
    VertexInEdgeIterator edge_iter1(vertex, graph_);
    while (edge_iter1.hasNext()) {
      Edge *edge = edge_iter1.next();
      Vertex *from_vertex = edge->from(graph_);
      if (search_pred_.searchFrom(from_vertex)
	  && search_pred_.searchThru(edge))
        return false;
    }
    // Levelize bidirect driver as if it was a fanout of the bidirect load.
    return !(graph_delay_calc_->bidirectDrvrSlewFromLoad(vertex->pin())
             && vertex->isBidirectDriver());
  }
  else
    return false;
}

bool
Levelize::hasFanout(Vertex *vertex)
{
  bool has_fanout = false;
  if (search_pred_.searchFrom(vertex)) {
    VertexOutEdgeIterator edge_iter2(vertex, graph_);
    while (edge_iter2.hasNext()) {
      Edge *edge = edge_iter2.next();
      Vertex *to_vertex = edge->from(graph_);
      if (search_pred_.searchTo(to_vertex)
	  && search_pred_.searchThru(edge)) {
        has_fanout = true;
        break;
      }
    }
    // Levelize bidirect driver as if it was a fanout of the bidirect load.
    if (graph_delay_calc_->bidirectDrvrSlewFromLoad(vertex->pin())
        && !vertex->isBidirectDriver())
      has_fanout = true;
  }
  return has_fanout;
}

// Non-recursive DFS to find back edges so the graph is acyclic.
void
Levelize::findBackEdges()
{
  Stats stats(debug_, report_);
  EdgeSeq path;
  FindBackEdgesStack stack;

  VertexSeq sorted_roots = sortedRootsWithFanout();
  for (Vertex *vertex : sorted_roots) {
    vertex->setVisited(true);
    vertex->setOnPath(true);
    stack.emplace(vertex, new VertexOutEdgeIterator(vertex, graph_));
  }

  findBackEdges(path, stack);
  findCycleBackEdges();
  stats.report("Levelize find back edges");
}

VertexSeq
Levelize::sortedRootsWithFanout()
{
  VertexSeq roots;
  for (Vertex *root : roots_) {
    if (hasFanout(root))
      roots.push_back(root);
  }
  // Sort the roots so that loop breaking is stable in regressions.
  // Skip sorting if it will take a long time.
  if (roots.size() < 100)
    sort(roots, VertexNameLess(network_));
  return roots;
}

EdgeSet
Levelize::findBackEdges(EdgeSeq &path,
                        FindBackEdgesStack &stack)
{
  EdgeSet back_edges;
  while (!stack.empty()) {
    VertexEdgeIterPair vertex_iter = stack.top();
    Vertex *vertex = vertex_iter.first;
    VertexOutEdgeIterator *edge_iter = vertex_iter.second;
    if (edge_iter->hasNext()) {
      Edge *edge = edge_iter->next();
      if (search_pred_.searchThru(edge)) {
        Vertex *to_vertex = edge->to(graph_);
        if (!to_vertex->visited()) {
          to_vertex->setVisited(true);
          to_vertex->setOnPath(true);
          path.push_back(edge);
          stack.emplace(to_vertex, new VertexOutEdgeIterator(to_vertex, graph_));
        }
        else if (to_vertex->visited2()) { // on path
          // Found a back edge (loop).
          recordLoop(edge, path);
          back_edges.insert(edge);
        }
      }
    }
    else {
      delete edge_iter;
      stack.pop();
      vertex->setOnPath(false);
      if (!path.empty())
        path.pop_back();
    }
  }
  return back_edges;
}

// Find back edges in cycles that are were not accessible from roots.
// Add roots for the disabled back edges so they are become accessible.
void
Levelize::findCycleBackEdges()
{
  // Search root-less cycles for back edges.
  VertexSeq unvisited = findUnvisitedVertices();
  // Sort cycle vertices so results are stable.
  // Skip sorting if it will take a long time.
  if (unvisited.size() < 100)
    sort(unvisited, VertexNameLess(network_));
  size_t back_edge_count = 0;
  VertexSet visited(graph_);
  for (Vertex *vertex : unvisited) {
    if (visited.find(vertex) == visited.end()) {
      VertexSet path_vertices(graph_);
      EdgeSeq path;
      FindBackEdgesStack stack;
      visited.insert(vertex);
      path_vertices.insert(vertex);
      stack.emplace(vertex, new VertexOutEdgeIterator(vertex, graph_));
      EdgeSet back_edges = findBackEdges(path, stack);
      for (Edge *back_edge : back_edges)
        roots_.insert(back_edge->from(graph_));
      back_edge_count += back_edges.size();
    }
  }
  debugPrint(debug_, "levelize", 1, "Found %zu cycle back edges", back_edge_count);
}

// Find vertices in cycles that are were not accessible from roots.
VertexSeq
Levelize::findUnvisitedVertices()
{
  VertexSeq unvisited;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (!vertex->visited()
	&& search_pred_.searchFrom(vertex))
      unvisited.push_back(vertex);
  }
  return unvisited;
}

////////////////////////////////////////////////////////////////

VertexSeq
Levelize::findTopologicalOrder()
{
  Stats stats(debug_, report_);
  std::map<Vertex*, int> in_degree;

  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (search_pred_.searchFrom(vertex)) {
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        Vertex *to_vertex = edge->to(graph_);
        if (search_pred_.searchThru(edge)
            && search_pred_.searchTo(to_vertex))
          in_degree[to_vertex] += 1;
        if (edge->role() == TimingRole::latchDtoQ())
          latch_d_to_q_edges_.insert(edge);
      }
      // Levelize bidirect driver as if it was a fanout of the bidirect load.
      const Pin *pin = vertex->pin();
      if (graph_delay_calc_->bidirectDrvrSlewFromLoad(pin)
          && !vertex->isBidirectDriver()) {
        Vertex *to_vertex = graph_->pinDrvrVertex(pin);;
        if (search_pred_.searchTo(to_vertex))
          in_degree[to_vertex] += 1;
      }
    }
  }

  std::deque<Vertex*> queue;
  for (Vertex *root : roots_)
    queue.push_back(root);

  VertexSeq topo_order;
  while (!queue.empty()) {
    Vertex *vertex = queue.front();
    queue.pop_front();
    topo_order.push_back(vertex);
    if (search_pred_.searchFrom(vertex)) {
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        Vertex *to_vertex = edge->to(graph_);
        if (search_pred_.searchThru(edge)
            && search_pred_.searchTo(to_vertex)) {
          const auto &to_degree_itr = in_degree.find(to_vertex);
          int &to_in_degree = to_degree_itr->second;
          to_in_degree -= 1;
          if (to_in_degree == 0)
            queue.push_back(to_vertex);
        }
      }
    }
    // Levelize bidirect driver as if it was a fanout of the bidirect load.
    const Pin *pin = vertex->pin();
    if (graph_delay_calc_->bidirectDrvrSlewFromLoad(pin)
        && !vertex->isBidirectDriver()) {
      Vertex *to_vertex = graph_->pinDrvrVertex(pin);
      if (search_pred_.searchTo(to_vertex)) {
        const auto &degree_itr = in_degree.find(to_vertex);
        int &in_degree = degree_itr->second;
        in_degree -= 1;
        if (in_degree == 0)
          queue.push_back(to_vertex);
      }
    }
  }

  if (debug_->check("levelize", 1)) {
    VertexIterator vertex_iter(graph_);
    while (vertex_iter.hasNext()) {
      Vertex *vertex = vertex_iter.next();
      if (in_degree[vertex] != 0)
        debugPrint(debug_, "levelize", 2, "topological sort missing %s",
                   vertex->to_string(this).c_str());
    }
  }
  if (debug_->check("levelize", 3)) {
    report_->reportLine("Topological sort");
    for (Vertex *vertex : topo_order)
      report_->reportLine("%s", vertex->to_string(this).c_str());
  }
  stats.report("Levelize topological sort");
  return topo_order;
}

void
Levelize::recordLoop(Edge *edge,
		     EdgeSeq &path)
{
  debugPrint(debug_, "levelize", 2, "Loop edge %s (%s)",
             edge->to_string(this).c_str(),
             edge->role()->to_string().c_str());
  EdgeSeq *loop_edges = loopEdges(path, edge);
  GraphLoop *loop = new GraphLoop(loop_edges);
  loops_.push_back(loop);
  if (variables_->dynamicLoopBreaking())
    sdc_->makeLoopExceptions(loop);

  // Record disabled loop edges so they can be cleared without
  // traversing the entire graph to find them.
  disabled_loop_edges_.insert(edge);
  edge->setIsDisabledLoop(true);
}

EdgeSeq *
Levelize::loopEdges(EdgeSeq &path,
		    Edge *closing_edge)
{
  debugPrint(debug_, "loop", 2, "Loop");
  EdgeSeq *loop_edges = new EdgeSeq;
  // Skip the "head" of the path up to where closing_edge closes the loop.
  Pin *loop_pin = closing_edge->to(graph_)->pin();
  bool copy = false;
  EdgeSeq::Iterator edge_iter(path);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Pin *from_pin = edge->from(graph_)->pin();
    if (from_pin == loop_pin)
      copy = true;
    if (copy) {
      debugPrint(debug_, "loop", 2, " %s",
                 edge->to_string(this).c_str());
      loop_edges->push_back(edge);
      loop_edges_.insert(edge);
    }
  }
  debugPrint(debug_, "loop", 2, " %s",
             closing_edge->to_string(this).c_str());
  loop_edges->push_back(closing_edge);
  loop_edges_.insert(closing_edge);
  return loop_edges;
}

void
Levelize::reportPath(EdgeSeq &path) const
{
  bool first_edge = true;
  EdgeSeq::Iterator edge_iter(path);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (first_edge)
      report_->reportLine(" %s", edge->from(graph_)->to_string(this).c_str());
    report_->reportLine(" %s", edge->to(graph_)->to_string(this).c_str());
    first_edge = false;
  }
}

////////////////////////////////////////////////////////////////

void
Levelize::assignLevels(VertexSeq &topo_sorted)
{
  for (Vertex *root : roots_)
    setLevel(root, 0);
  for (Vertex *vertex : topo_sorted) {
    if (vertex->level() != -1
	&& search_pred_.searchFrom(vertex)) {
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        Vertex *to_vertex = edge->to(graph_);
        if (search_pred_.searchThru(edge)
            && search_pred_.searchTo(to_vertex))
          setLevel(to_vertex, max(to_vertex->level(),
                                  vertex->level() + level_space_));
      }
      // Levelize bidirect driver as if it was a fanout of the bidirect load.
      const Pin *pin = vertex->pin();
      if (graph_delay_calc_->bidirectDrvrSlewFromLoad(pin)
          && !vertex->isBidirectDriver()) {
        Vertex *to_vertex = graph_->pinDrvrVertex(pin);
        if (search_pred_.searchTo(to_vertex))
          setLevel(to_vertex, max(to_vertex->level(),
                                  vertex->level() + level_space_));
      }
    }
  }
}

////////////////////////////////////////////////////////////////

// Make sure latch D input level is not the same as the Q level.
// This is because the Q arrival depends on the D arrival and
// to find them in parallel they have to be scheduled separately
// to avoid a race condition.
void
Levelize::ensureLatchLevels()
{
  EdgeSet::Iterator latch_edge_iter(latch_d_to_q_edges_);
  while (latch_edge_iter.hasNext()) {
    Edge *edge = latch_edge_iter.next();
    Vertex *from = edge->from(graph_);
    Vertex *to = edge->to(graph_);
    if (from->level() == to->level())
      setLevel(from, from->level() + level_space_);
  }
  latch_d_to_q_edges_.clear();
}

void
Levelize::setLevel(Vertex  *vertex,
		   Level level)
{
  debugPrint(debug_, "levelize", 2, "set level %s %d",
             vertex->to_string(this).c_str(),
             level);
  vertex->setLevel(level);
  max_level_ = max(level, max_level_);
  if (level >= Graph::vertex_level_max)
    report_->critical(616, "maximum logic level exceeded");
}

void
Levelize::invalid()
{
  if (levelized_) {
    debugPrint(debug_, "levelize", 1, "levels invalid");
    levelized_ = false;
    levels_valid_ = false;
  }
}

void
Levelize::invalidFrom(Vertex *vertex)
{
  if (levelized_) {
    debugPrint(debug_, "levelize", 1, "level invalid from %s",
               vertex->to_string(this).c_str());
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph_);
      relevelize_from_.insert(from_vertex);
    }
    relevelize_from_.insert(vertex);
    levels_valid_ = false;
  }
}

void
Levelize::deleteVertexBefore(Vertex *vertex)
{
  if (levelized_) {
    roots_.erase(vertex);
    relevelize_from_.erase(vertex);
  }
}

void
Levelize::relevelizeFrom(Vertex *vertex)
{
  if (levelized_) {
    debugPrint(debug_, "levelize", 1, "level invalid from %s",
               vertex->to_string(this).c_str());
    relevelize_from_.insert(vertex);
    levels_valid_ = false;
  }
}

void
Levelize::deleteEdgeBefore(Edge *edge)
{
  if (levelized_
      && loop_edges_.hasKey(edge)) {
    debugPrint(debug_, "levelize", 2, "delete loop edge %s",
               edge->to_string(this).c_str());
    disabled_loop_edges_.erase(edge);
    // Relevelize if a loop edge is removed. Incremental levelization
    // fails because the DFS path will be missing.
    levelized_ = false;
    levels_valid_ = false;
  }
}

// Incremental relevelization.
// Note that if vertices or edges are removed from the graph the
// downstream levels will NOT be reduced to the "correct" level (the
// search will immediately terminate without visiting downstream
// vertices because the new level is less than the existing level).
// This is acceptable because the BFS search that depends on the
// levels only requires that a vertex level be greater than that of
// its predecessors.
void
Levelize::relevelize()
{
  for (Vertex *vertex : relevelize_from_) {
    debugPrint(debug_, "levelize", 1, "relevelize from %s",
               vertex->to_string(this).c_str());
    if (search_pred_.searchFrom(vertex)) {
      if (isRoot(vertex))
	roots_.insert(vertex);
      VertexSet path_vertices(graph_);
      EdgeSeq path;
      visit(vertex, nullptr, vertex->level(), 1, path_vertices, path);
    }
  }
  ensureLatchLevels();
  levels_valid_ = true;
  relevelize_from_.clear();
}

void
Levelize::visit(Vertex *vertex,
		Edge *from,
                Level level,
		Level level_space,
                VertexSet &path_vertices,
		EdgeSeq &path)
{
  Pin *from_pin = vertex->pin();
  setLevelIncr(vertex, level);
  path_vertices.insert(vertex);
  if (from)
    path.push_back(from);

  if (search_pred_.searchFrom(vertex)) {
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (search_pred_.searchThru(edge)
	  && search_pred_.searchTo(to_vertex)) {
        if (path_vertices.find(to_vertex) != path_vertices.end())
	  // Back edges form feedback loops.
          recordLoop(edge, path);
        else if (to_vertex->level() <= level)
	  visit(to_vertex, edge, level+level_space, level_space,
		path_vertices, path);
      }
      if (edge->role() == TimingRole::latchDtoQ())
	  latch_d_to_q_edges_.insert(edge);
    }
    // Levelize bidirect driver as if it was a fanout of the bidirect load.
    if (graph_delay_calc_->bidirectDrvrSlewFromLoad(from_pin)
	&& !vertex->isBidirectDriver()) {
      Vertex *to_vertex = graph_->pinDrvrVertex(from_pin);
      if (search_pred_.searchTo(to_vertex)
	  && (to_vertex->level() <= level))
	visit(to_vertex, nullptr, level+level_space, level_space,
	      path_vertices, path);
    }
  }
  path_vertices.erase(vertex);
  if (from)
    path.pop_back();
}

bool
Levelize::isDisabledLoop(Edge *edge) const
{
  return disabled_loop_edges_.hasKey(edge);
}

void
Levelize::setLevelIncr(Vertex  *vertex,
                       Level level)
{
  debugPrint(debug_, "levelize", 2, "set level %s %d",
             vertex->to_string(this).c_str(),
             level);
  if (vertex->level() != level) {
    if (observer_)
      observer_->levelChangedBefore(vertex);
    vertex->setLevel(level);
  }
  max_level_ = max(level, max_level_);
  if (level >= Graph::vertex_level_max)
    criticalError(617, "maximum logic level exceeded");
}

void
Levelize::checkLevels()
{
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (search_pred_.searchTo(vertex)) {
      Level level = vertex->level();
      VertexInEdgeIterator edge_iter1(vertex, graph_);
      while (edge_iter1.hasNext()) {
	Edge *edge = edge_iter1.next();
	Vertex *from_vertex = edge->from(graph_);
	Level from_level = from_vertex->level();
	if (search_pred_.searchFrom(from_vertex)
	    && search_pred_.searchThru(edge)
	    && from_level >= level
	    // Loops with no entry edges are all level zero.
	    && !(from_level == 0 && level == 0))
	  report_->warn(617, "level check failed %s %d -> %s %d",
			from_vertex->name(network_),
			from_vertex->level(),
			vertex->name(network_),
			level);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

GraphLoop::GraphLoop(EdgeSeq *edges) :
  edges_(edges)
{
}

GraphLoop::~GraphLoop()
{
  delete edges_;
}

bool
GraphLoop::isCombinational() const
{
  EdgeSeq::Iterator edge_iter(edges_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    const TimingRole *role = edge->role();
    if (!(role == TimingRole::wire()
	  || role == TimingRole::combinational()
	  || role == TimingRole::tristateEnable()
	  || role == TimingRole::tristateDisable()))
      return false;
  }
  return true;
}

void
GraphLoop::report(const StaState *sta) const
{
  Graph *graph = sta->graph();
  Report *report = sta->report();
  bool first_edge = true;
  EdgeSeq::Iterator loop_edge_iter(edges_);
  while (loop_edge_iter.hasNext()) {
    Edge *edge = loop_edge_iter.next();
    if (first_edge)
      report->reportLine(" %s", edge->from(graph)->to_string(sta).c_str());
    report->reportLine(" %s", edge->to(graph)->to_string(graph).c_str());
    first_edge = false;
  }
}

} // namespace
