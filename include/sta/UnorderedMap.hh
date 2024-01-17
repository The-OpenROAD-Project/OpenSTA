// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include <cassert>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace sta {

// Add convenience functions around STL container.
template <class KEY, class VALUE, class HASH = std::hash<KEY>, class EQUAL = std::equal_to<KEY> >
class UnorderedMap : public std::unordered_map<KEY, VALUE, HASH, EQUAL>
{
public:
  UnorderedMap() :
    std::unordered_map<KEY, VALUE, HASH, EQUAL>()
  {
  }

  explicit UnorderedMap(const HASH &hash) :
    std::unordered_map<KEY, VALUE, HASH, EQUAL>(0, hash, std::equal_to<KEY>())
  {
  }

  explicit UnorderedMap(size_t size,
			const HASH &hash,
			const EQUAL &equal) :
    std::unordered_map<KEY, VALUE, HASH, EQUAL>(size, hash, equal)
  {
  }

  // Find out if key is in the set.
  bool
  hasKey(const KEY key) const
  {
    return this->find(key) != this->end();
  }

  // Find the value corresponding to key.
  VALUE
  findKey(const KEY key) const
  {
    auto find_iter = this->find(key);
    if (find_iter != this->end())
      return find_iter->second;
    else
      return nullptr;
  }
  void
  findKey(const KEY key,
	  // Return Values.
	  VALUE &value,
	  bool &exists) const
  {
    auto find_iter = this->find(key);
    if (find_iter != this->end()) {
      value = find_iter->second;
      exists = true;
    }
    else
      exists = false;
  }
  void
  findKey(const KEY &key,
	  // Return Values.
	  KEY &map_key,
	  VALUE &value,
	  bool &exists) const
  {
    auto find_iter = this->find(key);
    if (find_iter != this->end()) {
      map_key = find_iter->first;
      value = find_iter->second;
      exists = true;
    }
    else
      exists = false;
  }

  void
  insert(const KEY &key,
	 VALUE value)
  {
    this->operator[](key) = value;
  }

  void
  deleteContents()
  {
    Iterator iter(this);
    while (iter.hasNext())
      delete iter.next();
  }

  void
  deleteKeysContents()
  {
    Iterator iter(this);
    while (iter.hasNext()) {
      KEY key;
      VALUE value;
      iter.next(key, value);
      delete key;
      delete value;
    }
  }

  void
  deleteArrayContents()
  {
    Iterator iter(this);
    while (iter.hasNext())
      delete [] iter.next();
  }

