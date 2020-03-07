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

#include <mutex>
#include "DisallowCopyAssign.hh"
#include "Iterator.hh"
#include "Set.hh"
#include "StaState.hh"
#include "GraphClass.hh"
#include "VertexVisitor.hh"

namespace sta {

class SearchPred;
class BfsFwdIterator;
class BfsBkwdIterator;

// LevelQueue is a vector of vertex vectors indexed by logic level.
typedef Vector<VertexSeq> LevelQueue;

// Abstract base class for forward and backward breadth first search iterators.
// Visit all of the vertices at a level before moving to the next.
// Use enqueue to seed the search.
// Use enqueueAdjacentVertices as a vertex is visited to queue the
// fanout vertices filtered by the search predicate.
//
// Vertices are marked as being in the queue by using a flag on
// the vertex indexed by bfs_index.  A unique flag is only needed
// if the BFS in in use when other BFS's are simultaneously in use.
class BfsIterator : public StaState, Iterator<Vertex*>
{
public:
  virtual ~BfsIterator();
  // Make sure that the BFS queue is deep enough for the max logic level.
  void ensureSize();
  // Reset to virgin state.
  void clear();
  bool empty() const;
  // Enqueue a vertex to search from.
  void enqueue(Vertex *vertex);
  // Enqueue vertices adjacent to a vertex.
  void enqueueAdjacentVertices(Vertex *vertex);
  void enqueueAdjacentVertices(Vertex *vertex,
			       SearchPred *search_pred);
  void enqueueAdjacentVertices(Vertex *vertex,
			       Level to_level);
  virtual void enqueueAdjacentVertices(Vertex *vertex,
				       SearchPred *search_pred,
				       Level to_level) = 0;
  bool inQueue(Vertex *vertex);
  void checkInQueue(Vertex *vertex);
  // Notify iterator that vertex will be deleted.
  void deleteVertexBefore(Vertex *vertex);
  void remove(Vertex *vertex);
  void reportEntries(const Network *network);

  virtual bool hasNext();
  bool hasNext(Level to_level);
  virtual Vertex *next();

  // Apply visitor to all vertices in the queue in level order.
  // Returns the number of vertices that are visited.
  virtual int visit(Level to_level,
		    VertexVisitor *visitor);
  // Apply visitor to all vertices in the queue in level order,
  // using threads to parallelize the visits. visitor must be thread safe.
  // Returns the number of vertices that are visited.
  int visitParallel(Level to_level,
		    VertexVisitor *visitor);

protected:
  BfsIterator(BfsIndex bfs_index,
	      Level level_min,
	      Level level_max,
	      SearchPred *search_pred,
	      StaState *sta);
  void init();
  void deleteEntries(Level level);
  virtual bool levelLess(Level level1,
			 Level level2) const = 0;
  virtual bool levelLessOrEqual(Level level1,
				Level level2) const = 0;
  virtual void incrLevel(Level &level) = 0;
  void findNext(Level to_level);
  void deleteEntries();

  BfsIndex bfs_index_;
  Level level_min_;
  Level level_max_;
  SearchPred *search_pred_;
  LevelQueue queue_;
  std::mutex queue_lock_;
  // Min (max) level of queued vertices.
  Level first_level_;
  // Max (min) level of queued vertices.
  Level last_level_;

  friend class BfsFwdIterator;
  friend class BfsBkwdIterator;

private:
  DISALLOW_COPY_AND_ASSIGN(BfsIterator);
};

class BfsFwdIterator : public BfsIterator
{
public:
  BfsFwdIterator(BfsIndex bfs_index,
		 SearchPred *search_pred,
		 StaState *sta);
  virtual ~BfsFwdIterator();
  virtual void enqueueAdjacentVertices(Vertex *vertex,
				       SearchPred *search_pred,
				       Level to_level);
  using BfsIterator::enqueueAdjacentVertices;

protected:
  virtual bool levelLessOrEqual(Level level1,
				Level level2) const;
  virtual bool levelLess(Level level1,
			 Level level2) const;
  virtual void incrLevel(Level &level);

private:
  DISALLOW_COPY_AND_ASSIGN(BfsFwdIterator);
};

class BfsBkwdIterator : public BfsIterator
{
public:
  BfsBkwdIterator(BfsIndex bfs_index,
		  SearchPred *search_pred,
		  StaState *sta);
  virtual ~BfsBkwdIterator();
  virtual void enqueueAdjacentVertices(Vertex *vertex,
				       SearchPred *search_pred,
				       Level to_level);
  using BfsIterator::enqueueAdjacentVertices;

protected:
  virtual bool levelLessOrEqual(Level level1,
				Level level2) const;
  virtual bool levelLess(Level level1,
			 Level level2) const;
  virtual void incrLevel(Level &level);

private:
  DISALLOW_COPY_AND_ASSIGN(BfsBkwdIterator);
};

} // namespace
