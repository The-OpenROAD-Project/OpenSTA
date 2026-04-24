
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

#include <atomic>

#include "Debug.hh"
#include "DispatchQueue.hh"
#include "Graph.hh"
#include "Levelize.hh"
#include "Mutex.hh"
#include "Network.hh"
#include "Report.hh"
#include "Sdc.hh"
#include "SearchPred.hh"
#include "Variables.hh"

namespace sta {

// Persistent storage for Kahn's algorithm arrays.
// Allocated once and reused across visitParallel calls to
// avoid repeated allocation of large per-graph arrays.
//
// Thread-safety: active_vertices, seeds, and local_ready are written
// only during the single-threaded discovery/seeding phase at the top
// of the Kahn's branch. Worker tasks never touch them. Keep it that
// way -- making these persistent would otherwise be an invariant
// hazard.
struct BfsIterator::KahnState
{
  // -1 = not in active set, >= 0 = in-degree.
  std::vector<int> in_degree_init;
  // Atomic in-degrees for the parallel phase.
  std::unique_ptr<std::atomic<int>[]> in_degree;
  size_t in_degree_size = 0;
  // Vertex IDs touched in the previous call -- reset to -1 before reuse.
  std::vector<VertexId> prev_ids;
  // Discovered active set and zero-in-degree seed roots. Cleared at
  // the start of each call; capacity is retained so incremental
  // flows with many small calls avoid repeated allocation.
  std::vector<Vertex *> active_vertices;
  std::vector<Vertex *> seeds;
  // Per-worker local ready batches. Outer size is kept in sync with
  // thread_count at the call site; each inner vector is cleared per
  // call and its backing store is retained.
  std::vector<std::vector<Vertex *>> local_ready;
  // Atomic "any active-set predecessor reported a change" flag per
  // vertex. Only allocated/used when sta_kahn_visit_skip is on.
  std::unique_ptr<std::atomic<uint8_t>[]> pred_changed;
  size_t pred_changed_size = 0;

  void ensureInitSize(size_t needed)
  {
    if (in_degree_init.size() < needed)
      in_degree_init.resize(needed, -1);
  }

  void ensureAtomicSize(size_t needed)
  {
    if (in_degree_size < needed) {
      in_degree = std::make_unique<std::atomic<int>[]>(needed);
      in_degree_size = needed;
    }
  }

  void ensurePredChangedSize(size_t needed)
  {
    if (pred_changed_size < needed) {
      pred_changed = std::make_unique<std::atomic<uint8_t>[]>(needed);
      pred_changed_size = needed;
    }
  }

  void resetPrevious()
  {
    for (VertexId vid : prev_ids)
      in_degree_init[vid] = -1;
    prev_ids.clear();
  }
};

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
  }
  return visit_count;
}

// Recalculate first_level_/last_level_ from remaining queue entries.
void
BfsIterator::resetLevelBounds()
{
  first_level_ = level_max_;
  last_level_ = level_min_;
  for (Level l = 0; l < static_cast<Level>(queue_.size()); l++) {
    if (!queue_[l].empty()) {
      if (levelLess(l, first_level_))
        first_level_ = l;
      if (levelLess(last_level_, l))
        last_level_ = l;
    }
  }
}

