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

#pragma once

#include <stack>

#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "GraphClass.hh"
#include "SearchPred.hh"
#include "StaState.hh"

namespace sta {

class SearchPred;
class LevelizeObserver;

typedef std::pair<Vertex*,VertexOutEdgeIterator*> VertexEdgeIterPair;
typedef std::stack<VertexEdgeIterPair> FindBackEdgesStack;

class Levelize : public StaState
{
public:
  Levelize(StaState *sta);
  virtual ~Levelize();
  // Space between initially assigned levels that is filled in by
  // incremental levelization.  Set level space before levelization.
  void setLevelSpace(Level space);
  bool levelized() { return levels_valid_; }
  void ensureLevelized();
  void invalid();
  // Levels downstream from vertex are invalid.
  void invalidFrom(Vertex *vertex);
  void relevelizeFrom(Vertex *vertex);
  void deleteVertexBefore(Vertex *vertex);
  void deleteEdgeBefore(Edge *edge);
  int maxLevel() const { return max_level_; }
  // Vertices with no fanin edges.
  VertexSet *roots() { return roots_; }
  bool isRoot(Vertex *vertex);
  bool hasFanout(Vertex *vertex);
  // Reset to virgin state.
  void clear();
  // Edge is disabled to break combinational loops.
  bool isDisabledLoop(Edge *edge) const;
  // Only valid when levels are valid.
  GraphLoopSeq &loops() { return loops_; }
  // Set the observer for level changes.
  void setObserver(LevelizeObserver *observer);

protected:
  void levelize();
  void findRoots();
  VertexSeq sortedRootsWithFanout();
  VertexSeq findTopologicalOrder();
  void assignLevels(VertexSeq &topo_sorted);
  void recordLoop(Edge *edge,
                  EdgeSeq &path);
  EdgeSeq *loopEdges(EdgeSeq &path,
                     Edge *closing_edge);
  void ensureLatchLevels();
  void findBackEdges();
  EdgeSet findBackEdges(EdgeSeq &path,
                        FindBackEdgesStack &stack);
  void findCycleBackEdges();
  VertexSeq findUnvisitedVertices();
  void relevelize();
  void visit(Vertex *vertex,
             Edge *from,
             Level level,
             Level level_space,
             VertexSet &visited,
             VertexSet &path_vertices,
             EdgeSeq &path);
  void setLevel(Vertex  *vertex,
		Level level);
  void setLevelIncr(Vertex  *vertex,
                    Level level);
  void clearLoopEdges();
  void deleteLoops();
  void reportPath(EdgeSeq &path) const;

  SearchPredNonLatch2 search_pred_;
  bool levelized_;
  bool levels_valid_;
  Level max_level_;
  Level level_space_;
  VertexSet *roots_;
  VertexSet *relevelize_from_;
  GraphLoopSeq loops_;
  EdgeSet loop_edges_;
  EdgeSet disabled_loop_edges_;
  EdgeSet latch_d_to_q_edges_;
  LevelizeObserver *observer_;
};

// Loops broken by levelization may not necessarily be combinational.
// For example, a register/latch output can feed back to a gated clock
// enable on the register/latch clock.
class GraphLoop
{
public:
  explicit GraphLoop(EdgeSeq *edges);
  ~GraphLoop();
  EdgeSeq *edges() { return edges_; }
  bool isCombinational() const;
  void report(const StaState *sta) const;

private:
  EdgeSeq *edges_;
};

class LevelizeObserver
{
public:
  LevelizeObserver() {}
  virtual ~LevelizeObserver() {}
  virtual void levelsChangedBefore() = 0;
  virtual void levelChangedBefore(Vertex *vertex) = 0;
};

} // namespace
