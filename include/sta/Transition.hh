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

#include <array>
#include <vector>

#include "Iterator.hh"
#include "Map.hh"
#include "StringUtil.hh"

namespace sta {

class Transition;
class RiseFall;
class RiseFallBoth;

typedef Map<const std::string, const Transition*> TransitionMap;

// Rise/fall transition.
class RiseFall
{
public:
  // Singleton accessors.
  static const RiseFall *rise() { return &rise_; }
  static const RiseFall *fall() { return &fall_; }
  static int riseIndex() { return rise_.sdf_triple_index_; }
  static int fallIndex() { return fall_.sdf_triple_index_; }
  const std::string &to_string() const { return short_name_; }
  const char *name() const { return name_.c_str(); }
  const char *shortName() const { return short_name_.c_str(); }
  int index() const { return sdf_triple_index_; }
  const RiseFallBoth *asRiseFallBoth();
  const RiseFallBoth *asRiseFallBoth() const;
  const Transition *asTransition() const;
  // Find transition corresponding to rf_str.
  static const RiseFall *find(const char *rf_str);
  // Find transition from index.
  static const RiseFall *find(int index);
  const RiseFall *opposite() const;

  // for range support.
  // for (auto rf : RiseFall::range()) {}
  static const std::array<const RiseFall*, 2> &range() { return range_; }
  // for (auto rf_index : RiseFall::rangeIndex()) {}
  static const std::array<int, 2> &rangeIndex() { return range_index_; }
  static const int index_count = 2;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 1;

protected:
  RiseFall(const char *name,
           const char *short_name,
           int sdf_triple_index);

  const std::string name_;
  const std::string short_name_;
  const int sdf_triple_index_;

  static const RiseFall rise_;
  static const RiseFall fall_;
  static const std::array<const RiseFall*, 2> range_;
  static const std::array<int, 2> range_index_;
};

// Rise/fall/risefall transition.
class RiseFallBoth
{
public:
  // Singleton accessors.
  static const RiseFallBoth *rise() { return &rise_; }
  static const RiseFallBoth *fall() { return &fall_; }
  static const RiseFallBoth *riseFall() { return &rise_fall_; }
  const std::string &to_string() const { return short_name_; }
  const char *name() const { return name_.c_str(); }
  const char *shortName() const { return short_name_.c_str(); }
  int index() const { return sdf_triple_index_; }
  bool matches(const RiseFall *rf) const;
  bool matches(const Transition *tr) const;
  const RiseFall *asRiseFall() const { return as_rise_fall_; }
  // Find transition corresponding to string.
  static const RiseFallBoth *find(const char *tr_str);
  // for (const auto rf : rf->range()) {}
  const std::vector<const RiseFall*> &range() const { return range_; }
  // for (const auto rf_index : rf->rangeIndex()) {}
  const std::vector<int> &rangeIndex() const { return range_index_; }

  static const int index_count = 3;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 2;

protected:
  RiseFallBoth(const char *name,
               const char *short_name,
               int sdf_triple_index,
               const RiseFall *as_rise_fall,
               std::vector<const RiseFall*> range,
               std::vector<int> range_index);

  const std::string name_;
  const std::string short_name_;
  const int sdf_triple_index_;
  const RiseFall *as_rise_fall_;
  const std::vector<const RiseFall*> range_;
  const std::vector<int> range_index_;

  static const RiseFallBoth rise_;
  static const RiseFallBoth fall_;
  static const RiseFallBoth rise_fall_;
};

// General SDF transition.
class Transition
{
public:
  // Singleton accessors.
  static const Transition *rise() { return &rise_; }
  static const Transition *fall() { return &fall_; }
  static const Transition *tr0Z() { return &tr_0Z_; }
  static const Transition *trZ1() { return &tr_Z1_; }
  static const Transition *tr1Z() { return &tr_1Z_; }
  static const Transition *trZ0() { return &tr_Z0_; }
  static const Transition *tr0X() { return &tr_0X_; }
  static const Transition *trX1() { return &tr_X1_; }
  static const Transition *tr1X() { return &tr_1X_; }
  static const Transition *trX0() { return &tr_X0_; }
  static const Transition *trXZ() { return &tr_XZ_; }
  static const Transition *trZX() { return &tr_ZX_; }
  // Matches rise and fall.
  static const Transition *riseFall() { return &rise_fall_; }
  const std::string &to_string() const { return name_; }
  // As initial/final value pair.
  const char *asInitFinalString() const { return init_final_.c_str(); }
  int sdfTripleIndex() const { return sdf_triple_index_; }
  int index() const { return sdf_triple_index_; }
  const RiseFall *asRiseFall() const { return as_rise_fall_; }
  const RiseFallBoth *asRiseFallBoth() const;
  bool matches(const Transition *tr) const;
  // Find transition corresponding to string.
  static const Transition *find(const char *tr_str);
  static int maxIndex() { return max_index_; }

private:
  Transition(const char *name,
	     const char *init_final,
	     const RiseFall *as_rise_fall,
	     int sdf_triple_index);

  const std::string name_;
  const std::string init_final_;
  const RiseFall *as_rise_fall_;
  const int sdf_triple_index_;

  static const Transition rise_;
  static const Transition fall_;
  static const Transition tr_0Z_;
  static const Transition tr_Z1_;
  static const Transition tr_1Z_;
  static const Transition tr_Z0_;
  static const Transition tr_0X_;
  static const Transition tr_X1_;
  static const Transition tr_1X_;
  static const Transition tr_X0_;
  static const Transition tr_XZ_;
  static const Transition tr_ZX_;
  static const Transition rise_fall_;
  static const int index_count = 13;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 4;

  static TransitionMap transition_map_;
  static int max_index_;
};

} // namespace
