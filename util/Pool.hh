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

#ifndef STA_POOL_H
#define STA_POOL_H

#include <stddef.h>
#include <algorithm> // max
#include "DisallowCopyAssign.hh"
#include "Error.hh"
#include "Vector.hh"
#include "ObjectIndex.hh"

namespace sta {

using std::max;

typedef Vector<ObjectIndex> DeletedListHeads;

template <class OBJ>
class PoolBlock
{
public:
  explicit PoolBlock(ObjectIndex size,
		     ObjectIndex begin_index);
  OBJ *makeObject();
  OBJ *makeObjects(ObjectIndex count);
  ObjectIndex index(const OBJ *object);
  OBJ *find(ObjectIndex index);
  PoolBlock *nextBlock() const { return next_block_; }
  void setNextBlock(PoolBlock *next);
  ObjectIndex size() const { return objects_.size(); }

private:
  DISALLOW_COPY_AND_ASSIGN(PoolBlock);

  std::vector<OBJ> objects_;
  ObjectIndex begin_index_;
  ObjectIndex next_free_;
  PoolBlock *next_block_;
  static constexpr unsigned min_initial_size_ = 100;
};

template <class OBJ>
PoolBlock<OBJ>::PoolBlock(ObjectIndex size,
			  ObjectIndex begin_index) :
  objects_(size),
  begin_index_(begin_index),
  next_free_(0),
  next_block_(nullptr)
{
}

template <class OBJ>
OBJ *
PoolBlock<OBJ>::makeObject()
{
  if (next_free_ < objects_.size())
    return &objects_[next_free_++];
  else
    return nullptr;
}

template <class OBJ>
OBJ *
PoolBlock<OBJ>::makeObjects(ObjectIndex count)
{
  if ((next_free_ + count - 1) < objects_.size()) {
    OBJ *object = &objects_[next_free_];
    next_free_ += count;
    return object;
  }
  else
    return nullptr;
}

template <class OBJ>
ObjectIndex
PoolBlock<OBJ>::index(const OBJ *object)
{
  if (object >= &objects_[0] && object < &objects_[objects_.size()])
    // Index==0 is reserved.
    return begin_index_ + object - &objects_[0] + 1;
  else
    return 0;
}

template <class OBJ>
OBJ *
PoolBlock<OBJ>::find(ObjectIndex index)
{
  // Index==0 is reserved.
  ObjectIndex index1 = index - 1;
  if (index1 >= begin_index_
      && index1 < begin_index_ + objects_.size())
    return &objects_[index1 - begin_index_];
  else
    return nullptr;
}

template <class OBJ>
void
PoolBlock<OBJ>::setNextBlock(PoolBlock *next)
{
  next_block_ = next;
}

////////////////////////////////////////////////////////////////

template <class OBJ>
class Pool
{
public:
  Pool();
  explicit Pool(ObjectIndex size);
  explicit Pool(ObjectIndex size,
		float growth_factor);
  ~Pool();
  OBJ *makeObject();
  OBJ *makeObjects(ObjectIndex count);
  void deleteObject(OBJ *object);
  void deleteObject(ObjectIndex index);
  void deleteObjects(OBJ *objects,
		     ObjectIndex count);
  void deleteObjects(ObjectIndex index,
		     ObjectIndex count);
  // Index=0 is reserved for the nullptr object pointer.
  ObjectIndex index(const OBJ *object) const;
  OBJ *find(ObjectIndex index) const;
  ObjectIndex size() const { return size_; }
  void clear();

protected:
  PoolBlock<OBJ> *makeBlock(ObjectIndex block_size);
  OBJ *findDeletedObject(ObjectIndex count);
  void deleteObjects(OBJ *object,
		     ObjectIndex index,
		     ObjectIndex count);

