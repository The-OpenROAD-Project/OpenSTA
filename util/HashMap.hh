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

#ifndef STA_HASHMAP_H
#define STA_HASHMAP_H

#include <stddef.h>  // size_t
#include "Hash.hh"

namespace sta {

template <class KEY, class VALUE> class HashMapBucket;


template <class OBJECT>
class PtrHash
{
public:
  Hash operator()(const OBJECT obj) const { return hashPtr(obj); }
};

template <class OBJECT>
class PtrEqual
{
public:
  bool operator()(const OBJECT obj1,
		  const OBJECT obj2) const
  { return obj1 == obj2; }
};

template <class KEY, class VALUE, class HASH = PtrHash<KEY>, class EQUAL = PtrEqual<KEY> >
class HashMap
{
public:
  HashMap();
  explicit HashMap(size_t capacity,
		   bool auto_resize);
  explicit HashMap(size_t capacity,
		   bool auto_resize,
		   HASH hash,
		   EQUAL equal);
  ~HashMap();
  // Number of objects in the table.
  size_t size() const { return size_; }
  // Number of hash buckets.
  size_t capacity() const { return capacity_; }
  // Multi-threaded findKey is safe during resize.
  void resize(size_t capacity);
  void insert(KEY key,
	      VALUE value);
  VALUE findKey(KEY key);
  VALUE findKey(KEY key) const;
  void findKey(KEY key,
	       VALUE &value,
	       bool &exists) const;
  void findKey(KEY key,
	       KEY &map_key,
	       VALUE &value,
	       bool &exists) const;
  bool hasKey(KEY key) const;
  void eraseKey(KEY key);
  bool empty() const;
  void clear();
  void deleteContentsClear();
  void deleteArrayContentsClear();
  int longestBucketLength() const;
  Hash longestBucketHash() const;
  int bucketLength(Hash hash) const;

  void
  deleteContents()
  {
    Iterator iter(this);
    while (iter.hasNext())
      delete iter.next();
  }

  void
  deleteArrayContents()
  {
    Iterator iter(this);
    while (iter.hasNext())
      delete [] iter.next();
  }

  // Java style container itererator
  //  Map::Iterator<string *, Value, stringLess> iter(map);
  //  while (iter.hasNext()) {
  //    Value *v = iter.next();
  //  }
  class Iterator
  {
  public:
    Iterator() : container_(NULL) {}
    explicit Iterator(HashMap<KEY, VALUE, HASH, EQUAL> *container)
    { init(container); }
    explicit Iterator(HashMap<KEY, VALUE, HASH, EQUAL> &container)
    { init(container); }
    void init(HashMap<KEY, VALUE, HASH, EQUAL> *container)
    { container_ = container;
      hash_= 0;
      next_ = NULL;
      if (container_)
	findNext();
    }
    void init(HashMap<KEY, VALUE, HASH, EQUAL> &container)
    { container_ = &container;
      hash_= 0;
      next_ = NULL;
      if (container_)
	findNext();
    }
    bool hasNext() { return container_ && next_ != NULL; }
    VALUE next() { 
      HashMapBucket<KEY, VALUE> *next = next_;
      findNext();
      return next->value();
    }
    void next(KEY &key, VALUE &value) { 
      HashMapBucket<KEY, VALUE> *next = next_;
      findNext();
      key = next->key();
      value = next->value();
    }
    HashMap<KEY, VALUE, HASH, EQUAL> *container() { return container_; }

  private:
    void
    findNext()
    {
      if (next_)
	next_ = next_->next();
      while (next_ == NULL
	     && hash_ < container_->capacity())
	next_ = container_->table_[hash_++];
    }
    HashMap<KEY, VALUE, HASH, EQUAL> *container_;
    size_t hash_;
    HashMapBucket<KEY, VALUE> *next_;
  };

  class ConstIterator
  {
  public:
    ConstIterator() : container_(NULL) {}
    explicit ConstIterator(const HashMap<KEY, VALUE, HASH, EQUAL> *container)
    { init(container); }
    explicit ConstIterator(const HashMap<KEY, VALUE, HASH, EQUAL> &container)
    { init(container); }
    void init(const HashMap<KEY, VALUE, HASH, EQUAL> *container)
    { container_ =  container; hash_= 0; next_ = NULL; findNext(); }
    void init(HashMap<KEY, VALUE, HASH, EQUAL> &container)
    { container_ = &container; hash_= 0; next_ = NULL; findNext(); }
    bool hasNext() { return container_ && next_ != NULL; }
    VALUE next() {
      HashMapBucket<KEY, VALUE> *next = next_;
      findNext();
      return next->value();
    }
    void next(// Return values.
	      KEY &key,
	      VALUE &value) {
      HashMapBucket<KEY, VALUE> *next = next_;
      findNext();
      key = next->key();
      value = next->value();
    }
    HashMap<KEY, VALUE, HASH, EQUAL> *container() { return container_; }

