// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include <limits.h>
#include "Machine.hh"
#include "Report.hh"
#include "Debug.hh"
#include "Mutex.hh"
#include "DispatchQueue.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Levelize.hh"
#include "Sdc.hh"
#include "SearchPred.hh"
#include "Bfs.hh"

#include "galois/Galois.h"
#include "galois/substrate/PerThreadStorage.h"

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
  history_max_level_1_= 0;
  per_thread_min_level_.reset();
  per_thread_max_level_.reset();
  ensureSize();
}

void
BfsIterator::ensureSize()
{
  if (levelize_->levelized()) {
    int max_level_1 = levelize_->maxLevel() + 1;
    if (queue_.size() < max_level_1)
      queue_.resize(max_level_1);
    // allocate worklist at corresponding levels
    if (history_max_level_1_ < max_level_1) {
      for (auto i = history_max_level_1_; i < max_level_1; i++) {
        queue_[i] = new galois::InsertBag<Vertex*>;
      }
    }
  }
}

BfsIterator::~BfsIterator()
{
}

void
BfsIterator::clear()
{
  Level level = first_level_;
  while (levelLessOrEqual(level, last_level_)) {
    auto& level_vertices = *queue_[level];
    galois::do_all(
        galois::iterate(level_vertices),
        [&] (Vertex* vertex) {
          if (vertex)
	    vertex->setBfsInQueue(bfs_index_, false);
        }
    );
    level_vertices.clear();
    incrLevel(level);
  }
  init();
}

void
BfsIterator::reportEntries(const Network *network)
{
  Level level = first_level_;
  while (levelLessOrEqual(level, last_level_)) {
    auto& level_vertices = *queue_[level];
    if (!level_vertices.empty()) {
      printf("Level %d\n", level);
      for (auto vertex : level_vertices) {
	if (vertex)
	  printf(" %s\n", vertex->name(network));
      }
    }
    incrLevel(level);
  }
}

void 
BfsIterator::deleteEntries(Level level)
{
  auto& level_vertices = *queue_[level];
  galois::do_all(
      galois::iterate(level_vertices),
      [&] (Vertex* vertex) {
        if (vertex)
          vertex->setBfsInQueue(bfs_index_, false);
      }
  );
  level_vertices.clear();
}

bool
BfsIterator::empty() const
{
  // need to call updateLevelBoundaries() before this
  return levelLess(last_level_, first_level_);
}

void
BfsIterator::enqueueAdjacentVertices(Vertex *vertex)
{
  enqueueAdjacentVertices(vertex, search_pred_, level_max_);
}

void
BfsIterator::enqueueAdjacentVertices(Vertex *vertex,
				     SearchPred *search_pred)
{
  enqueueAdjacentVertices(vertex, search_pred, level_max_);
}

void
BfsIterator::enqueueAdjacentVertices(Vertex *vertex,
				     Level to_level)
{
  enqueueAdjacentVertices(vertex, search_pred_, to_level);
}

int
BfsIterator::visit(Level to_level,
		   VertexVisitor *visitor)
{
  int visit_count = 0;
  updateLevelBoundaries();
  while (levelLessOrEqual(first_level_, last_level_)
	 && levelLessOrEqual(first_level_, to_level)) {
    auto& level_vertices = *queue_[first_level_];
    incrLevel(first_level_);
    if (!level_vertices.empty()) {
      for (auto vertex : level_vertices) {
	if (vertex) {
	  vertex->setBfsInQueue(bfs_index_, false);
	  visitor->visit(vertex);
	  visit_count++;
	}
      }
      level_vertices.clear();
      levelFinished(visitor);
    }
  }
  return visit_count;
}

