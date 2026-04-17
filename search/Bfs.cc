
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

#include "Bfs.hh"
#include <set>
#include <iostream>

#include "Report.hh"
#include "Debug.hh"
#include "Mutex.hh"
#include "DispatchQueue.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Levelize.hh"
#include "SearchPred.hh"

namespace sta {

BfsIterator::BfsIterator(BfsIndex bfs_index,
                         Level level_min,
                         Level level_max,
                         SearchPred *search_pred,
                         StaState *sta) :
  StaState(sta),
  bfs_index_(bfs_index),
  level_min_(level_min),
  level_max_(level_max),
  search_pred_(search_pred)
{
  init();
}

void
BfsIterator::init()
{
  first_level_ = level_max_;
  last_level_ = level_min_;
  ensureSize();
}

void
BfsIterator::ensureSize()
{
  if (levelize_->levelized()) {
    unsigned max_level_1 = levelize_->maxLevel() + 1;
    if (queue_.size() < max_level_1)
      queue_.resize(max_level_1);
  }
}

BfsIterator::~BfsIterator() {}

void
BfsIterator::clear()
{
  Level level = first_level_;
  while (levelLessOrEqual(level, last_level_)) {
    VertexSeq &level_vertices = queue_[level];
    for (Vertex *vertex : level_vertices) {
      if (vertex)
        vertex->setBfsInQueue(bfs_index_, false);
    }
    level_vertices.clear();
    incrLevel(level);
  }
  init();
}

void
BfsIterator::reportEntries() const
{
  for (Level level = first_level_; levelLessOrEqual(level, last_level_);
       incrLevel(level)) {
    const VertexSeq &level_vertices = queue_[level];
    if (!level_vertices.empty()) {
      report_->report("Level {}", level);
      for (Vertex *vertex : level_vertices)
        report_->report(" {}", vertex ? vertex->to_string(this) : "NULL");
    }
  }
}

void
BfsIterator::deleteEntries(Level level)
{
  VertexSeq &level_vertices = queue_[level];
  for (Vertex *vertex : level_vertices) {
    if (vertex)
      vertex->setBfsInQueue(bfs_index_, false);
  }
  level_vertices.clear();
}

bool
BfsIterator::empty() const
{
  return levelLess(last_level_, first_level_);
}

void
BfsIterator::enqueueAdjacentVertices(Vertex *vertex)
{
  enqueueAdjacentVertices(vertex, search_pred_);
}

void
BfsIterator::enqueueAdjacentVertices(Vertex *vertex,
                                     const Mode *mode)
{
  enqueueAdjacentVertices(vertex, search_pred_, mode);
}

int
BfsIterator::visit(Level to_level,
                   VertexVisitor *visitor)
{
  int visit_count = 0;
  while (levelLessOrEqual(first_level_, last_level_)
         && levelLessOrEqual(first_level_, to_level)) {
    Level level = first_level_;
    VertexSeq &level_vertices = queue_[level];
    incrLevel(first_level_);
    // Note that ArrivalVisitor::enqueueRefPinInputDelays may enqueue
    // vertices at this level so range iteration fails if the vector grows.
    while (!level_vertices.empty()) {
      Vertex *vertex = level_vertices.back();
      level_vertices.pop_back();
      if (vertex) {
        checkLevel(vertex, level);
        vertex->setBfsInQueue(bfs_index_, false);
        visitor->visit(vertex);
        visit_count++;
      }
    }
    level_vertices.clear();
    visitor->levelFinished();
  }
  return visit_count;
}

int
BfsIterator::visitParallel(Level to_level,
                           VertexVisitor *visitor)
{
  size_t thread_count = thread_count_;
  int visit_count = 0;
  if (!empty()) {
    if (thread_count == 1)
      visit_count = visit(to_level, visitor);
    else {
      std::vector<VertexVisitor *> visitors;
      for (int k = 0; k < thread_count_; k++)
        visitors.push_back(visitor->copy());
      while (levelLessOrEqual(first_level_, last_level_)
             && levelLessOrEqual(first_level_, to_level)) {
        VertexSeq &level_vertices = queue_[first_level_];
        Level level = first_level_;
        incrLevel(first_level_);
        if (!level_vertices.empty()) {
          size_t vertex_count = level_vertices.size();
          if (vertex_count < thread_count) {
            for (Vertex *vertex : level_vertices) {
              if (vertex) {
                checkLevel(vertex, level);
                vertex->setBfsInQueue(bfs_index_, false);
                visitor->visit(vertex);
              }
            }
          }
          else {
            size_t from = 0;
            size_t chunk_size = vertex_count / thread_count;
            BfsIndex bfs_index = bfs_index_;
            for (size_t k = 0; k < thread_count; k++) {
              // Last thread gets the left overs.
              size_t to = (k == thread_count - 1) ? vertex_count : from + chunk_size;
              dispatch_queue_->dispatch([=, this](size_t) {
                for (size_t i = from; i < to; i++) {
                  Vertex *vertex = level_vertices[i];
                  if (vertex) {
                    checkLevel(vertex, level);
                    vertex->setBfsInQueue(bfs_index, false);
                    visitors[k]->visit(vertex);
                  }
                }
              });
              from = to;
            }
            dispatch_queue_->finishTasks();
          }
          visitor->levelFinished();
          level_vertices.clear();
          visit_count += vertex_count;
        }
      }
      for (VertexVisitor *visitor : visitors)
        delete visitor;
    }
  }
  return visit_count;
}

bool
BfsIterator::hasNext()
{
  return hasNext(last_level_);
}

bool
BfsIterator::hasNext(Level to_level)
{
  findNext(to_level);
  return levelLessOrEqual(first_level_, last_level_)
      && !queue_[first_level_].empty();
}

Vertex *
BfsIterator::next()
{
  VertexSeq &level_vertices = queue_[first_level_];
  Vertex *vertex = level_vertices.back();
  level_vertices.pop_back();
  vertex->setBfsInQueue(bfs_index_, false);
  return vertex;
}

void
BfsIterator::findNext(Level to_level)
{
  while (levelLessOrEqual(first_level_, last_level_)
         && levelLessOrEqual(first_level_, to_level)) {
    VertexSeq &level_vertices = queue_[first_level_];
    // Skip null entries from deleted vertices.
    while (!level_vertices.empty()) {
      Vertex *vertex = level_vertices.back();
      if (vertex == nullptr)
        level_vertices.pop_back();
      else {
        checkLevel(vertex, first_level_);
        return;
      }
    }
    incrLevel(first_level_);
  }
}

void
BfsIterator::enqueue(Vertex *vertex)
{
  debugPrint(debug_, "bfs", 2, "enqueue {}", vertex->to_string(this));
  if (!vertex->bfsInQueue(bfs_index_)) {
    Level level = vertex->level();
    LockGuard lock(queue_lock_);
    if (!vertex->bfsInQueue(bfs_index_)) {
      vertex->setBfsInQueue(bfs_index_, true);
      queue_[level].push_back(vertex);

      if (levelLess(last_level_, level))
        last_level_ = level;
      if (levelLess(level, first_level_))
        first_level_ = level;
    }
  }
}

bool
BfsIterator::inQueue(Vertex *vertex)
{
  // checkInQueue(vertex);
  return vertex->bfsInQueue(bfs_index_);
}

void
BfsIterator::checkInQueue(Vertex *vertex)
{
  Level level = vertex->level();
  if (static_cast<Level>(queue_.size()) > level) {
    for (Vertex *v : queue_[level]) {
      if (v == vertex) {
        if (vertex->bfsInQueue(bfs_index_))
          return;
        else
          debugPrint(debug_, "bfs", 1, "extra {}", vertex->to_string(this));
      }
    }
  }
  if (vertex->bfsInQueue(bfs_index_))
    debugPrint(debug_, "brs", 1, "missing {}", vertex->to_string(this));
}

void
BfsIterator::checkLevel(Vertex *vertex,
                        Level level)
{
  if (vertex->level() != level)
    report_->error(2300, "vertex {} level {} != bfs level {}",
                   vertex->to_string(this), vertex->level(), level);
}

void
BfsIterator::deleteVertexBefore(Vertex *vertex)
{
  remove(vertex);
}

// Remove by inserting null vertex pointer.
void
BfsIterator::remove(Vertex *vertex)
{
  // If the iterator has not been inited the queue will be empty.
  Level level = vertex->level();
  if (vertex->bfsInQueue(bfs_index_) && static_cast<Level>(queue_.size()) > level) {
    debugPrint(debug_, "bfs", 2, "remove {}", vertex->to_string(this));
    for (Vertex *&v : queue_[level]) {
      if (v == vertex) {
        v = nullptr;
        vertex->setBfsInQueue(bfs_index_, false);
        break;
      }
    }
  }
}

////////////////////////////////////////////////////////////////

BfsFwdIterator::BfsFwdIterator(BfsIndex bfs_index,
                               SearchPred *search_pred,
                               StaState *sta) :
  BfsIterator(bfs_index,
              0,
              level_max,
              search_pred,
              sta)
{
}

// clear() without saving lists to list_free_.
BfsFwdIterator::~BfsFwdIterator()
{
  for (Level level = first_level_; level <= last_level_; level++)
    deleteEntries(level);
}

void
BfsFwdIterator::incrLevel(Level &level) const
{
  level++;
}

bool
BfsFwdIterator::levelLessOrEqual(Level level1,
                                 Level level2) const
{
  return level1 <= level2;
}

bool
BfsFwdIterator::levelLess(Level level1,
                          Level level2) const
{
  return level1 < level2;
}

void
BfsFwdIterator::enqueueAdjacentVertices(Vertex *vertex,
                                        SearchPred *search_pred)
{
  if (search_pred->searchFrom(vertex)) {
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (search_pred->searchThru(edge) && search_pred->searchTo(to_vertex))
        enqueue(to_vertex);
    }
  }
}

