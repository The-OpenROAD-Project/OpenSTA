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

#include <array>
#include <vector>
#include "DisallowCopyAssign.hh"
#include "Iterator.hh"
#include "Map.hh"
#include "StringUtil.hh"

namespace sta {

class Transition;
class RiseFall;
class RiseFallBoth;

typedef Map<const char*, Transition*, CharPtrLess> TransitionMap;

// Rise/fall transition.
class RiseFall
{
public:
  // Singleton accessors.
  static RiseFall *rise() { return &rise_; }
  static RiseFall *fall() { return &fall_; }
  static int riseIndex() { return rise_.sdf_triple_index_; }
  static int fallIndex() { return fall_.sdf_triple_index_; }
  const char *asString() const { return short_name_; }
  const char *name() const { return name_; }
  const char *shortName() const { return short_name_; }
  void setShortName(const char *short_name);
  int index() const { return sdf_triple_index_; }
  RiseFallBoth *asRiseFallBoth();
  const RiseFallBoth *asRiseFallBoth() const;
  Transition *asTransition() const;
  // Find transition corresponding to tr_str.
  static RiseFall *find(const char *tr_str);
  // Find transition from index.
  static RiseFall *find(int index);
  RiseFall *opposite() const;

  // for range support.
  // for (auto tr : RiseFall::range()) {}
  static const std::array<RiseFall*, 2> &range() { return range_; }
  // for (auto tr_index : RiseFall::rangeIndex()) {}
  static const std::array<int, 2> &rangeIndex() { return range_index_; }
  static const int index_count = 2;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 1;

protected:
  RiseFall(const char *name,
		const char *short_name,
		int sdf_triple_index);
  ~RiseFall();

  const char *name_;
  const char *short_name_;
  const int sdf_triple_index_;

  static RiseFall rise_;
  static RiseFall fall_;
  static const std::array<RiseFall*, 2> range_;
  static const std::array<int, 2> range_index_;

private:
  DISALLOW_COPY_AND_ASSIGN(RiseFall);
};

// Rise/fall/risefall transition.
class RiseFallBoth
{
public:
  // Singleton accessors.
  static RiseFallBoth *rise() { return &rise_; }
  static RiseFallBoth *fall() { return &fall_; }
  static RiseFallBoth *riseFall() { return &rise_fall_; }
  const char *asString() const { return short_name_; }
  const char *name() const { return name_; }
  const char *shortName() const { return short_name_; }
  void setShortName(const char *short_name);
  int index() const { return sdf_triple_index_; }
  bool matches(const RiseFall *rf) const;
  bool matches(const Transition *tr) const;
  RiseFall *asRiseFall() const { return as_rise_fall_; }
  // Find transition corresponding to string.
  static RiseFallBoth *find(const char *tr_str);
  // for (auto tr : min_max->range()) {}
  const std::vector<RiseFall*> &range() const { return range_; }
  // for (auto tr_index : min_max->rangeIndex()) {}
  const std::vector<int> &rangeIndex() const { return range_index_; }

  static const int index_count = 3;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 2;

protected:
  RiseFallBoth(const char *name,
		    const char *short_name,
		    int sdf_triple_index,
		    RiseFall *as_rise_fall,
		    std::vector<RiseFall*> range,
		    std::vector<int> range_index);
  ~RiseFallBoth();

  const char *name_;
  const char *short_name_;
  const int sdf_triple_index_;
  RiseFall *as_rise_fall_;
  const std::vector<RiseFall*> range_;
  const std::vector<int> range_index_;

  static RiseFallBoth rise_;
  static RiseFallBoth fall_;
  static RiseFallBoth rise_fall_;

private:
  DISALLOW_COPY_AND_ASSIGN(RiseFallBoth);
};

// General SDF transition.
class Transition
{
public:
  // Singleton accessors.
  static Transition *rise() { return &rise_; }
  static Transition *fall() { return &fall_; }
  static Transition *tr0Z() { return &tr_0Z_; }
  static Transition *trZ1() { return &tr_Z1_; }
  static Transition *tr1Z() { return &tr_1Z_; }
  static Transition *trZ0() { return &tr_Z0_; }
  static Transition *tr0X() { return &tr_0X_; }
  static Transition *trX1() { return &tr_X1_; }
  static Transition *tr1X() { return &tr_1X_; }
  static Transition *trX0() { return &tr_X0_; }
  static Transition *trXZ() { return &tr_XZ_; }
  static Transition *trZX() { return &tr_ZX_; }
  void setName(const char *name);
  // Matches rise and fall.
  static Transition *riseFall() { return &rise_fall_; }
  const char *asString() const { return name_; }
  // As initial/final value pair.
  const char *asInitFinalString() const { return init_final_; }
  int sdfTripleIndex() const { return sdf_triple_index_; }
  int index() const { return sdf_triple_index_; }
  RiseFall *asRiseFall() const { return as_rise_fall_; }
  const RiseFallBoth *asRiseFallBoth() const;
  bool matches(const Transition *tr) const;
  // Find transition corresponding to string.
  static Transition *find(const char *tr_str);
  static int maxIndex() { return max_index_; }

private:
  Transition(const char *name,
	     const char *init_final,
	     RiseFall *as_rise_fall,
	     int sdf_triple_index);
  ~Transition();

  const char *name_;
  const char *init_final_;
  RiseFall *as_rise_fall_;
  const int sdf_triple_index_;

  static Transition rise_;
  static Transition fall_;
  static Transition tr_0Z_;
  static Transition tr_Z1_;
  static Transition tr_1Z_;
  static Transition tr_Z0_;
  static Transition tr_0X_;
  static Transition tr_X1_;
  static Transition tr_1X_;
  static Transition tr_X0_;
  static Transition tr_XZ_;
  static Transition tr_ZX_;
  static Transition rise_fall_;
  static const int index_count = 13;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 4;

  static TransitionMap transition_map_;
  static int max_index_;

private:
  DISALLOW_COPY_AND_ASSIGN(Transition);
};

// Obsolete. Use range iteration instead.
// for (auto tr : RiseFall::range()) {}
class RiseFallIterator : public Iterator<RiseFall*>
{
public:
  RiseFallIterator() : index_(0), index_max_(RiseFall::index_max) {}
  explicit RiseFallIterator(const RiseFallBoth *rf);
  void init();
  virtual bool hasNext();
  virtual RiseFall *next();

private:
  DISALLOW_COPY_AND_ASSIGN(RiseFallIterator);

  int index_;
  int index_max_;
};

} // namespace