  private:
    void
    findNext()
    {
      if (next_)
	next_ = next_->next();
      while (next_ == NULL
	     && hash_ < container_->capacity())
	next_ = container_->table_[hash_++];
    }
    const HashMap<KEY, VALUE, HASH, EQUAL> *container_;
    size_t hash_;
    HashMapBucket<KEY, VALUE> *next_;
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
  HashMapBucket<KEY, VALUE> **table_;
  HashMap *tmp_;
};

template <class KEY, class VALUE>
class HashMapBucket
{
public:
  HashMapBucket(KEY key,
		VALUE value,
		HashMapBucket *next);
  KEY key() const { return key_; }
  VALUE &value() { return value_; }
  void set(KEY key, VALUE value) { key_ = key; value_ = value; }
  HashMapBucket *next() const { return next_; }
  void setNext(HashMapBucket *next) { next_ = next; }

private:
  KEY key_;
  VALUE value_;
  HashMapBucket *next_;
};

template <class KEY, class VALUE>
HashMapBucket<KEY, VALUE>::HashMapBucket(KEY key,
					 VALUE value,
					 HashMapBucket *next) :
  key_(key),
  value_(value),
  next_(next)
{
}

////////////////////////////////////////////////////////////////

template <class KEY, class VALUE, class HASH, class EQUAL>
HashMap<KEY, VALUE, HASH, EQUAL>::HashMap() :
  capacity_(default_capacity),
  auto_resize_(true),
  hash_(HASH()),
  equal_(EQUAL())
{
  initTable();
}

template <class KEY, class VALUE, class HASH, class EQUAL>
HashMap<KEY, VALUE, HASH, EQUAL>::HashMap(size_t capacity,
					  bool auto_resize) :
  auto_resize_(auto_resize),
  capacity_(capacity),
  hash_(HASH()),
  equal_(EQUAL())
{
  initTable();
}

template <class KEY, class VALUE, class HASH, class EQUAL>
HashMap<KEY, VALUE, HASH, EQUAL>::HashMap(size_t capacity,
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

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::initTable()
{
  size_ = 0;
  tmp_ = NULL;
  table_ = new HashMapBucket<KEY, VALUE>*[capacity_];
  for (size_t i = 0; i < capacity_; i++)
    table_[i] = NULL;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
HashMap<KEY, VALUE, HASH, EQUAL>::~HashMap()
{
  for (size_t hash = 0; hash < capacity_; hash++) {
    HashMapBucket<KEY, VALUE> *next;
    for (HashMapBucket<KEY, VALUE> *bucket = table_[hash];
	 bucket;
	 bucket = next) {
      next = bucket->next();
      delete bucket;
    }
  }
  delete [] table_;
  delete tmp_;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
bool
HashMap<KEY, VALUE, HASH, EQUAL>::hasKey(KEY key) const
{
  size_t hash = hash_(key) % capacity_;
  HashMapBucket<KEY, VALUE> *head = table_[hash];
  for (HashMapBucket<KEY, VALUE> *bucket = head; bucket; bucket = bucket->next()) {
    if (equal_(bucket->key(), key)) {
      return true;
    }
  }
  return false;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
VALUE
HashMap<KEY, VALUE, HASH, EQUAL>::findKey(KEY key)
{
  size_t hash = hash_(key) % capacity_;
  HashMapBucket<KEY, VALUE> *head = table_[hash];
  for (HashMapBucket<KEY, VALUE> *bucket = head; bucket; bucket = bucket->next()) {
    KEY bucket_key = bucket->key();
    if (equal_(bucket_key, key)) {
      return bucket->value();
    }
  }
  return NULL;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
VALUE
HashMap<KEY, VALUE, HASH, EQUAL>::findKey(KEY key) const
{
  size_t hash = hash_(key) % capacity_;
  HashMapBucket<KEY, VALUE> *head = table_[hash];
  for (HashMapBucket<KEY, VALUE> *bucket = head; bucket; bucket = bucket->next()) {
    KEY bucket_key = bucket->key();
    if (equal_(bucket_key, key)) {
      return bucket->value();
    }
  }
  return NULL;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::findKey(KEY key,
					  VALUE &value,
					  bool &exists) const
{
  size_t hash = hash_(key) % capacity_;
  HashMapBucket<KEY, VALUE> *head = table_[hash];
  for (HashMapBucket<KEY, VALUE> *bucket = head; bucket; bucket = bucket->next()) {
    KEY bucket_key = bucket->key();
    if (equal_(bucket_key, key)) {
      value = bucket->value();
      exists = true;
      return;
    }
  }
  exists = false;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::findKey(KEY key,
					  KEY &map_key,
					  VALUE &value,
					  bool &exists) const
{
  size_t hash = hash_(key) % capacity_;
  HashMapBucket<KEY, VALUE> *head = table_[hash];
  for (HashMapBucket<KEY, VALUE> *bucket = head; bucket; bucket = bucket->next()) {
    KEY bucket_key = bucket->key();
    if (equal_(bucket_key, key)) {
      map_key = bucket->key();
      value = bucket->value();
      exists = true;
      return;
    }
  }
  exists = false;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::insert(KEY key,
					 VALUE value)
{
  size_t hash = hash_(key) % capacity_;
  HashMapBucket<KEY, VALUE> *head = table_[hash];
  for (HashMapBucket<KEY, VALUE> *bucket = head; bucket; bucket = bucket->next()) {
    if (equal_(bucket->key(), key)) {
      bucket->set(key, value);
      return;
    }
  }
  HashMapBucket<KEY, VALUE> *bucket = 
    new HashMapBucket<KEY, VALUE>(key, value, head);
  table_[hash] = bucket;
  size_++;
  if (size_ > capacity_
      && auto_resize_)
    resize(nextMersenne(capacity_));
}

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::resize(size_t capacity)
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
      tmp_ = new HashMap<KEY, VALUE, HASH, EQUAL>(capacity, auto_resize_,
						  hash_, equal_);
      for (size_t hash = 0; hash < capacity_; hash++) {
	for (HashMapBucket<KEY, VALUE> *bucket = table_[hash];
	     bucket;
	     bucket = bucket->next())
	  tmp_->insert(bucket->key(), bucket->value());
      }

      size_t prev_capacity = capacity_;
      HashMapBucket<KEY, VALUE> **prev_table = table_;

      // Switch over.
      table_ = tmp_->table_;
      capacity_ = capacity;

      tmp_->capacity_ = prev_capacity;
      tmp_->table_ = prev_table;
    }
  }
}

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::eraseKey(KEY key)
{
  size_t hash = hash_(key) % capacity_;
  HashMapBucket<KEY, VALUE> *head = table_[hash];
  HashMapBucket<KEY, VALUE> *prev = NULL;
  for (HashMapBucket<KEY, VALUE> *bucket = head; bucket; bucket = bucket->next()) {
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

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::deleteContentsClear()
{
  if (size_ > 0) {
    for (size_t hash = 0; hash < capacity_; hash++) {
      HashMapBucket<KEY, VALUE> *next;
      for (HashMapBucket<KEY, VALUE> *bucket = table_[hash];
	   bucket;
	   bucket = next) {
	delete bucket->value();
	next = bucket->next();
	delete bucket;
      }
      table_[hash] = NULL;
    }
    size_ = 0;
  }
}

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::deleteArrayContentsClear()
{
  if (size_ > 0) {
    for (size_t hash = 0; hash < capacity_; hash++) {
      HashMapBucket<KEY, VALUE> *next;
      for (HashMapBucket<KEY, VALUE> *bucket = table_[hash];
	   bucket;
	   bucket = next) {
	delete [] bucket->value();
	next = bucket->next();
	delete bucket;
      }
      table_[hash] = NULL;
    }
    size_ = 0;
  }
}

template <class KEY, class VALUE, class HASH, class EQUAL>
void
HashMap<KEY, VALUE, HASH, EQUAL>::clear()
{
  if (size_ > 0) {
    for (size_t hash = 0; hash < capacity_; hash++) {
      HashMapBucket<KEY, VALUE> *next;
      for (HashMapBucket<KEY, VALUE> *bucket = table_[hash];
	   bucket;
	   bucket = next) {
	next = bucket->next();
	delete bucket;
      }
      table_[hash] = NULL;
    }
    size_ = 0;
  }
}

template <class KEY, class VALUE, class HASH, class EQUAL>
bool
HashMap<KEY, VALUE, HASH, EQUAL>::empty() const
{
  return size_ == 0;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
int
HashMap<KEY, VALUE, HASH, EQUAL>::longestBucketLength() const
{
  return bucketLength(longestBucketHash());
}

template <class KEY, class VALUE, class HASH, class EQUAL>
Hash
HashMap<KEY, VALUE, HASH, EQUAL>::longestBucketHash() const
{
  int longest = 0;
  Hash longest_hash = 0;
  for (size_t hash = 0; hash < capacity_; hash++) {
    int length = bucketLength(hash);
    if (length > longest) {
      longest = length;
      longest_hash = hash;
    }
  }
  return longest_hash;
}

template <class KEY, class VALUE, class HASH, class EQUAL>
int
HashMap<KEY, VALUE, HASH, EQUAL>::bucketLength(Hash hash) const
{
  int length = 0;
  for (HashMapBucket<KEY, VALUE> *bucket = table_[hash];
       bucket;
       bucket = bucket->next())
    length++;
  return length;
}

} // namespace
#endif