void
BfsFwdIterator::enqueueAdjacentVertices(Vertex *vertex,
                                        SearchPred *search_pred,
                                        const Mode *mode)
{
  if (search_pred->searchFrom(vertex, mode)) {
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (search_pred->searchThru(edge, mode)
          && search_pred->searchTo(to_vertex, mode))
        enqueue(to_vertex);
    }
  }
}

////////////////////////////////////////////////////////////////

BfsBkwdIterator::BfsBkwdIterator(BfsIndex bfs_index,
                                 SearchPred *search_pred,
                                 StaState *sta) :
  BfsIterator(bfs_index,
              level_max,
              0,
              search_pred,
              sta)
{
}

// clear() without saving lists to list_free_.
BfsBkwdIterator::~BfsBkwdIterator()
{
  for (Level level = first_level_; level >= last_level_; level--)
    deleteEntries(level);
}

void
BfsBkwdIterator::incrLevel(Level &level) const
{
  level--;
}

bool
BfsBkwdIterator::levelLessOrEqual(Level level1,
                                  Level level2) const
{
  return level1 >= level2;
}

bool
BfsBkwdIterator::levelLess(Level level1,
                           Level level2) const
{
  return level1 > level2;
}

void
BfsBkwdIterator::enqueueAdjacentVertices(Vertex *vertex,
                                         SearchPred *search_pred)
{
  if (search_pred->searchTo(vertex)) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph_);
      if (search_pred->searchFrom(from_vertex) && search_pred->searchThru(edge))
        enqueue(from_vertex);
    }
  }
}

