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

#include <unordered_set>
#include <algorithm>

namespace sta {

// Add convenience functions around STL container.
template <class KEY, class HASH = std::hash<KEY>, class EQUAL = std::equal_to<KEY> >
class UnorderedSet : public std::unordered_set<KEY, HASH, EQUAL>
{
public:
  UnorderedSet() :
    std::unordered_set<KEY, HASH, EQUAL>()
  {
  }

  explicit UnorderedSet(size_t size,
			const HASH &hash,
			const EQUAL &equal) :
    std::unordered_set<KEY, HASH, EQUAL>(size, hash, equal)
  {
  }

  // Find out if key is in the set.
  bool
  hasKey(const KEY key) const
  {
    return this->find(key) != this->end();
  }

  // Find the value corresponding to key.
  KEY
  findKey(const KEY key) const
  {
    auto find_iter = this->find(key);
    if (find_iter != this->end())
      return *find_iter;
    else
      return nullptr;
  }

  void
  deleteContents()
  {
    Iterator iter(this);
    while (iter.hasNext())
      delete iter.next();
  }

  void
  deleteContentsClear()
  {
    deleteContents();
    std::unordered_set<KEY,HASH,EQUAL>::clear();
  }

  // Java style container itererator
  //  Set::Iterator<string *, Value, stringLess> iter(set);
  //  while (iter.hasNext()) {
  //    Value *v = iter.next();
  //  }
  class Iterator
  {
  public:
    Iterator() : container_(nullptr) {}
    explicit Iterator(std::unordered_set<KEY,HASH,EQUAL> *container) :
      container_(container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    explicit Iterator(std::unordered_set<KEY,HASH,EQUAL> &container) :
      container_(&container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    void init(std::unordered_set<KEY,HASH,EQUAL> *container)
    { container_ = container; if (container_ != nullptr) iter_=container_->begin();}
    void init(std::unordered_set<KEY,HASH,EQUAL> &container)
    { container_ = &container; if (container_ != nullptr) iter_=container_->begin();}
    bool hasNext() { return container_ != nullptr && iter_ != container_->end(); }
    KEY next() { return *iter_++; }
    std::unordered_set<KEY,HASH,EQUAL> *container() { return container_; }

  private:
    std::unordered_set<KEY,HASH,EQUAL> *container_;
    typename std::unordered_set<KEY,HASH,EQUAL>::iterator iter_;
  };

  class ConstIterator
  {
  public:
    ConstIterator() : container_(nullptr) {}
    explicit ConstIterator(const std::unordered_set<KEY,HASH,EQUAL> *container) :
      container_(container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    explicit ConstIterator(const std::unordered_set<KEY,HASH,EQUAL> &container) :
      container_(&container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    void init(const std::unordered_set<KEY,HASH,EQUAL> *container)
    { container_ = container; if (container_ != nullptr) iter_=container_->begin();}
    void init(const std::unordered_set<KEY,HASH,EQUAL> &container)
    { container_ = &container; if (container_ != nullptr) iter_=container_->begin();}
    bool hasNext() { return container_ != nullptr && iter_ != container_->end(); }
    KEY next() { return iter_++->second; }
    const std::unordered_set<KEY,HASH,EQUAL> *container() { return container_; }

  private:
    const std::unordered_set<KEY,HASH,EQUAL> *container_;
    typename std::unordered_set<KEY,HASH,EQUAL>::const_iterator iter_;
  };
};

} // namespace
