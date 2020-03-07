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

#include <vector>
#include <algorithm>

namespace sta {

// Add convenience functions around STL container.
template <class OBJ>
class Vector : public std::vector<OBJ>
{
public:
  Vector() : std::vector<OBJ>() {}
  Vector(size_t n) : std::vector<OBJ>(n) {}
  Vector(size_t n, const OBJ &obj) : std::vector<OBJ>(n, obj) {}

  // Erase an object from the vector (slow).
  void
  eraseObject(OBJ obj)
  {
    auto find_iter = std::find(this->begin(), this->end(), obj);
    if (find_iter != this->end())
      this->erase(find_iter);
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
    this->clear();
  }

  void
  deleteArrayContentsClear()
  {
    Iterator iter(this);
    while (iter.hasNext())
      delete [] iter.next();
    this->clear();
  }

  // Java style container itererator
  //  Vector::Iterator<Object*> iter(vector);
  //  while (iter.hasNext()) {
  //    Object *v = iter.next();
  //  }
  class Iterator
  {
  public:
    Iterator() : container_(nullptr) {}
    Iterator(std::vector<OBJ> *container) :
      container_(container)
    { if (container) iter_ = container->begin(); }
    Iterator(std::vector<OBJ> &container) :
      container_(&container)
    { if (container_) iter_ = container_->begin(); }
    void init() { iter_ = container_->begin(); }
    void init(std::vector<OBJ> *container)
    { container_ = container; if (container_) iter_=container_->begin(); }
    void init(std::vector<OBJ> &container)
    { container_ = &container; iter_ = container_->begin(); }
    bool hasNext() { return container_ && iter_ != container_->end(); }
    OBJ& next() { return *iter_++; }
    std::vector<OBJ> *container() { return container_; }

  private:
    std::vector<OBJ> *container_;
    typename std::vector<OBJ>::iterator iter_;
  };

  class ConstIterator
  {
  public:
    ConstIterator() : container_(nullptr) {}
    ConstIterator(const std::vector<OBJ> *container) :
      container_(container)
    { if (container_) iter_ = container_->begin(); }
    ConstIterator(const std::vector<OBJ> &container) :
      container_(&container)
    { iter_ = container_->begin(); }
    void init() { iter_ = container_->begin(); }
    void init(const std::vector<OBJ> *container)
    { container_ = container; if (container_) iter_=container_->begin();}
    void init(const std::vector<OBJ> &container)
    { container_ = &container; if (container_) iter_=container_->begin();}
    bool hasNext() { return container_ && iter_ != container_->end(); }
    const OBJ& next() { return *iter_++; }
    const std::vector<OBJ> *container() { return container_; }

  private:
    const std::vector<OBJ> *container_;
    typename std::vector<OBJ>::const_iterator iter_;
  };
};

template <class OBJ, class SortCmp>
void
sort(Vector<OBJ> &seq, SortCmp cmp)
{
  // For some strange reason std::sort goes off into never never land
  // when optimization is turned on in gcc.
  std::stable_sort(seq.begin(), seq.end(), cmp);
}

template <class OBJ, class SortCmp>
void
sort(Vector<OBJ> *seq, SortCmp cmp)
{
  // For some strange reason std::sort goes off into never never land
  // when optimization is turned on in gcc.
  std::stable_sort(seq->begin(), seq->end(), cmp);
}

} // namespace sta