void
BfsBkwdIterator::enqueueAdjacentVertices(Vertex *vertex,
                                         SearchPred *search_pred,
                                         const Mode *mode)
{
  if (search_pred->searchTo(vertex, mode)) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph_);
      if (search_pred->searchFrom(from_vertex, mode)
          && search_pred->searchThru(edge, mode))
        enqueue(from_vertex);
    }
  }
}

thread_local int current_thread_id = 0;

BfsFwdInDegreeIterator::BfsFwdInDegreeIterator(BfsIndex bfs_index,
                                               SearchPred *search_pred,
                                               StaState *sta) :
  StaState(sta),
  bfs_index_(bfs_index),
  search_pred_(search_pred)
{
}

BfsFwdInDegreeIterator::~BfsFwdInDegreeIterator()
{
}

void BfsFwdInDegreeIterator::clear()
{
  in_degrees_.reset();
  in_degrees_size_ = 0;
  roots_.clear();
}

void BfsFwdInDegreeIterator::computeInDegrees()
{
  size_t vertex_count = graph_->vertexCount();
  in_degrees_ = std::make_unique<std::atomic<int>[]>(vertex_count + 1);
  in_degrees_size_ = vertex_count + 1;
  for (size_t i = 0; i < in_degrees_size_; i++) {
    in_degrees_[i].store(0, std::memory_order_relaxed);
  }
  roots_.clear();
  processed_edges_.clear();

  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    vertex->setVisited(false);
    std::set<Vertex*> counted_successors;
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (search_pred_->searchThru(edge)) {
        if (counted_successors.insert(to_vertex).second) {
          in_degrees_[to_vertex->objectIdx()].fetch_add(1, std::memory_order_relaxed);
        }
      }
    }
  }



  VertexIterator vertex_iter2(graph_);
  while (vertex_iter2.hasNext()) {
    Vertex *vertex = vertex_iter2.next();
    if (search_pred_->searchFrom(vertex)) {
      if (in_degrees_[vertex->objectIdx()].load(std::memory_order_relaxed) == 0) {
        roots_.push_back(vertex);
      }
    }
  }
}

