// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
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

#pragma once

#include <stddef.h>  // size_t
#include "Hash.hh"

namespace sta {

template <class KEY> class HashSetBucket;

template <class KEY, class HASH, class EQUAL>
class HashSet
{
public:
  HashSet();
  explicit HashSet(size_t capacity,
		   bool auto_resize);
  explicit HashSet(size_t capacity,
		   bool auto_resize,
		   HASH hash,
		   EQUAL equal);
  ~HashSet();
  // Number of objects in the table.
  size_t size() const { return size_; }
  // Number of hash buckets.
  size_t capacity() const { return capacity_; }
  // Multi-threaded findKey is safe during resize.
  void reserve(size_t capacity);
  void insert(KEY key);
  KEY findKey(KEY key);
  KEY findKey(KEY key) const;
  bool hasKey(KEY key);
  void erase(KEY key);
  bool empty() const;
  void clear();
  void deleteContentsClear();
  int longestBucketLength() const;
  size_t longestBucketHash() const;
  int bucketLength(size_t hash) const;

  void
  deleteContents()
  {
    Iterator iter(this);
    while (iter.hasNext())
      delete iter.next();
  }

  // Java style container itererator
  //  Map::Iterator<string *, Value, stringLess> iter(map);
  //  while (iter.hasNext()) {
  //    Value *v = iter.next();
  //  }
  class Iterator
  {
  public:
    Iterator() : container_(nullptr) {}
    explicit Iterator(HashSet<KEY, HASH, EQUAL> *container)
    { init(container); }
    explicit Iterator(HashSet<KEY, HASH, EQUAL> &container)
    { init(container); }
    void init(HashSet<KEY, HASH, EQUAL> *container)
    { container_ = container;
      hash_= 0;
      next_ = nullptr;
      if (container_)
	findNext();
    }
    void init(HashSet<KEY, HASH, EQUAL> &container)
    { container_ = &container;
      hash_= 0;
      next_ = nullptr;
      if (container_)
	findNext();
    }
    bool hasNext() { return container_ && next_ != nullptr; }
    KEY next() { 
      HashSetBucket<KEY> *next = next_;
      findNext();
      return next->key();
    }
    HashSet<KEY, HASH, EQUAL> *container() { return container_; }

  private:
    void
    findNext()
    {
      if (next_)
	next_ = next_->next();
      while (next_ == nullptr
	     && hash_ < container_->capacity())
	next_ = container_->table_[hash_++];
    }
    HashSet<KEY, HASH, EQUAL> *container_;
    size_t hash_;
    HashSetBucket<KEY> *next_;
  };

  class ConstIterator
  {
  public:
    ConstIterator() : container_(nullptr) {}
    explicit ConstIterator(const HashSet<KEY, HASH, EQUAL> *container)
    { init(container); }
    explicit ConstIterator(const HashSet<KEY, HASH, EQUAL> &container)
    { init(container); }
    void init(const HashSet<KEY, HASH, EQUAL> *container)
    { container_ =  container; hash_= 0; next_ = nullptr; findNext(); }
    void init(HashSet<KEY, HASH, EQUAL> &container)
    { container_ = &container; hash_= 0; next_ = nullptr; findNext(); }
    bool hasNext() { return container_ && next_ != nullptr; }
    KEY next() { 
      HashSetBucket<KEY> *next = next_;
      findNext();
      return next->key();
    }
    HashSet<KEY, HASH, EQUAL> *container() { return container_; }

  private:
    void
    findNext()
    {
      if (next_)
	next_ = next_->next();
      while (next_ == nullptr
	     && hash_ < container_->capacity())
	next_ = container_->table_[hash_++];
    }
    const HashSet<KEY, HASH, EQUAL> *container_;
    size_t hash_;
    HashSetBucket<KEY> *next_;
  };

protected:
  void initTable();
  static const int default_capacity = (2 << 6) - 1;

