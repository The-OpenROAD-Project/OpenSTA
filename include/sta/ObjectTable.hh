// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#pragma once

#include "Vector.hh"
#include "Error.hh"
#include "ObjectId.hh"

namespace sta {

template <class OBJECT>
class TableBlock;

// Object tables allocate objects in blocks and use 32 bit IDs to
// reference an object. Paging performance is improved by allocating
// blocks instead of individual objects, and object sizes are reduced
// by using 32 bit references instead of 64 bit pointers.
//
// Class TYPE must define member functions
//  ObjectIdx objectIdx() const
//  void setObjectIdx(ObjectIdx idx)
// to get/set the index of the object in a block, which can be a bit
// field ObjectTable::idx_bits (7 bits) wide.

template <class TYPE>
class ObjectTable
{
public:
  ObjectTable();
  ~ObjectTable();
  TYPE *make();
  void destroy(TYPE *object);
  TYPE *pointer(ObjectId id) const;
  TYPE &ref(ObjectId id) const;
  ObjectId objectId(const TYPE *object);
  size_t size() const { return size_; }
  void clear();

  // Objects are allocated in blocks of 128.
  static constexpr int idx_bits = 7;
  static constexpr int block_object_count = (1 << idx_bits);
  static constexpr int block_id_max = 1 << (object_id_bits - idx_bits);

private:
  void makeBlock();
  void freePush(TYPE *object,
		ObjectId id);

  size_t size_;
  // Object ID of next free object.
  ObjectId free_;
  Vector<TableBlock<TYPE>*> blocks_;
  static constexpr ObjectId idx_mask_ = block_object_count - 1;
};

template <class TYPE>
ObjectTable<TYPE>::ObjectTable() :
  size_(0),
  free_(object_id_null)
{
}

template <class TYPE>
ObjectTable<TYPE>::~ObjectTable()
{
  blocks_.deleteContents();
}

template <class TYPE>
TYPE *
ObjectTable<TYPE>::make()
{
  if (free_ == object_id_null)
    makeBlock();
  TYPE *object = pointer(free_);
  ObjectIdx idx = free_ & idx_mask_;
  object->setObjectIdx(idx);
  ObjectId *free_next = reinterpret_cast<ObjectId*>(object);
  free_ = *free_next;
  size_++;
  return object;
}

template <class TYPE>
void
ObjectTable<TYPE>::freePush(TYPE *object,
			    ObjectId id)
{
  // Link free objects into a list linked by Object ID.
  ObjectId *free_next = reinterpret_cast<ObjectId*>(object);
  *free_next = free_;
  free_ = id;
}

template <class TYPE>
void
ObjectTable<TYPE>::makeBlock()
{
  BlockIdx block_index = blocks_.size();
  TableBlock<TYPE> *block = new TableBlock<TYPE>(block_index, this);
  blocks_.push_back(block);
  if (blocks_.size() >= block_id_max)
    criticalError(224, "max object table block count exceeded.");
  // ObjectId zero is reserved for object_id_null.
  int last = (block_index > 0) ? 0 : 1;
  for (int i = block_object_count - 1; i >= last; i--) {
    TYPE *obj = block->pointer(i);
    ObjectId id = (block_index << idx_bits) + i;
    freePush(obj, id);
  }
}

template <class TYPE>
TYPE *
ObjectTable<TYPE>::pointer(ObjectId id) const
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
TYPE &
ObjectTable<TYPE>::ref(ObjectId id) const
{
  if (id == object_id_null)
    criticalError(225, "null ObjectId reference is undefined.");
  else {
    BlockIdx blk_idx = id >> idx_bits;
    ObjectIdx obj_idx = id & idx_mask_;
    return blocks_[blk_idx]->ptr(obj_idx);
  }
}

template <class TYPE>
ObjectId
ObjectTable<TYPE>::objectId(const TYPE *object)
{
  ObjectIdx idx = object->objectIdx();
  const TableBlock<TYPE> *blk =
    reinterpret_cast<const TableBlock<TYPE>*>(object - idx);
  return (blk->index() << idx_bits) + idx;
}

template <class TYPE>
void
ObjectTable<TYPE>::destroy(TYPE *object)
{
  ObjectId object_id = objectId(object);
  object->~TYPE();
  size_--;
  freePush(object, object_id);
}

template <class TYPE>
void
ObjectTable<TYPE>::clear()
{
  blocks_.deleteContentsClear();
  size_ = 0;
}

////////////////////////////////////////////////////////////////

template <class TYPE>
class TableBlock
{
public:
  TableBlock(BlockIdx block_idx,
	     ObjectTable<TYPE> *table);
  BlockIdx index() const { return block_idx_; }
  TYPE &ref(ObjectIdx idx) { return objects_[idx]; }
  TYPE *pointer(ObjectIdx idx) { return &objects_[idx]; }

private:
  TYPE objects_[ObjectTable<TYPE>::block_object_count];
  BlockIdx block_idx_;
  ObjectTable<TYPE> *table_;
};

template <class TYPE>
TableBlock<TYPE>::TableBlock(BlockIdx block_idx,
			     ObjectTable<TYPE> *table) :
  block_idx_(block_idx),
  table_(table)
{
}

} // Namespace
