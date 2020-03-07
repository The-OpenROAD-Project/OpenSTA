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
#include "StaState.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "GraphClass.hh"

namespace sta {

class SearchPred;
class LevelizeObserver;

class Levelize : public StaState
{
public:
  explicit Levelize(StaState *sta);
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
  VertexSet &roots() { return roots_; }
  bool isRoot(Vertex *vertex);
  // Reset to virgin state.
  void clear();
  // Edge is disabled to break combinational loops.
  bool isDisabledLoop(Edge *edge) const;
  // Only valid when levels are valid.
  GraphLoopSeq *loops() { return loops_; }
  // Set the observer for level changes.
  void setObserver(LevelizeObserver *observer);

protected:
  void levelize();
  void findRoots();
  void sortRoots(VertexSeq &roots);
  void levelizeFrom(VertexSeq &roots);
  void visit(Vertex *vertex, Level level, Level level_space, EdgeSeq &path);
  void levelizeCycles();
  void relevelize();
  void clearLoopEdges();
  void deleteLoops();
  void recordLoop(Edge *edge, EdgeSeq &path);
  EdgeSeq *loopEdges(EdgeSeq &path, Edge *closing_edge);
  void ensureLatchLevels();
  void setLevel(Vertex  *vertex,
		Level level);

  SearchPred *search_pred_;
  bool levelized_;
  bool levels_valid_;
  Level max_level_;
  Level level_space_;
  VertexSet roots_;
  VertexSet relevelize_from_;
  GraphLoopSeq *loops_;
  EdgeSet loop_edges_;
  EdgeSet disabled_loop_edges_;
  EdgeSet latch_d_to_q_edges_;
  LevelizeObserver *observer_;

private:
  DISALLOW_COPY_AND_ASSIGN(Levelize);
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
  void report(Report *report,
	      Network *network,
	      Graph *graph) const;

private:
  DISALLOW_COPY_AND_ASSIGN(GraphLoop);

  EdgeSeq *edges_;
};

class LevelizeObserver
{
public:
  LevelizeObserver() {}
  virtual ~LevelizeObserver() {}
  virtual void levelChangedBefore(Vertex *vertex) = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(LevelizeObserver);
};

} // namespace