  // Table size.
  size_t capacity_;
  bool auto_resize_;
  HASH hash_;
  EQUAL equal_;
  size_t size_;
  HashSetBucket<KEY> **table_;
  HashSet *tmp_;
};

template <class KEY>
class HashSetBucket
{
public:
  HashSetBucket(KEY key, HashSetBucket *next);
  KEY key() const { return key_; }
  void setKey(KEY key) { key_ = key; }
  HashSetBucket *next() const { return next_; }
  void setNext(HashSetBucket *next) { next_ = next; }

private:
  KEY key_;
  HashSetBucket *next_;
};

template <class KEY>
HashSetBucket<KEY>::HashSetBucket(KEY key,
				  HashSetBucket *next) :
  key_(key),
  next_(next)
{
}

////////////////////////////////////////////////////////////////

template <class KEY, class HASH, class EQUAL>
HashSet<KEY, HASH, EQUAL>::HashSet() :
  capacity_(default_capacity),
  auto_resize_(true),
  hash_(HASH()),
  equal_(EQUAL())
{
  initTable();
}

template <class KEY, class HASH, class EQUAL>
HashSet<KEY, HASH, EQUAL>::HashSet(size_t capacity,
				   bool auto_resize) :
  capacity_(capacity),
  auto_resize_(auto_resize),
  hash_(HASH()),
  equal_(EQUAL())
{
  initTable();
}

template <class KEY, class HASH, class EQUAL>
HashSet<KEY, HASH, EQUAL>::HashSet(size_t capacity,
				   bool auto_resize,
				   HASH hash,
				   EQUAL equal) :
  capacity_(capacity),
  auto_resize_(auto_resize),
  hash_(hash),
  equal_(equal)
{
  initTable();
}

template <class KEY, class HASH, class EQUAL>
void
HashSet<KEY, HASH, EQUAL>::initTable()
{
  size_ = 0;
  tmp_ = nullptr;
  table_ = new HashSetBucket<KEY>*[capacity_];
  for (size_t i = 0; i < capacity_; i++)
    table_[i] = nullptr;
}

template <class KEY, class HASH, class EQUAL>
HashSet<KEY, HASH, EQUAL>::~HashSet()
{
  for (size_t hash = 0; hash < capacity_; hash++) {
    HashSetBucket<KEY> *next;
    for (HashSetBucket<KEY> *bucket = table_[hash];
	 bucket;
	 bucket = next) {
      next = bucket->next();
      delete bucket;
    }
  }
  delete [] table_;
  delete tmp_;
}

template <class KEY, class HASH, class EQUAL>
KEY
HashSet<KEY, HASH, EQUAL>::findKey(KEY key) const
{
  size_t hash = hash_(key) % capacity_;
  HashSetBucket<KEY> *head = table_[hash];
  for (HashSetBucket<KEY> *bucket = head; bucket; bucket = bucket->next()) {
    KEY bucket_key = bucket->key();
    if (equal_(bucket_key, key)) {
      return bucket_key;
    }
  }
  return nullptr;
}

template <class KEY, class HASH, class EQUAL>
KEY
HashSet<KEY, HASH, EQUAL>::findKey(KEY key)
{
  size_t hash = hash_(key) % capacity_;
  HashSetBucket<KEY> *head = table_[hash];
  for (HashSetBucket<KEY> *bucket = head; bucket; bucket = bucket->next()) {
    KEY bucket_key = bucket->key();
    if (equal_(bucket_key, key)) {
      return bucket_key;
    }
  }
  return nullptr;
}

template <class KEY, class HASH, class EQUAL>
bool
HashSet<KEY, HASH, EQUAL>::hasKey(KEY key)
{
  size_t hash = hash_(key) % capacity_;
  HashSetBucket<KEY> *head = table_[hash];
  for (HashSetBucket<KEY> *bucket = head; bucket; bucket = bucket->next()) {
    KEY bucket_key = bucket->key();
    if (equal_(bucket_key, key)) {
      return true;
    }
  }
  return false;
}

template <class KEY, class HASH, class EQUAL>
void
HashSet<KEY, HASH, EQUAL>::insert(KEY key)
{
  size_t hash = hash_(key) % capacity_;
  HashSetBucket<KEY> *head = table_[hash];
  for (HashSetBucket<KEY> *bucket = head; bucket; bucket = bucket->next()) {
    if (equal_(bucket->key(), key)) {
      bucket->setKey(key);
      return;
    }
  }
  HashSetBucket<KEY> *bucket = new HashSetBucket<KEY>(key, head);
  table_[hash] = bucket;
  size_++;
  if (size_ > capacity_
      && auto_resize_)
    reserve(nextMersenne(capacity_));
}

template <class KEY, class HASH, class EQUAL>
void
HashSet<KEY, HASH, EQUAL>::reserve(size_t capacity)
{
  if (capacity != capacity_) {
    if (size_ == 0) {
      // Table is empty.
      capacity_ = capacity;
      delete [] table_;
      delete tmp_;
      initTable();
    }
    else {
      delete tmp_;
      // Copy entries to tmp.
      tmp_ = new HashSet<KEY, HASH, EQUAL>(capacity, auto_resize_,
					   hash_, equal_);
      for (size_t hash = 0; hash < capacity_; hash++) {
	for (HashSetBucket<KEY> *bucket = table_[hash];
	     bucket;
	     bucket = bucket->next())
	  tmp_->insert(bucket->key());
      }

      size_t prev_capacity = capacity_;
      HashSetBucket<KEY> **prev_table = table_;

      // Switch over.
      table_ = tmp_->table_;
      capacity_ = capacity;

      tmp_->capacity_ = prev_capacity;
      tmp_->table_ = prev_table;
    }
  }
}

template <class KEY, class HASH, class EQUAL>
void
HashSet<KEY, HASH, EQUAL>::erase(KEY key)
{
  size_t hash = hash_(key) % capacity_;
  HashSetBucket<KEY> *head = table_[hash];
  HashSetBucket<KEY> *prev = nullptr;
  for (HashSetBucket<KEY> *bucket = head; bucket; bucket = bucket->next()) {
    if (equal_(bucket->key(), key)) {
      if (prev)
	prev->setNext(bucket->next());
      else
	table_[hash] = bucket->next();
      delete bucket;
      size_--;
      break;
    }
    prev = bucket;
  }
}

template <class KEY, class HASH, class EQUAL>
void
HashSet<KEY, HASH, EQUAL>::deleteContentsClear()
{
  if (size_ > 0) {
    for (size_t hash = 0; hash < capacity_; hash++) {
      HashSetBucket<KEY> *next;
      for (HashSetBucket<KEY> *bucket = table_[hash];
	   bucket;
	   bucket = next) {
	delete bucket->key();
	next = bucket->next();
	delete bucket;
      }
      table_[hash] = nullptr;
    }
    size_ = 0;
  }
}

template <class KEY, class HASH, class EQUAL>
void
HashSet<KEY, HASH, EQUAL>::clear()
{
  if (size_ > 0) {
    for (size_t hash = 0; hash < capacity_; hash++) {
      HashSetBucket<KEY> *next;
      for (HashSetBucket<KEY> *bucket = table_[hash];
	   bucket;
	   bucket = next) {
	next = bucket->next();
	delete bucket;
      }
      table_[hash] = nullptr;
    }
    size_ = 0;
  }
}

template <class KEY, class HASH, class EQUAL>
bool
HashSet<KEY, HASH, EQUAL>::empty() const
{
  return size_ == 0;
}

template <class KEY, class HASH, class EQUAL>
int
HashSet<KEY, HASH, EQUAL>::longestBucketLength() const
{
  return bucketLength(longestBucketHash());
}

template <class KEY, class HASH, class EQUAL>
size_t
HashSet<KEY, HASH, EQUAL>::longestBucketHash() const
{
  int longest = 0;
  size_t longest_hash = 0;
  for (size_t hash = 0; hash < capacity_; hash++) {
    int length = bucketLength(hash);
    if (length > longest) {
      longest = length;
      longest_hash = hash;
    }
  }
  return longest_hash;
}

template <class KEY, class HASH, class EQUAL>
int
HashSet<KEY, HASH, EQUAL>::bucketLength(size_t hash) const
{
  int length = 0;
  for (HashSetBucket<KEY> *bucket = table_[hash];
       bucket;
       bucket = bucket->next())
    length++;
  return length;
}

} // namespace
