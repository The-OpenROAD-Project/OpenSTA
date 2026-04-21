// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "GraphClass.hh"
#include "Iterator.hh"
#include "StaState.hh"
#include "VertexVisitor.hh"

namespace sta {

class SearchPred;
class BfsFwdIterator;
class BfsBkwdIterator;

// LevelQueue is a vector of vertex vectors indexed by logic level.
using LevelQueue = std::vector<VertexSeq>;

// Abstract base class for forward and backward breadth first search iterators.
// Visit all of the vertices at a level before moving to the next.
// Use enqueue to seed the search.
// Use enqueueAdjacentVertices as a vertex is visited to queue the
// fanout vertices filtered by the search predicate.
//
// Vertices are marked as being in the queue by using a flag on
// the vertex indexed by bfs_index.  A unique flag is only needed
// if the BFS in in use when other BFS's are simultaneously in use.
class BfsIterator : public StaState,
                    public Iterator<Vertex*>
{
public:
  // Make sure that the BFS queue is deep enough for the max logic level.
  void ensureSize();
  // Reset to virgin state.
  void clear();
  [[nodiscard]] bool empty() const;
  // Enqueue a vertex to search from.
  void enqueue(Vertex *vertex);
  // Enqueue vertices adjacent to a vertex.
  void enqueueAdjacentVertices(Vertex *vertex);
  virtual void enqueueAdjacentVertices(Vertex *vertex,
                                       const Mode *mode);
  virtual void enqueueAdjacentVertices(Vertex *vertex,
				       SearchPred *search_pred,
                                       const Mode *mode) = 0;
  virtual void enqueueAdjacentVertices(Vertex *vertex,
                                       SearchPred *search_pred) = 0;
  [[nodiscard]] bool inQueue(Vertex *vertex);
  void checkInQueue(Vertex *vertex);
  // Notify iterator that vertex will be deleted.
  void deleteVertexBefore(Vertex *vertex);
  void remove(Vertex *vertex);
  void reportEntries() const;
  // Enable/disable Kahn's algorithm for parallel traversal.
  // When disabled (default), the original level-based BFS is used.
  // Kahn's requires a non-null kahn_pred to know which edges to
  // follow during discovery. Set it via setKahnPred() before enabling.
  void setUseKahns(bool use_kahns) { use_kahns_ = use_kahns; }
  bool useKahns() const { return use_kahns_; }
  // Search predicate used by Kahn's discovery and successor decrement.
  // Separate from search_pred_ which is used by the original BFS.
  void setKahnPred(SearchPred *pred) { kahn_pred_ = pred; }

  bool hasNext() override;
  bool hasNext(Level to_level);
  Vertex *next() override;

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
  virtual void incrLevel(Level &level) const = 0;
  void findNext(Level to_level);
  void deleteEntries();
  void checkLevel(Vertex *vertex,
                  Level level);

  // Kahn's algorithm: iterate BFS-direction successor vertices
  // with predicate filtering. Forward follows out-edges; backward
  // follows in-edges. Called for discovery, in-degree counting,
  // and successor decrement in the Kahn's worker.
  using VertexFn = std::function<void(Vertex*)>;
  virtual void kahnForEachSuccessor(Vertex *vertex,
                                    SearchPred *pred,
                                    const VertexFn &fn) = 0;
  void resetLevelBounds();

  // Persistent Kahn's state to avoid per-call allocation.
  struct KahnState;
  std::unique_ptr<KahnState> kahn_state_;

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
  bool use_kahns_ = true;
  SearchPred *kahn_pred_ = nullptr;

  friend class BfsFwdIterator;
  friend class BfsBkwdIterator;
};

class BfsFwdIterator : public BfsIterator
{
public:
  BfsFwdIterator(BfsIndex bfs_index,
		 SearchPred *search_pred,
		 StaState *sta);
  ~BfsFwdIterator() override;
  void enqueueAdjacentVertices(Vertex *vertex,
                              SearchPred *search_pred) override;
  void enqueueAdjacentVertices(Vertex *vertex,
			       SearchPred *search_pred,
                               const Mode *mode) override;
  using BfsIterator::enqueueAdjacentVertices;

protected:
  bool levelLessOrEqual(Level level1,
			Level level2) const override;
  bool levelLess(Level level1,
		 Level level2) const override;
  void incrLevel(Level &level) const override;
  void kahnForEachSuccessor(Vertex *vertex,
                            SearchPred *pred,
                            const VertexFn &fn) override;
};

class BfsBkwdIterator : public BfsIterator
{
public:
  BfsBkwdIterator(BfsIndex bfs_index,
		  SearchPred *search_pred,
		  StaState *sta);
  ~BfsBkwdIterator() override;
  void enqueueAdjacentVertices(Vertex *vertex,
                              SearchPred *search_pred) override;
  void enqueueAdjacentVertices(Vertex *vertex,
			       SearchPred *search_pred,
                               const Mode *mode) override;
  using BfsIterator::enqueueAdjacentVertices;

protected:
  bool levelLessOrEqual(Level level1,
			Level level2) const override;
  bool levelLess(Level level1,
		 Level level2) const override;
  void incrLevel(Level &level) const override;
  void kahnForEachSuccessor(Vertex *vertex,
                            SearchPred *pred,
                            const VertexFn &fn) override;
};

} // namespace sta
