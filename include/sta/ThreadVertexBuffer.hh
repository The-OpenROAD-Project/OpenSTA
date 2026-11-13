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
    {
      std::lock_guard<std::mutex> lock(shard.lock);
      shard.vertices.push_back(vertex);
    }
    count_.fetch_add(1, std::memory_order_release);
  }

  bool empty() const { return count_.load(std::memory_order_acquire) == 0; }

  // Call visit(vertex) for each buffered vertex, then empty the buffer.
  template <typename Visit>
  void drain(Visit visit)
  {
    if (empty())
      return;
    for (Shard &shard : shards_) {
      for (Vertex *vertex : shard.vertices)
        visit(vertex);
      shard.vertices.clear();
    }
    count_.store(0, std::memory_order_release);
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

  // Each thread keeps the shard index it gets on first use.
  static size_t threadShard()
  {
    static std::atomic<size_t> next_shard{0};
    thread_local const size_t shard
        = next_shard.fetch_add(1, std::memory_order_relaxed) % shard_count_;
    return shard;
  }

  struct alignas(64) Shard
  {
    std::mutex lock;
    VertexSeq vertices;
  };
  std::array<Shard, shard_count_> shards_;
  std::atomic<size_t> count_{0};
};

} // namespace sta
