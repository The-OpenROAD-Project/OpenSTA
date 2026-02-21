// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

namespace sta {

// Java style container iterator.
//  Iterator<Object*> iter(container);
//  while (iter.hasNext()) {
//    Object *obj = iter.next();
//  }
template <class OBJ>
class Iterator
{
public:
  virtual ~Iterator() {}
  virtual bool hasNext() = 0;
  virtual OBJ next() = 0;
};

template <typename VECTOR_TYPE, typename OBJ_TYPE>
class VectorIterator : public Iterator<OBJ_TYPE>
{
public:
  VectorIterator(const VECTOR_TYPE *seq) :
    seq_(seq)
  {
    if (seq_)
      itr_ = seq_->begin();
  }
  VectorIterator(const VECTOR_TYPE &seq) :
    seq_(&seq),
    itr_(seq.begin())
  {
  }

  bool hasNext() { return seq_  && itr_ != seq_->end(); }
  OBJ_TYPE next() { return *itr_++; }

protected:
  const VECTOR_TYPE *seq_;
  VECTOR_TYPE::const_iterator itr_;
};

template <typename MAP_TYPE, typename OBJ_TYPE>
class MapIterator : public Iterator<OBJ_TYPE>
{
public:
  MapIterator(const MAP_TYPE *map) :
    map_(map)
  {
    if (map)
      itr_ = map->begin();
  }
  MapIterator(const MAP_TYPE &map) :
    map_(&map),
    itr_(map.begin())
  {
  }

  bool hasNext() { return map_ && itr_ != map_->end(); }
  OBJ_TYPE next() {
    OBJ_TYPE next = itr_->second;
    itr_++;
    return next;
  }

protected:
  const MAP_TYPE *map_;
  MAP_TYPE::const_iterator itr_;
};

template <typename SET_TYPE, typename OBJ_TYPE>
class SetIterator : public Iterator<OBJ_TYPE>
{
public:
  SetIterator(const SET_TYPE *set) :
    set_(set)
  {
    if (set)
      itr_ = set->begin();
  }
  SetIterator(const SET_TYPE &set) :
    set_(&set),
    itr_(set.begin())
  {
  }

  bool hasNext() { return set_ && itr_ != set_->end(); }
  OBJ_TYPE next() { return *itr_++; }

protected:
  const SET_TYPE *set_;
  SET_TYPE::const_iterator itr_;
};

} // namespace