int
BfsIterator::visitParallel(Level to_level,
			   VertexVisitor *visitor)
{
  int visit_count = 0;
  updateLevelBoundaries();

  if (!empty()) {
    galois::setActiveThreads(threadCount());

    if (galois::getActiveThreads() <= 1)
      visit_count = visit(to_level, visitor);

    else {
      galois::substrate::PerThreadStorage<VertexVisitor*> visitors;
      for (int i = 0; i < galois::getActiveThreads(); i++)
	*(visitors.getRemote(i)) = visitor->copy();

      galois::GAccumulator<int> visit_count_local;
      visit_count_local.reset();

      while (levelLessOrEqual(first_level_, last_level_)
	     && levelLessOrEqual(first_level_, to_level)) {
	auto& level_vertices = *queue_[first_level_];
	incrLevel(first_level_);
	if (!level_vertices.empty()) {
          galois::do_all(
              galois::iterate(level_vertices),
              [&] (Vertex* vertex) {
	        if (vertex) {
	          vertex->setBfsInQueue(bfs_index_, false);
                  (*(visitors.getLocal()))->visit(vertex);
                  visit_count_local += 1;
                }
              }
              , galois::steal()
              , galois::loopname("visitParallel")
	  );
          levelFinished(visitor);
	  level_vertices.clear();
	}
      }
      visit_count = visit_count_local.reduce();
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
     && !queue_[first_level_]->empty();
}

Vertex *
BfsIterator::next()
{
  auto& level_vertices = *queue_[first_level_];
  Vertex *vertex = *(level_vertices.begin());
  vertex->setBfsInQueue(bfs_index_, false);
  remove(vertex);

  galois::InsertBag<Vertex*> bag;
  galois::do_all(
      galois::iterate(level_vertices),
      [&] (Vertex* vertex) {
        if (vertex)
          bag.push_back(vertex);
      }
  );

  level_vertices.clear();
  galois::do_all(
      galois::iterate(bag),
      [&] (Vertex* vertex) {
        level_vertices.push(vertex);
      }
  );
  bag.clear();

  return vertex;
}

void
BfsIterator::findNext(Level to_level)
{
  while (levelLessOrEqual(first_level_, last_level_)
	 && levelLessOrEqual(first_level_, to_level)
	 && queue_[first_level_]->empty())
    incrLevel(first_level_);
}

void
BfsIterator::enqueue(Vertex *vertex)
{
  debugPrint1(debug_, "bfs", 2, "enqueue %s\n", vertex->name(sdc_network_));
  if (!vertex->bfsInQueue(bfs_index_)) {
    if (vertex->setBfsInQueue(bfs_index_, true)) {
      Level level = vertex->level();
      queue_[level]->push_back(vertex);
      per_thread_min_level_.update(level);
      per_thread_max_level_.update(level);
    }
  }
}

bool
BfsIterator::inQueue(Vertex *vertex)
{
  //  checkInQueue(vertex);
  return vertex->bfsInQueue(bfs_index_);
}

void
BfsIterator::checkInQueue(Vertex *vertex)
{
  Level level = vertex->level();
  if (static_cast<Level>(queue_.size()) > level) {
    for (auto v : *queue_[level]) {
      if (v == vertex) {
	if (vertex->bfsInQueue(bfs_index_))
	  return;
	else
	  printf("extra %s\n", vertex->name(sdc_network_));
      }
    }
  }
  if (vertex->bfsInQueue(bfs_index_))
    printf("missing %s\n", vertex->name(sdc_network_));
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
  if (vertex->bfsInQueue(bfs_index_)
      && static_cast<Level>(queue_.size()) > level) {
    for (auto &v : *queue_[level]) {
      if (v == vertex) {
	v = nullptr;
	vertex->setBfsInQueue(bfs_index_, false);
        break;
      }
    }
  }
}

void
BfsIterator::levelFinished(VertexVisitor* visitor)
{
  if (visitor)
    visitor->levelFinished();
  updateLevelBoundaries();
}

////////////////////////////////////////////////////////////////

BfsFwdIterator::BfsFwdIterator(BfsIndex bfs_index,
			       SearchPred *search_pred,
			       StaState *sta) :
  BfsIterator(bfs_index, 0, INT_MAX, search_pred, sta)
{
}

// clear() without saving lists to list_free_.
BfsFwdIterator::~BfsFwdIterator()
{
  for (Level level = first_level_; level <= last_level_; level++)
    deleteEntries(level);
}

void
BfsFwdIterator::incrLevel(Level &level)
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
					SearchPred *search_pred,
					Level to_level)
{
  if (search_pred->searchFrom(vertex)) {
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (to_vertex->level() <= to_level
	  && search_pred->searchThru(edge)
	  && search_pred->searchTo(to_vertex))
	enqueue(to_vertex);
    }
  }
}

void
BfsFwdIterator::updateLevelBoundaries() {
  auto minLv = per_thread_min_level_.reduce();
  if (levelLess(minLv, first_level_))
    first_level_ = minLv;
  per_thread_min_level_.reset();

  auto maxLv = per_thread_max_level_.reduce();
  if (levelLess(last_level_, maxLv))
    last_level_ = maxLv;
  per_thread_max_level_.reset();
}

////////////////////////////////////////////////////////////////

BfsBkwdIterator::BfsBkwdIterator(BfsIndex bfs_index,
				 SearchPred *search_pred,
				 StaState *sta) :
  BfsIterator(bfs_index, INT_MAX, 0, search_pred, sta)
{
}

// clear() without saving lists to list_free_.
BfsBkwdIterator::~BfsBkwdIterator()
{
  for (Level level = first_level_; level >= last_level_; level--)
    deleteEntries(level);
}

void
BfsBkwdIterator::incrLevel(Level &level)
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
					 SearchPred *search_pred,
					 Level to_level)
{
  if (search_pred->searchTo(vertex)) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph_);
      if (from_vertex->level() >= to_level
	  && search_pred->searchFrom(from_vertex)
	  && search_pred->searchThru(edge))
	enqueue(from_vertex);
    }
  }
}

void
BfsBkwdIterator::updateLevelBoundaries() {
  auto maxLv = per_thread_max_level_.reduce();
  if (levelLess(maxLv, first_level_))
    first_level_ = maxLv;
  per_thread_max_level_.reset();

  auto minLv = per_thread_min_level_.reduce();
  if (levelLess(last_level_, minLv))
    last_level_ = minLv;
  per_thread_min_level_.reset();
}

} // namespace
