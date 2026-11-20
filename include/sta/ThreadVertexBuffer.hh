// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>

#include "GraphClass.hh"

namespace sta {

// OpenROAD fork: collects vertices from many threads without a shared
// lock. Each thread appends to its own shard. drain(), moveTo() and clear()
// are called from a single thread while no thread is adding.
class ThreadVertexBuffer
{
public:
  void add(Vertex *vertex)
  {
    Shard &shard = shards_[threadShard()];
    std::lock_guard<std::mutex> lock(shard.lock);
    shard.vertices.push_back(vertex);
    shard.size.store(shard.vertices.size(), std::memory_order_release);
    // Only the first add after a drain writes the shared flag; later adds
    // just read it, so threads do not contend on its cache line.
    if (!maybe_nonempty_.load(std::memory_order_relaxed))
      maybe_nonempty_.store(true, std::memory_order_release);
  }

  // Safe to call while other threads add; the answer may be stale.
  bool empty() const
  {
    if (!maybe_nonempty_.load(std::memory_order_acquire))
      return true;
    for (const Shard &shard : shards_) {
      if (shard.size.load(std::memory_order_acquire) != 0)
        return false;
    }
    return true;
  }

  // Call visit(vertex) for each buffered vertex, then empty the buffer.
  template <typename Visit>
  void drain(Visit visit)
  {
    if (!maybe_nonempty_.load(std::memory_order_acquire))
      return;
    maybe_nonempty_.store(false, std::memory_order_relaxed);
    for (Shard &shard : shards_) {
      if (shard.size.load(std::memory_order_acquire) == 0)
        continue;
      for (Vertex *vertex : shard.vertices)
        visit(vertex);
      shard.vertices.clear();
      shard.size.store(0, std::memory_order_release);
    }
  }

  void moveTo(VertexSet &vertices)
  {
    drain([&vertices](Vertex *vertex) { vertices.insert(vertex); });
  }

  void clear()
  {
    drain([](Vertex *) {});
  }

private:
  static constexpr size_t shard_count_ = 64;

  // Each thread keeps the shard index it gets on first use. Threads can
  // share a shard (more than 64 threads over the process lifetime); that
  // only costs some lock contention on the shard.
  static size_t threadShard()
  {
    static std::atomic<size_t> next_shard{0};
    thread_local const size_t shard
        = next_shard.fetch_add(1, std::memory_order_relaxed) % shard_count_;
    return shard;
  }

  // Each shard is on its own cache line(s), so threads do not contend on a
  // shared counter.
  struct alignas(64) Shard
  {
    std::mutex lock;
    VertexSeq vertices;
    std::atomic<size_t> size{0};
  };
  std::array<Shard, shard_count_> shards_;
  // False only when every shard is empty; makes empty() and drain() a
  // single load in the common empty case.
  alignas(64) std::atomic<bool> maybe_nonempty_{false};
};

} // namespace sta
