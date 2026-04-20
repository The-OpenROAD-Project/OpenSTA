// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include "Transition.hh"

#include <algorithm>

#include "ContainerHelpers.hh"

namespace sta {

const RiseFall RiseFall::rise_("rise", "^", 0);
const RiseFall RiseFall::fall_("fall", "v", 1);
const std::array<const RiseFall*, 2> RiseFall::range_{&rise_, &fall_};
const std::array<size_t, 2> RiseFall::range_index_{rise_.index(), fall_.index()};

RiseFall::RiseFall(std::string_view name,
                   std::string_view  short_name,
                   size_t sdf_triple_index) :
  name_(name),
  short_name_(short_name),
  sdf_triple_index_(sdf_triple_index)
{
}

const std::string&
RiseFall::to_string(bool use_short) const
{
  if (use_short)
    return short_name_;
  else
    return name_;
}

const RiseFall *
RiseFall::opposite() const
{
  if (this == &rise_)
    return &fall_;
  else
    return &rise_;
}

const RiseFall *
RiseFall::find(std::string_view rf_name)
{
  if (rf_name == rise_.name() || rf_name == rise_.shortName())
    return &rise_;
  if (rf_name == fall_.name() || rf_name == fall_.shortName())
    return &fall_;
  return nullptr;
}

const RiseFall *
RiseFall::find(size_t index)
{
  if (index == rise_.index())
    return &rise_;
  else
    return &fall_;
}

const RiseFallBoth *
RiseFall::asRiseFallBoth()
{
  if (this == &rise_)
    return RiseFallBoth::rise();
  else
    return RiseFallBoth::fall();
}

const RiseFallBoth *
RiseFall::asRiseFallBoth() const
{
  if (this == &rise_)
    return RiseFallBoth::rise();
  else
    return RiseFallBoth::fall();
}

const Transition *
RiseFall::asTransition() const
{
  if (this == &rise_)
    return Transition::rise();
  else
    return Transition::fall();
}

////////////////////////////////////////////////////////////////

const RiseFallBoth RiseFallBoth::rise_("rise", "^", 0,
                                       RiseFall::rise(),
                                       {RiseFall::rise()},
                                       {RiseFall::riseIndex()});
const RiseFallBoth RiseFallBoth::fall_("fall", "v", 1,
                                       RiseFall::fall(),
                                       {RiseFall::fall()},
                                       {RiseFall::fallIndex()});
const RiseFallBoth RiseFallBoth::rise_fall_("rise_fall", "rf", 2,
                                            nullptr,
                                            {RiseFall::rise(),
                                             RiseFall::fall()},
                                            {RiseFall::riseIndex(),
                                             RiseFall::fallIndex()});

RiseFallBoth::RiseFallBoth(std::string_view name,
                           std::string_view short_name,
                           size_t sdf_triple_index,
                           const RiseFall *as_rise_fall,
                           const std::vector<const RiseFall*> &range,
                           const std::vector<size_t> &range_index) :
  name_(name),
  short_name_(short_name),
  sdf_triple_index_(sdf_triple_index),
  as_rise_fall_(as_rise_fall),
  range_(range),
  range_index_(range_index)
{
}

const std::string&
RiseFallBoth::to_string(bool use_short) const
{
  if (use_short)
    return short_name_;
  else
    return name_;
}

const RiseFallBoth *
RiseFallBoth::find(std::string_view rf_name)
{
  if (rf_name == rise_.name())
    return &rise_;
  if (rf_name == fall_.name())
    return &fall_;
  if (rf_name == rise_fall_.name())
    return &rise_fall_;
  return nullptr;
}

bool
RiseFallBoth::matches(const RiseFall *rf) const
{
  return this == &rise_fall_
    || as_rise_fall_ == rf;
}

bool
RiseFallBoth::matches(const Transition *tr) const
{
  return this == &rise_fall_
    || (this == &rise_
        && tr == Transition::rise())
    || (this == &fall_
        && tr == Transition::fall());
}

////////////////////////////////////////////////////////////////

TransitionMap Transition::transition_map_;
size_t Transition::max_index_ = 0;

// Sdf triple order defined on Sdf 3.0 spec, pg 3-17.
const Transition Transition::rise_{  "^", "01", RiseFall::rise(),  0};
const Transition Transition::fall_ { "v", "10", RiseFall::fall(),  1};
const Transition Transition::tr_0Z_{"0Z", "0Z", RiseFall::rise(),  2};
const Transition Transition::tr_Z1_{"Z1", "Z1", RiseFall::rise(),  3};
const Transition Transition::tr_1Z_{"1Z", "1Z", RiseFall::fall(),  4};
const Transition Transition::tr_Z0_{"Z0", "Z0", RiseFall::fall(),  5};
const Transition Transition::tr_0X_{"0X", "0X", RiseFall::rise(),  6};
const Transition Transition::tr_X1_{"X1", "X1", RiseFall::rise(),  7};
const Transition Transition::tr_1X_{"1X", "1X", RiseFall::fall(),  8};
const Transition Transition::tr_X0_{"X0", "X0", RiseFall::fall(),  9};
const Transition Transition::tr_XZ_{"XZ", "XZ", nullptr,          10};
const Transition Transition::tr_ZX_{"ZX", "ZX", nullptr,          11};
const Transition Transition::rise_fall_{"*", "**", nullptr,       12};

Transition::Transition(std::string_view name,
                       std::string_view init_final,
                       const RiseFall *as_rise_fall,
                       size_t sdf_triple_index) :
  name_(name),
  init_final_(init_final),
  as_rise_fall_(as_rise_fall),
  sdf_triple_index_(sdf_triple_index)
{
  transition_map_[name_] = this;
  transition_map_[init_final_] = this;
  max_index_ = std::max(sdf_triple_index, max_index_);
}

bool
Transition::matches(const Transition *tr) const
{
  return this == riseFall() || tr == this;
}

const Transition *
Transition::find(std::string_view tr_name)
{
  return findStringKey(transition_map_, tr_name);
}

const RiseFallBoth *
Transition::asRiseFallBoth() const
{
  return reinterpret_cast<const RiseFallBoth*>(as_rise_fall_);
}

} // namespace sta
