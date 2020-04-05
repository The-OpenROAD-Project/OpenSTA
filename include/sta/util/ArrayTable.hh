// Parallax Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// All rights reserved.
// 
// No part of this document may be copied, transmitted or
// disclosed in any form or fashion without the express
// written consent of Parallax Software, Inc.

#ifndef STA_ARRAY_TABLE_H
#define STA_ARRAY_TABLE_H

#include "Vector.hh"
#include "ObjectId.hh"

namespace sta {

template <class TYPE>
class ArrayBlock;

// Array tables allocate arrays of objects in blocks and use 32 bit IDs to
// reference the array. Paging performance is improved by allocating
// blocks instead of individual arrays, and object sizes are reduced
// by using 32 bit references instead of 64 bit pointers.
// They are similar to ObjectTables but do not support delete/destroy or
// reclaiming deleted arrays.

template <class TYPE>
class ArrayTable
{
public:
  ArrayTable();
  ~ArrayTable();
  void make(uint32_t count,
	    TYPE *&array,
	    ObjectId &id);
  // Grow as necessary and return pointer for id.
  TYPE *ensureId(ObjectId id);
  TYPE *pointer(ObjectId id) const;
  TYPE &ref(ObjectId id) const;
  size_t size() const { return size_; }
  void clear();

  static constexpr ObjectId idx_bits = 10;
  static constexpr ObjectId block_size = (1 << idx_bits);

private:
  ArrayBlock<TYPE> *makeBlock(uint32_t size);

  size_t size_;
  // Block index of free block (blocks_[size - 1]).
  BlockIdx free_block_idx_;
  // Index of next free object in free_block_idx_.
  ObjectIdx free_idx_;
  Vector<ArrayBlock<TYPE>*> blocks_;
  static constexpr ObjectId idx_mask_ = block_size - 1;
};

template <class TYPE>
ArrayTable<TYPE>::ArrayTable() :
  size_(0),
  free_block_idx_(block_idx_null),
  free_idx_(object_idx_null)
{
}

template <class TYPE>
ArrayTable<TYPE>::~ArrayTable()
{
  blocks_.deleteContents();
}

template <class TYPE>
void
ArrayTable<TYPE>::make(uint32_t count,
		       TYPE *&array,
		       ObjectId &id)
{
  ArrayBlock<TYPE> *block = blocks_.size() ? blocks_[free_block_idx_] : nullptr;
  if ((free_idx_ == object_idx_null
       && free_block_idx_ == block_idx_null)
      || free_idx_ + count >= block->size()) {
    uint32_t size = (count > block_size) ? count : block_size;
    block = makeBlock(size);
  }
  // makeId(free_block_idx_, idx_bits)
  id = (free_block_idx_ << idx_bits) + free_idx_;
  array = block->pointer(free_idx_);
  free_idx_ += count;
  size_ += count;
}

template <class TYPE>
ArrayBlock<TYPE> *
ArrayTable<TYPE>::makeBlock(uint32_t size)
{
  BlockIdx block_idx = blocks_.size();
  ArrayBlock<TYPE> *block = new ArrayBlock<TYPE>(size);
  blocks_.push_back(block);
  free_block_idx_ = block_idx;
  // ObjectId zero is reserved for object_id_null.
  free_idx_ = (block_idx > 0) ? 0 : 1;
  return block;
}

template <class TYPE>
TYPE *
ArrayTable<TYPE>::pointer(ObjectId id) const
{
  if (id == object_id_null)
    return nullptr;
  else {
    BlockIdx blk_idx = id >> idx_bits;
    ObjectIdx obj_idx = id & idx_mask_;
    return blocks_[blk_idx]->pointer(obj_idx);
  }
}

template <class TYPE>
TYPE *
ArrayTable<TYPE>::ensureId(ObjectId id)
{
  BlockIdx blk_idx = id >> idx_bits;
  ObjectIdx obj_idx = id & idx_mask_;
  // Make enough blocks for blk_idx to be valid.
  for (BlockIdx i = blocks_.size(); i <= blk_idx; i++) {
    ArrayBlock<TYPE> *block = new ArrayBlock<TYPE>(block_size);
    blocks_.push_back(block);
  }
  return blocks_[blk_idx]->pointer(obj_idx);
}

template <class TYPE>
TYPE &
ArrayTable<TYPE>::ref(ObjectId id) const
{
  if (id == object_id_null)
    internalError("null ObjectId reference is undefined.");
  else {
    BlockIdx blk_idx = id >> idx_bits;
    ObjectIdx obj_idx = id & idx_mask_;
    return blocks_[blk_idx]->ref(obj_idx);
  }
}

template <class TYPE>
void
ArrayTable<TYPE>::clear()
{
  blocks_.deleteContentsClear();
  size_ = 0;
  free_block_idx_ = block_idx_null;
  free_idx_ = object_idx_null;
}

////////////////////////////////////////////////////////////////

template <class TYPE>
class ArrayBlock
{
public:
  ArrayBlock(uint32_t size);
  ~ArrayBlock();
  uint32_t size() const { return size_; }
  TYPE &ref(ObjectIdx idx) { return objects_[idx]; }
  TYPE *pointer(ObjectIdx idx) { return &objects_[idx]; }

private:
  uint32_t size_;
  TYPE *objects_;
};

template <class TYPE>
ArrayBlock<TYPE>::ArrayBlock(uint32_t size) :
  size_(size),
  objects_(new TYPE[size])
{
}

template <class TYPE>
ArrayBlock<TYPE>::~ArrayBlock()
{
  delete [] objects_;
}

} // Namespace
#endif
