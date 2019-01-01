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
#include "ThreadForEach.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Levelize.hh"
#include "Sdc.hh"
#include "SearchPred.hh"
#include "Bfs.hh"

namespace sta {

BfsList::BfsList(Vertex *vertex,
		 BfsList *next) :
  vertex_(vertex),
  next_(next)
{
}

void
BfsList::setVertex(Vertex *vertex)
{
  vertex_ = vertex;
}

void
BfsList::setNext(BfsList *next)
{
  next_ = next;
}

class BfsListIterator
{
public:
  explicit BfsListIterator(BfsList *list,
			   BfsIterator *bfs,
			   BfsIndex bfs_index);
  bool hasNext() { return next_ != NULL; }
  Vertex *next();
  int count() const { return count_; }

private:
  BfsIterator *bfs_;
  BfsIndex bfs_index_;
  BfsList *next_;
  int count_;
};

BfsListIterator::BfsListIterator(BfsList *list,
				 BfsIterator *bfs,
				 BfsIndex bfs_index) :
  bfs_(bfs),
  bfs_index_(bfs_index),
  next_(list),
  count_(0)
{
}

Vertex *
BfsListIterator::next()
{
  Vertex *vertex = next_->vertex();
  BfsList *next = next_->next();
  bfs_->freeList(next_);
  next_ = next;
  vertex->setBfsInQueue(bfs_index_, false);
  count_++;
  return vertex;
}

////////////////////////////////////////////////////////////////

BfsIterator::BfsIterator(BfsIndex bfs_index,
			 Level level_min,
			 Level level_max,
			 SearchPred *search_pred,
			 StaState *sta) :
  StaState(sta),
  bfs_index_(bfs_index),
  level_min_(level_min),
  level_max_(level_max),
  search_pred_(search_pred),
  list_free_(NULL)
{
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

BfsIterator::~BfsIterator()
{
  // Delete free list.
  while (list_free_) {
    BfsList *next = list_free_->next();
    delete list_free_;
    list_free_ = next;
  }
}

void
BfsIterator::clear()
{
  Level level = first_level_;
  while (levelLessOrEqual(level, last_level_)) {
    BfsList *level_vertices = queue_[level];
    if (level_vertices) {
      for (BfsList *l = level_vertices, *next; l; l = next) {
	Vertex *vertex = l->vertex();
	vertex->setBfsInQueue(bfs_index_, false);
	next = l->next();
	freeList(l);
      }
      queue_[level] = NULL;
    }
    incrLevel(level);
  }
  init();
}

void
BfsIterator::reportEntries(const Network *network)
{
  Level level = first_level_;
  while (levelLessOrEqual(level, last_level_)) {
    BfsList *level_vertices = queue_[level];
    if (level_vertices) {
      printf("Level %d\n", level);
      for (BfsList *l = level_vertices, *next; l; l = next) {
	Vertex *vertex = l->vertex();
	printf(" %s\n", vertex->name(network));
	next = l->next();
      }
    }
    incrLevel(level);
  }
}

void 
BfsIterator::deleteEntries(Level level)
{
  BfsList *level_vertices = queue_[level];
  if (level_vertices) {
    for (BfsList *l = level_vertices, *next; l; l = next) {
      Vertex *vertex = l->vertex();
      vertex->setBfsInQueue(bfs_index_, false);
      next = l->next();
      delete l;
    }
  }
}

bool
BfsIterator::empty() const
{
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
  while (levelLessOrEqual(first_level_, last_level_)
	 && levelLessOrEqual(first_level_, to_level)) {
    BfsList *level_vertices = queue_[first_level_];
    if (level_vertices) {
      // Remove vertices from queue.
      queue_[first_level_] = NULL;
      incrLevel(first_level_);
      for (BfsList *l = level_vertices, *next; l; l = next) {
	Vertex *vertex = l->vertex();
	vertex->setBfsInQueue(bfs_index_, false);
	visitor->visit(vertex);
	next = l->next();
	freeList(l);
	visit_count++;
      }
    }
    else
      incrLevel(first_level_);
  }
  return visit_count;
}

int
BfsIterator::visitParallel(Level to_level,
			   VertexVisitor *visitor)
{
  int visit_count = 0;
  if (!empty()) {
    int thread_count = threadCount();
    if (thread_count <= 1)
      visit_count = visit(to_level, visitor);
    else {
      ForEachArg<BfsListIterator, VertexVisitor> *args =
	new ForEachArg<BfsListIterator,VertexVisitor>[thread_count];
      Thread *threads = new Thread[thread_count];
      Mutex lock;
      for (int i = 0; i < thread_count; i++) {
	ForEachArg<BfsListIterator,VertexVisitor> *arg = &args[i];
	arg->lock_ = &lock;
	arg->func_ = visitor->copy();
      }

      Level level = first_level_;
      while (levelLessOrEqual(level, last_level_)
	     && levelLessOrEqual(level, to_level)) {
	BfsList *level_vertices = queue_[level];
	if (level_vertices) {
	  // Remove level vertices from queue.
	  queue_[level] = NULL;
	  incrLevel(first_level_);
	  BfsListIterator iter(level_vertices, this, bfs_index_);

	  for (int i = 0; i < thread_count; i++) {
	    ForEachArg<BfsListIterator,VertexVisitor> *arg = &args[i];
	    // Initialize the iterator for this level's vertices.
	    arg->iter_ = &iter;
	    threads[i].beginTask(forEachBegin<BfsListIterator,
				 VertexVisitor, Vertex*>,
				 reinterpret_cast<void*>(arg));
	  }

	  // Wait for all threads working on this level before moving on.
	  for (int i = 0; i < thread_count; i++)
	    threads[i].wait();

	  visit_count += iter.count();
	  level = first_level_;
	}
	else {
	  incrLevel(first_level_);
	  level = first_level_;
	}
      }

      for (int i = 0; i < thread_count; i++) {
	ForEachArg<BfsListIterator,VertexVisitor> *arg = &args[i];
	delete arg->func_;
      }
      delete [] threads;
      delete [] args;
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
    && queue_[first_level_] != NULL;
}

Vertex *
BfsIterator::next()
{
  BfsList *head = queue_[first_level_];
  Vertex *vertex = head->vertex();
  vertex->setBfsInQueue(bfs_index_, false);
  queue_[first_level_] = head->next();
  freeList(head);
  return vertex;
}

void
BfsIterator::findNext(Level to_level)
{
  while (levelLessOrEqual(first_level_, last_level_)
	 && levelLessOrEqual(first_level_, to_level)
	 && queue_[first_level_] == NULL)
    incrLevel(first_level_);
}

void
BfsIterator::enqueue(Vertex *vertex)
{
  debugPrint1(debug_, "bfs", 2, "enqueue %s\n", vertex->name(sdc_network_));
  Level level = vertex->level();
  if (!vertex->bfsInQueue(bfs_index_)) {
    queue_lock_.lock();
    if (!vertex->bfsInQueue(bfs_index_)) {
      vertex->setBfsInQueue(bfs_index_, true);
      queue_[level] = makeList(vertex, queue_[level]);

      if (levelLess(last_level_, level))
	last_level_ = level;
      if (levelLess(level, first_level_))
	first_level_ = level;
    }
    queue_lock_.unlock();
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
    for (BfsList *l = queue_[level]; l; l = l->next()) {
      if (l->vertex() == vertex) {
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

void
BfsIterator::remove(Vertex *vertex)
{
  // If the iterator has not been inited the queue will be empty.
  Level level = vertex->level();
  if (vertex->bfsInQueue(bfs_index_)
      && static_cast<Level>(queue_.size()) > level) {
    BfsList *next, *prev = NULL;
    for (BfsList *l = queue_[level]; l; l = next) {
      next = l->next();
      if (l->vertex() == vertex) {
        if (prev)
          prev->setNext(next);
        else
          queue_[level] = next;
	vertex->setBfsInQueue(bfs_index_, false);
        freeList(l);
        break;
      }
      prev = l;
    }
  }
}

BfsList *
BfsIterator::makeList(Vertex *vertex,
		      BfsList *next)
{
  BfsList *l;
  list_lock_.lock();
  if (list_free_) {
    l = list_free_;
    list_free_ = l->next();
    list_lock_.unlock();
    l->setVertex(vertex);
    l->setNext(next);
  }
  else {
    list_lock_.unlock();
    l = new BfsList(vertex, next);
  }
  return l;
}

void
BfsIterator::freeList(BfsList *l)
{
  list_lock_.lock();
  l->setNext(list_free_);
  list_free_ = l;
  list_lock_.unlock();
}

void
BfsIterator::deleteList(BfsList *list)
{
  while (list) {
    BfsList *next = list->next();
    list->vertex()->setBfsInQueue(bfs_index_, false);
    delete list;
    list = next;
  }
}

////////////////////////////////////////////////////////////////

BfsFwdIterator::BfsFwdIterator(BfsIndex bfs_index,
			       SearchPred *search_pred,
			       StaState *sta) :
  BfsIterator(bfs_index, 0, INT_MAX, search_pred, sta)
{
  init();
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

////////////////////////////////////////////////////////////////

BfsBkwdIterator::BfsBkwdIterator(BfsIndex bfs_index,
				 SearchPred *search_pred,
				 StaState *sta) :
  BfsIterator(bfs_index, INT_MAX, 0, search_pred, sta)
{
  init();
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

} // namespace