  void
  deleteContentsClear()
  {
    deleteContents();
    std::unordered_map<KEY,VALUE,HASH,EQUAL>::clear();
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
    explicit Iterator(std::unordered_map<KEY,VALUE,HASH,EQUAL> *container) :
      container_(container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    explicit Iterator(std::unordered_map<KEY,VALUE,HASH,EQUAL> &container) :
      container_(&container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    void init(std::unordered_map<KEY,VALUE,HASH,EQUAL> *container)
    { container_ = container; if (container_ != nullptr) iter_=container_->begin();}
    void init(std::unordered_map<KEY,VALUE,HASH,EQUAL> &container)
    { container_ = &container; if (container_ != nullptr) iter_=container_->begin();}
    bool hasNext() { return container_ != nullptr && iter_ != container_->end(); }
    VALUE next() { return iter_++->second; }
    void next(KEY &key,
	      VALUE &value)
    { key = iter_->first; value = iter_->second; iter_++; }
    std::unordered_map<KEY,VALUE,HASH,EQUAL> *container() { return container_; }

  private:
    std::unordered_map<KEY,VALUE,HASH,EQUAL> *container_;
    typename std::unordered_map<KEY,VALUE,HASH,EQUAL>::iterator iter_;
  };

  class ConstIterator
  {
  public:
    ConstIterator() : container_(nullptr) {}
    explicit ConstIterator(const std::unordered_map<KEY,VALUE,HASH,EQUAL> *container) :
      container_(container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    explicit ConstIterator(const std::unordered_map<KEY,VALUE,HASH,EQUAL> &container) :
      container_(&container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    void init(const std::unordered_map<KEY,VALUE,HASH,EQUAL> *container)
    { container_ = container; if (container_ != nullptr) iter_=container_->begin();}
    void init(const std::unordered_map<KEY,VALUE,HASH,EQUAL> &container)
    { container_ = &container; if (container_ != nullptr) iter_=container_->begin();}
    bool hasNext() { return container_ != nullptr && iter_ != container_->end(); }
    VALUE next() { return iter_++->second; }
    void next(KEY &key,
	      VALUE &value)
    { key = iter_->first; value = iter_->second; iter_++; }
    const std::unordered_map<KEY,VALUE,HASH,EQUAL> *container() { return container_; }

  private:
    const std::unordered_map<KEY,VALUE,HASH,EQUAL> *container_;
    typename std::unordered_map<KEY,VALUE,HASH,EQUAL>::const_iterator iter_;
  };
};
  
template <class KEY, class VALUE, class EQUAL = std::equal_to<KEY> >
class MapVector
{
  struct elem_t
  {
    KEY key;
    VALUE value;
    bool valid = true;
    elem_t(KEY k, VALUE v)
      :key(k), value(v)
    {
    }
  };

  std::vector<elem_t> vec;
  EQUAL equal_obj;
  size_t erased = 0;

  auto find_vec(const KEY &key) const {
    return std::find_if(vec.begin(), vec.end(), [&key, this](const elem_t& elem) { return elem.valid && equal_obj(key, elem.key); });
  }

  auto find_vec(const KEY &key) {
    return std::find_if(vec.begin(), vec.end(), [&key, this](const elem_t& elem) { return elem.valid && equal_obj(key, elem.key); });
  }

public:
  MapVector()
  {
  }

  MapVector(size_t size,
	    const EQUAL &equal)
    :equal_obj(equal)
  {
    vec.reserve(size);
  }

  // Find out if key is in the set.
  bool
  hasKey(const KEY key) const
  {
    return this->find_vec(key) != vec.end();
  }

  // Find the value corresponding to key.
  VALUE
  findKey(const KEY key) const
  {
    auto find_iter = this->find_vec(key);
    if (find_iter != vec.end())
      return find_iter->value;
    else
      return nullptr;
  }
  void
  findKey(const KEY key,
	  // Return Values.
	  VALUE &value,
	  bool &exists) const
  {
    auto find_iter = this->find_vec(key);
    if (find_iter != vec.end()) {
      value = find_iter->value;
      exists = true;
    }
    else
      exists = false;
  }
  void
  findKey(const KEY &key,
	  // Return Values.
	  KEY &map_key,
	  VALUE &value,
	  bool &exists) const
  {
    auto find_iter = this->find_vec(key);
    if (find_iter != vec.end()) {
      map_key = find_iter->key;
      value = find_iter->value;
      exists = true;
    }
    else
      exists = false;
  }

  void
  insert(const KEY &key,
	 VALUE value)
  {
    auto find_iter = this->find_vec(key);
    if (find_iter != vec.end()) find_iter->value = value;
    else vec.push_back(elem_t(key, value));
  }

  size_t size() const
  {
    return vec.size() - erased;
  }

  bool empty() const
  {
    return size() == 0;
  }

  void clear()
  {
    vec.clear();
    erased = 0;
  }

  void erase(const KEY &key)
  {
    auto find_iter = this->find_vec(key);
    if (find_iter != vec.end())
      {
      find_iter->valid = false;
      erased++;
      }
  }
  
  class Iterator
  {
  public:
    Iterator() : container_(nullptr) {}
    explicit Iterator(MapVector<KEY, VALUE, EQUAL> *map)
    { init(map); }
    explicit Iterator(MapVector<KEY, VALUE, EQUAL> &map)
    { init(map); }
    void init(MapVector<KEY, VALUE, EQUAL> *map)
    {
      assert(map != nullptr);
      container_ = &map->vec;
      iter_ = container_->begin();
    }
    void init(MapVector<KEY, VALUE, EQUAL> &map)
    { container_ = &map.vec; iter_ = container_->begin(); }
    bool hasNext()
    {
      while (iter_ != container_->end() && !iter_->valid) iter_++;
      return iter_ != container_->end();
    }
    VALUE next() { return iter_++->value; }
    void next(KEY &key,
	      VALUE &value)
    { key = iter_->key; value = iter_->value; iter_++; }
    std::vector<elem_t> *container() { return container_; }

  private:
    std::vector<elem_t> *container_;
    typename std::vector<elem_t>::iterator iter_;
  };

  class ConstIterator
  {
  public:
    ConstIterator() : container_(nullptr) {}
    explicit ConstIterator(const MapVector<KEY, VALUE, EQUAL> *map)
    { init(map); }
    explicit ConstIterator(const MapVector<KEY, VALUE, EQUAL> &map)
    { init(map); }
    void init(const MapVector<KEY, VALUE, EQUAL> *map)
    {
      assert(map != nullptr);
      container_ = &map->vec;
      iter_ = container_->begin();
    }
    void init(const MapVector<KEY, VALUE, EQUAL> &map)
    { container_ = &map.vec; iter_ = container_->begin(); }
    bool hasNext()
    {
      while (iter_ != container_->end() && !iter_->valid) iter_++;
      return iter_ != container_->end();
    }
    VALUE next() { return iter_++->value; }
    void next(KEY &key,
	      VALUE &value)
    { key = iter_->key; value = iter_->value; iter_++; }
    const std::vector<elem_t> *container() { return container_; }

  private:
    const std::vector<elem_t> *container_;
    typename std::vector<elem_t>::const_iterator iter_;
  };
};

} // namespace