  ObjectIndex size_;
  float growth_factor_;
  PoolBlock<OBJ> *blocks_;
  PoolBlock<OBJ> *last_block_;
  // Index of deleted objects that are the head of a list indexed by
  // object array count.
  DeletedListHeads deleted_list_heads_;
};

template <class OBJ>
Pool<OBJ>::Pool(ObjectIndex size) :
  Pool(size, .2F)
{
}

template <class OBJ>
Pool<OBJ>::Pool(ObjectIndex size,
		float growth_factor) :
  size_(0),
  growth_factor_(growth_factor),
  blocks_(nullptr),
  last_block_(nullptr)
{
  makeBlock(size);
}

template <class OBJ>
Pool<OBJ>::~Pool()
{
  PoolBlock<OBJ> *next;
  for (PoolBlock<OBJ> *block = blocks_; block; block = next) {
    next = block->nextBlock();
    delete block;
  }
}

template <class OBJ>
OBJ *
Pool<OBJ>::makeObject()
{
  OBJ *object = findDeletedObject(1);
  if (object == nullptr) {
    object = last_block_->makeObject();
    if (object == nullptr) {
      ObjectIndex block_size=static_cast<ObjectIndex>(size_*growth_factor_+2);
      PoolBlock<OBJ> *block = makeBlock(block_size);
      object = block->makeObject();
    }
  }
  return object;
}

template <class OBJ>
OBJ *
Pool<OBJ>::findDeletedObject(ObjectIndex count)
{
  if (deleted_list_heads_.size() > count) {
    ObjectIndex index = deleted_list_heads_[count];
    if (index) {
      OBJ *object = find(index);
      ObjectIndex next_index = *reinterpret_cast<ObjectIndex*>(object);
      deleted_list_heads_[count] = next_index;
      return object;
    }
  }
  return nullptr;
}

template <class OBJ>
OBJ *
Pool<OBJ>::makeObjects(ObjectIndex count)
{
  OBJ *objects = findDeletedObject(count);
  if (objects == nullptr) {
    objects = last_block_->makeObjects(count);
    if (objects == nullptr) {
      ObjectIndex block_size=max(static_cast<ObjectIndex>(size_*growth_factor_+2),
				 count);
      PoolBlock<OBJ> *block = makeBlock(block_size);
      objects = block->makeObjects(count);
    }
  }
  return objects;
}

template <class OBJ>
PoolBlock<OBJ> *
Pool<OBJ>::makeBlock(ObjectIndex block_size)
{
  PoolBlock<OBJ> *block = new PoolBlock<OBJ>(block_size, size_);
  // Add new block to end of block list so searches start with
  // the first block.
  if (last_block_)
    last_block_->setNextBlock(block);
  else
    blocks_ = block;
  last_block_ = block;
  size_ += block_size;
  return block;
}

template <class OBJ>
ObjectIndex
Pool<OBJ>::index(const OBJ *object) const
{
  if (object) {
    for (PoolBlock<OBJ> *block = blocks_; block; block = block->nextBlock()) {
      ObjectIndex index = block->index(object);
      if (index > 0)
	return index;
    }
    internalError("object index not found in pool");
    return 0;
  }
  else
    return 0;
}

template <class OBJ>
OBJ *
Pool<OBJ>::find(ObjectIndex index) const
{
  if (index) {
    for (PoolBlock<OBJ> *block = blocks_; block; block = block->nextBlock()) {
      OBJ *object = block->find(index);
      if (object)
	return object;
    }
    internalError("object index not found in pool");
    return nullptr;
  }
  else
    return nullptr;
}

template <class OBJ>
void
Pool<OBJ>::deleteObject(OBJ *object)
{
  deleteObjects(object, index(object), 1);
}

template <class OBJ>
void
Pool<OBJ>::deleteObject(ObjectIndex index)
{
  deleteObjects(find(index), index, 1);
}

template <class OBJ>
void
Pool<OBJ>::deleteObjects(OBJ *objects,
			 ObjectIndex count)
{
  deleteObjects(objects, index(objects), count);
}

template <class OBJ>
void
Pool<OBJ>::deleteObjects(ObjectIndex index,
			 ObjectIndex count)
{
  deleteObjects(find(index), index, count);
}

template <class OBJ>
void
Pool<OBJ>::deleteObjects(OBJ *object,
			 ObjectIndex index,
			 ObjectIndex count)
{
  if (deleted_list_heads_.size() <= count)
    deleted_list_heads_.resize(count + 1);
  ObjectIndex next_index = deleted_list_heads_[count];
  *reinterpret_cast<ObjectIndex*>(object) = next_index;
  deleted_list_heads_[count] = index;
}

template <class OBJ>
void
Pool<OBJ>::clear()
{
  last_block_ = blocks_;
  last_block_->clear();
}

} // Namespace
#endif