void
BfsIterator::dropProcessedEntries(Level first, Level last, Level to_level)
{
  Level level = first;
  while (levelLessOrEqual(level, last)
         && levelLessOrEqual(level, to_level)) {
    VertexSeq &level_vertices = queue_[level];
    if (!level_vertices.empty()) {
      auto write = level_vertices.begin();
      // remove() nulls out entries (see BfsIterator::remove), so the
      // null check is required -- not defensive padding.
      for (Vertex *v : level_vertices) {
        if (v != nullptr && v->bfsInQueue(bfs_index_))
          *write++ = v;
      }
      level_vertices.erase(write, level_vertices.end());
    }
    incrLevel(level);
  }
  resetLevelBounds();
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
    else if (!variables_->useKahnsBfs()
             || !kahn_pred_
             || variables_->dynamicLoopBreaking()) {
      // Original level-based parallel BFS with per-level barriers.
      // dynamic_loop_breaking enables disabled-loop edges based on
      // arrival tags that only emerge during propagation. Kahn's
      // discovery runs before any propagation and cannot see those
      // tags, so we fall back to the original BFS whenever dynamic
      // loop breaking is active.
      std::vector<VertexVisitor *> visitors;
      visitors.reserve(thread_count_);
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
              size_t to = (k == thread_count - 1)
                  ? vertex_count : from + chunk_size;
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
          level_vertices.clear();
          visit_count += vertex_count;
        }
      }
      for (VertexVisitor *v : visitors)
        delete v;
    }
    else {
      // -------------------------------------------------------
      // Kahn's algorithm: process vertices as soon as all their
      // predecessors are done, eliminating per-level barriers.
      // -------------------------------------------------------

      // Lazy-init persistent Kahn state.
      if (!kahn_state_)
        kahn_state_ = std::make_unique<KahnState>();

      // Vertex IDs can exceed vertexCount() after deletions
      // (ObjectTable uses block-based IDs). Start with a
      // reasonable estimate and grow dynamically during discovery.
      VertexId vertex_count = graph_->vertexCount();
      kahn_state_->ensureInitSize(vertex_count + 1);
      kahn_state_->resetPrevious();

      std::vector<int> &in_deg = kahn_state_->in_degree_init;
      std::vector<Vertex *> &active_vertices = kahn_state_->active_vertices;
      active_vertices.clear();
      VertexId max_id = 0;
      // Under clks_only (findClkArrivals), the visitor uses
      // postponeClkFanouts to stop at reg CK; mirror that at the
      // discovery layer so the active set is narrowed instead of
      // eagerly walking the full data fanout.
      const bool stop_at_reg_clk = visitor->stopDiscoveryAtRegClk();
      // Opt-in: skip visitor body on vertices whose active-set
      // predecessors all reported no arrival change.
      const bool visit_skip = variables_->useKahnsVisitSkip();

      // Collect seed vertices from the level queue.
      Level saved_first = first_level_;
      Level saved_last = last_level_;
      Level level = first_level_;
      while (levelLessOrEqual(level, last_level_)
             && levelLessOrEqual(level, to_level)) {
        for (Vertex *vertex : queue_[level]) {
          if (vertex) {
            VertexId vid = graph_->id(vertex);
            if (vid >= in_deg.size())
              in_deg.resize(vid + 128, -1);
            if (in_deg[vid] == -1) {
              in_deg[vid] = 0;
              active_vertices.push_back(vertex);
              if (vid > max_id) max_id = vid;
            }
          }
        }
        incrLevel(level);
      }

      // BFS discovery -- mirrors enqueueAdjacentVertices logic.
      // Under stop_at_reg_clk, don't recurse past reg CK vertices
      // (matches Search.cc:1179 postponeClkFanouts semantics).
      size_t disc_idx = 0;
      while (disc_idx < active_vertices.size()) {
        Vertex *vertex = active_vertices[disc_idx++];
        if (stop_at_reg_clk && vertex->isRegClk())
          continue;
        kahnForEachSuccessor(vertex, kahn_pred_,
                             [&](Vertex *succ) {
          if (!levelLessOrEqual(succ->level(), to_level))
            return;
          VertexId sid = graph_->id(succ);
          if (sid >= in_deg.size())
            in_deg.resize(sid + 128, -1);
          if (in_deg[sid] == -1) {
            in_deg[sid] = 1;
            active_vertices.push_back(succ);
            succ->setBfsInQueue(bfs_index_, true);
            if (sid > max_id) max_id = sid;
          }
          else
            in_deg[sid]++;
        });
      }

      size_t active_count = active_vertices.size();
      debugPrint(debug_, "bfs", 1, "kahns {} active vertices", active_count);

      if (active_count == 0) {
        kahn_state_->prev_ids.clear();
        dropProcessedEntries(saved_first, saved_last, to_level);
        return 0;
      }

      // Size atomic array to cover max discovered ID.
      kahn_state_->ensureAtomicSize(max_id + 1);
      std::atomic<int> *in_degree = kahn_state_->in_degree.get();
      std::atomic<uint8_t> *pred_changed = nullptr;
      if (visit_skip) {
        kahn_state_->ensurePredChangedSize(max_id + 1);
        pred_changed = kahn_state_->pred_changed.get();
      }

      // Copy active in-degrees to atomic array and record IDs
      // for cleanup on the next call. Under visit_skip, init
      // pred_changed: seeds (in_deg==0) start at 1 so they are
      // always visited; non-seeds start at 0 and flip to 1 when
      // any predecessor reports a change.
      kahn_state_->prev_ids.clear();
      kahn_state_->prev_ids.reserve(active_count);
      int initial_ready_count = 0;
      for (Vertex *v : active_vertices) {
        VertexId vid = graph_->id(v);
        in_degree[vid].store(in_deg[vid], std::memory_order_relaxed);
        if (pred_changed != nullptr)
          pred_changed[vid].store(in_deg[vid] == 0 ? 1 : 0,
                                  std::memory_order_relaxed);
        kahn_state_->prev_ids.push_back(vid);
        if (in_deg[vid] == 0)
          initial_ready_count++;
      }
      debugPrint(debug_, "bfs", 1, "kahns {} initial ready",
                 initial_ready_count);

      // Phase 3: Kahn's traversal with per-worker local batches.
      // Each worker drains a thread-local ready vector in-line,
      // only spilling half back to DispatchQueue when the batch
      // exceeds kKahnBatchSpillThreshold so idle workers can steal.
      // Collapses O(vertex) dispatches to O(thread_count + spills).
      std::vector<VertexVisitor *> visitors;
      for (size_t k = 0; k < thread_count; k++)
        visitors.push_back(visitor->copy());

      std::atomic<int> total_visited{0};
      BfsIndex bfs_index = bfs_index_;
      SearchPred *pred = kahn_pred_;
      size_t in_deg_size = in_deg.size();

      // Steady-state fan-out rarely exceeds this; wide bursts (clock
      // boundaries) spill so work reaches idle workers.
      constexpr size_t kKahnBatchSpillThreshold = 64;
      // A spill event hands the older half of a worker's batch back
      // to DispatchQueue as this many shard-tasks (round-robin), so
      // the dispatch mutex is hit a bounded small number of times
      // per spill rather than once per spilled vertex. Mirrors the
      // shard pattern used by the initial seeding dispatches below.
      constexpr size_t kKahnSpillShardCount = 4;
      std::vector<std::vector<Vertex *>> &local_ready
          = kahn_state_->local_ready;
      // Resize outer to thread_count (thread_count can change between
      // calls via sta::set_thread_count). Inner backing stores are
      // retained; only newly-added slots allocate.
      if (local_ready.size() != thread_count)
        local_ready.resize(thread_count);
      for (auto &b : local_ready) {
        b.clear();
        if (b.capacity() < kKahnBatchSpillThreshold * 2)
          b.reserve(kKahnBatchSpillThreshold * 2);
      }

      // Recursive task lambda: self-reference via std::function.
      // Captures persist on visitParallel's stack until finishTasks
      // returns.
      std::function<void(Vertex*, size_t)> process;
      process = [&, bfs_index, pred, in_deg_size,
                 stop_at_reg_clk, visit_skip,
                 pred_changed](Vertex *seed,
                               size_t tid) {
        auto &batch = local_ready[tid];
        batch.push_back(seed);
        while (!batch.empty()) {
          Vertex *vertex = batch.back();
          batch.pop_back();
          vertex->setBfsInQueue(bfs_index, false);
          // visit_skip optimization: if no active-set predecessor
          // reported a change, skip the visitor body. We still walk
          // successors for the decrement so Kahn's invariant holds.
          bool changed = true;
          const bool skip = visit_skip
              && pred_changed[graph_->id(vertex)]
                     .load(std::memory_order_acquire) == 0;
          if (skip) {
            changed = false;
          } else {
            visitors[tid]->visit(vertex);
            total_visited.fetch_add(1, std::memory_order_relaxed);
            if (visit_skip)
              changed = visitors[tid]->lastVisitChanged();
          }
          // Skip the edge-list walk past reg CK under stop_at_reg_clk;
          // per-edge in_deg guard below would reject each hit anyway.
          if (stop_at_reg_clk && vertex->isRegClk())
            continue;
          kahnForEachSuccessor(vertex, pred, [&](Vertex *succ) {
            VertexId sid = graph_->id(succ);
            if (sid < in_deg_size && in_deg[sid] >= 0) {
              // Propagate change signal to successor before the
              // fetch_sub's release so the winner observes it. The
              // relaxed-load guard elides redundant stores when a
              // prior predecessor already set the flag, avoiding
              // cache-line bouncing on high-fan-in vertices.
              if (visit_skip && changed
                  && pred_changed[sid]
                         .load(std::memory_order_relaxed) == 0)
                pred_changed[sid].store(1, std::memory_order_release);
              int prev = in_degree[sid]
                  .fetch_sub(1, std::memory_order_acq_rel);
              if (prev == 1)
                batch.push_back(succ);
            }
          });
          if (batch.size() > kKahnBatchSpillThreshold) {
            // Hand older half back so idle workers can steal. Shard
            // the spill into up to kKahnSpillShardCount chunks so the
            // dispatch mutex is hit a small bounded number of times
            // per spill event rather than once per spilled vertex.
            // Mirrors the seeding shard pattern below: each chunk
            // pre-loads into the receiving worker's local_ready and
            // enters the drain loop via process(chunk[0], t).
            const size_t spill = batch.size() / 2;
            const size_t shards
                = std::min<size_t>(kKahnSpillShardCount, spill);
            for (size_t s = 0; s < shards; s++) {
              std::vector<Vertex *> chunk;
              chunk.reserve(spill / shards + 1);
              for (size_t i = s; i < spill; i += shards)
                chunk.push_back(batch[i]);
              if (chunk.empty())
                continue;
              dispatch_queue_->dispatch(
                [&process, &local_ready,
                 chunk = std::move(chunk)](size_t t) {
                  auto &nested = local_ready[t];
                  for (size_t i = 1; i < chunk.size(); i++)
                    nested.push_back(chunk[i]);
                  process(chunk[0], t);
                });
            }
            batch.erase(batch.begin(), batch.begin() + spill);
          }
        }
      };

      // Shard seeds across up to thread_count dispatches instead of
      // dispatching per-seed. Each worker pre-loads its local batch
      // with its shard and then runs the drain loop.
      std::vector<Vertex *> &seeds = kahn_state_->seeds;
      seeds.clear();
      if (seeds.capacity() < static_cast<size_t>(initial_ready_count))
        seeds.reserve(initial_ready_count);
      for (Vertex *v : active_vertices) {
        if (in_deg[graph_->id(v)] == 0)
          seeds.push_back(v);
      }
      size_t shards = std::min<size_t>(thread_count,
                                       std::max<size_t>(seeds.size(), 1));
      for (size_t s = 0; s < shards; s++) {
        std::vector<Vertex *> chunk;
        chunk.reserve(seeds.size() / shards + 1);
        for (size_t i = s; i < seeds.size(); i += shards)
          chunk.push_back(seeds[i]);
        if (chunk.empty())
          continue;
        dispatch_queue_->dispatch(
          [&process, &local_ready, chunk = std::move(chunk)](size_t t) {
            auto &batch = local_ready[t];
            for (size_t i = 1; i < chunk.size(); i++)
              batch.push_back(chunk[i]);
            process(chunk[0], t);
          });
      }
      dispatch_queue_->finishTasks();

      visit_count = total_visited.load(std::memory_order_relaxed);

      for (VertexVisitor *v : visitors)
        delete v;

      dropProcessedEntries(saved_first, saved_last, to_level);
    }
  }
  visit_count_cumulative_.fetch_add(visit_count, std::memory_order_relaxed);
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
  if (std::cmp_greater(queue_.size(), level)) {
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
  if (vertex->bfsInQueue(bfs_index_) && std::cmp_greater(queue_.size(), level)) {
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
BfsFwdIterator::kahnForEachSuccessor(Vertex *vertex,
                                     SearchPred *pred,
                                     const VertexFn &fn)
{
  if (pred->searchFrom(vertex)) {
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (pred->searchThru(edge) && pred->searchTo(to_vertex))
        fn(to_vertex);
    }
  }
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
BfsBkwdIterator::kahnForEachSuccessor(Vertex *vertex,
                                      SearchPred *pred,
                                      const VertexFn &fn)
{
  if (pred->searchTo(vertex)) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph_);
      if (pred->searchFrom(from_vertex) && pred->searchThru(edge))
        fn(from_vertex);
    }
  }
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

}  // namespace sta
