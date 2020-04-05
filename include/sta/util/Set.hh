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

#include <set>
#include <math.h>
#include <algorithm>

namespace sta {

// Add convenience functions around STL container.
template <class KEY, class CMP = std::less<KEY> >
class Set : public std::set<KEY, CMP>
{
public:
  Set() : std::set<KEY, CMP>() {}
  explicit Set(const CMP &cmp) : std::set<KEY, CMP>(cmp) {}

    // Find the entry corresponding to key.
  KEY findKey(const KEY key) const
  {
    auto find_iter = this->find(key);
    if (find_iter != this->end())
      return *find_iter;
    else
      return nullptr;
  }
  // Find out if key is in the set.
  bool hasKey(const KEY key) const
  {
    auto find_iter = this->find(key);
    return find_iter != this->end();
  }

  // Slowaris STL doesn't support operator== on sets.
  static bool equal(const std::set<KEY, CMP> *set1,
		    const std::set<KEY, CMP> *set2);

  // True if set2 is a subset of this set.
  bool isSubset(const std::set<KEY, CMP> *set2);

  void insertSet(const std::set<KEY, CMP> *set2);

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
    this->clear();
  }

  static bool
  intersects(std::set<KEY, CMP> &set1,
	     std::set<KEY, CMP> &set2);
  static bool
  intersects(std::set<KEY, CMP> *set1,
	     std::set<KEY, CMP> *set2);

  // Java style container itererator
  //  Set::Iterator<Key*> iter(set);
  //  while (iter.hasNext()) {
  //    Key *v = iter.next();
  //  }
  class Iterator
  {
  public:
    Iterator() : container_(nullptr) {}
    explicit Iterator(std::set<KEY, CMP> *container) :
      container_(container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    explicit Iterator(std::set<KEY, CMP> &container) :
      container_(&container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    void init(std::set<KEY, CMP> *container)
    { container_ = container; if (container_ != nullptr) iter_=container_->begin();}
    void init(std::set<KEY, CMP> &container)
    { container_ = &container; if (container_ != nullptr) iter_=container_->begin();}
    bool hasNext() { return container_ != nullptr && iter_ != container_->end(); }
    KEY next() { return *iter_++; }
    std::set<KEY, CMP> *container() { return container_; }

  private:
    std::set<KEY, CMP> *container_;
    typename std::set<KEY, CMP>::iterator iter_;
  };

  class ConstIterator
  {
  public:
    ConstIterator() : container_(nullptr) {}
    explicit ConstIterator(const std::set<KEY, CMP> *container) :
      container_(container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    explicit ConstIterator(const std::set<KEY, CMP> &container) :
      container_(&container)
    { if (container_ != nullptr) iter_ = container_->begin(); }
    void init(const std::set<KEY, CMP> *container)
    { container_ = container; if (container_ != nullptr) iter_=container_->begin();}
    void init(const std::set<KEY, CMP> &container)
    { container_ = &container; if (container_ != nullptr) iter_=container_->begin();}

    bool hasNext() { return container_ != nullptr && iter_ != container_->end(); }
    KEY next() { return *iter_++; }
    const std::set<KEY, CMP> *container() { return container_; }

  private:
    const std::set<KEY, CMP> *container_;
    typename std::set<KEY, CMP>::const_iterator iter_;
  };
};

template <class KEY, class CMP>
bool
Set<KEY, CMP>::equal(const std::set<KEY, CMP> *set1,
		     const std::set<KEY, CMP> *set2)
{
  if ((set1 == nullptr || set1->empty())
      && (set2 == nullptr || set2->empty()))
    return true;
  else if (set1 && set2) {
    if (set1->size() == set2->size()) {
      typename Set<KEY, CMP>::ConstIterator iter1(set1);
      typename Set<KEY, CMP>::ConstIterator iter2(set2);
      while (iter1.hasNext() && iter2.hasNext()) {
	if (iter1.next() != iter2.next())
	  return false;
      }
      return true;
    }
    else
      return false;
  }
  else
    return false;
}

template <class KEY, class CMP>
bool
Set<KEY, CMP>::isSubset(const std::set<KEY, CMP> *set2)
{
  if (this->empty() && set2->empty())
    return true;
  else {
    typename Set<KEY, CMP>::ConstIterator iter2(set2);
    while (iter2.hasNext()) {
      const KEY key2 = iter2.next();
      if (!hasKey(key2))
	return false;
    }
    return true;
  }
}

template <class KEY, class CMP>
bool
Set<KEY, CMP>::intersects(std::set<KEY, CMP> &set1,
			  std::set<KEY, CMP> &set2)
{
  return intersects(&set1, &set2);
}

template <class KEY, class CMP>
bool
Set<KEY, CMP>::intersects(std::set<KEY, CMP> *set1,
			  std::set<KEY, CMP> *set2)
{
  if (set1 && !set1->empty()
      && set2 && !set2->empty()) {
    const std::set<KEY, CMP> *small = set1;
    const std::set<KEY, CMP> *big = set2;
    if (small->size() > big->size()) {
      small = set2;
      big = set1;
    }
    auto iter1 = big->begin();
    auto last1 = big->end();
    auto iter2 = small->begin();
    auto last2 = small->end();
    if (static_cast<float>(small->size() + big->size()) < (small->size() * log(static_cast<float>(big->size())))) {
      while (iter1 != last1 && iter2 != last2) {
        if (*iter1 < *iter2)
          ++iter1;
        else if (*iter2 < *iter1)
          ++iter2;
        else
          return true;
      }
    }
    else {
      for (/* empty */; iter2 != last2; ++iter2) {
        const KEY key2 = *iter2;
        if (big->find(key2) != last1)
          return true;
      }
    }
  }
  return false;
}

// A complicated way to call the base class operator<.
template <class KEY, class CMP>
bool
operator<(const Set<KEY, CMP> &set1, const Set<KEY, CMP> &set2)
{
  const std::set<KEY, CMP> &set1_base = set1;
  const std::set<KEY, CMP> &set2_base = set2;
  return set1_base < set2_base;
}

template <class KEY, class CMP>
void 
Set<KEY, CMP>::insertSet(const std::set<KEY, CMP> *set2)
{
  if (set2)
    this->insert(set2->begin(), set2->end());
}

} // namespace