void BfsFwdInDegreeIterator::computeInDegrees(const VertexSet &invalid_delays)
{
  // For incremental, we do a reachability pass to find the affected subgraph.
  // Then we compute in-degrees within that subgraph.
  
  // 1. Find reachable subgraph from invalid_delays.
  std::set<Vertex*> reachable;
  std::vector<Vertex*> work_list;
  for (Vertex *v : invalid_delays) {
    work_list.push_back(v);
    reachable.insert(v);
  }
  
  size_t idx = 0;
  while (idx < work_list.size()) {
    Vertex *v = work_list[idx++];
    VertexOutEdgeIterator edge_iter(v, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (search_pred_->searchThru(edge)) {
        if (reachable.insert(to_vertex).second) {
          work_list.push_back(to_vertex);
        }
      }
    }
  }
  
  // 2. Compute in-degrees within the reachable subgraph.
  size_t vertex_count = graph_->vertexCount();
  in_degrees_ = std::make_unique<std::atomic<int>[]>(vertex_count + 1);
  in_degrees_size_ = vertex_count + 1;
  for (size_t i = 0; i < in_degrees_size_; i++) {
    in_degrees_[i].store(0, std::memory_order_relaxed);
  }
  roots_.clear();
  
  for (Vertex *v : reachable) {
    VertexOutEdgeIterator edge_iter(v, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (search_pred_->searchThru(edge)) {
        if (reachable.count(to_vertex)) {
          in_degrees_[to_vertex->objectIdx()].fetch_add(1, std::memory_order_relaxed);
        }
      }
    }
  }
  
  // 3. Find roots within the reachable subgraph.
  for (Vertex *v : reachable) {
    if (in_degrees_[v->objectIdx()].load(std::memory_order_relaxed) == 0) {
      roots_.push_back(v);
    }
  }
}

void BfsFwdInDegreeIterator::enqueue(Vertex *vertex)
{
  visitors_[current_thread_id]->visit(vertex);
  visit_count_->fetch_add(1, std::memory_order_relaxed);
  enqueueAdjacentVertices(vertex);
}

void BfsFwdInDegreeIterator::enqueueAdjacentVertices(Vertex *vertex)
{
  VertexOutEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *to_vertex = edge->to(graph_);
    if (search_pred_->searchThru(edge)) {
      if (!to_vertex->visited()) {
        bool inserted = false;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          inserted = processed_edges_.insert(edge).second;
        }
        if (inserted) {
          int old_deg = in_degrees_[to_vertex->objectIdx()].fetch_sub(1, std::memory_order_acq_rel);
          if (old_deg == 1) {
            to_vertex->setVisited(true);
            if (dispatch_queue_) {
          dispatch_queue_->dispatch([this, to_vertex](size_t tid) {
            current_thread_id = tid;
            visitors_[tid]->visit(to_vertex);
            visit_count_->fetch_add(1, std::memory_order_relaxed);
            enqueueAdjacentVertices(to_vertex);
          });
        } else {
          current_thread_id = 0;
          visitors_[0]->visit(to_vertex);
          visit_count_->fetch_add(1, std::memory_order_relaxed);
          enqueueAdjacentVertices(to_vertex);
        }
      }
      }
      }
    }
  }
}

int BfsFwdInDegreeIterator::visitParallel(Level to_level, VertexVisitor *visitor)
{
  size_t thread_count = dispatch_queue_ ? dispatch_queue_->getThreadCount() : 1;
  visitors_.clear();
  if (dispatch_queue_) {
    for (size_t k = 0; k < thread_count; k++)
      visitors_.push_back(visitor->copy());
  } else {
    visitors_.push_back(visitor);
  }

  std::atomic<int> visit_count(0);
  visit_count_ = &visit_count;

  for (Vertex *root : roots_) {
    if (dispatch_queue_) {
      dispatch_queue_->dispatch([this, root](size_t tid) {
        current_thread_id = tid;
        visitors_[tid]->visit(root);
        visit_count_->fetch_add(1, std::memory_order_relaxed);
        enqueueAdjacentVertices(root);
      });
    } else {
      current_thread_id = 0;
      visitors_[0]->visit(root);
      visit_count_->fetch_add(1, std::memory_order_relaxed);
      enqueueAdjacentVertices(root);
    }
  }

  if (dispatch_queue_)
    dispatch_queue_->finishTasks();

  if (dispatch_queue_) {
    for (VertexVisitor *v : visitors_)
      delete v;
  }
  visitors_.clear();

  return visit_count.load(std::memory_order_relaxed);
}

}  // namespace sta
