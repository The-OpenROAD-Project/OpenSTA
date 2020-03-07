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

#include "Machine.hh"
#include "Transition.hh"

namespace sta {

using std::max;

RiseFall RiseFall::rise_("rise", "^", 0);
RiseFall RiseFall::fall_("fall", "v", 1);
const std::array<RiseFall*, 2> RiseFall::range_{&rise_, &fall_};
const std::array<int, 2> RiseFall::range_index_{rise_.index(), fall_.index()};

RiseFall::RiseFall(const char *name,
			     const char *short_name,
			     int sdf_triple_index) :
  name_(name),
  short_name_(stringCopy(short_name)),
  sdf_triple_index_(sdf_triple_index)
{
}

RiseFall::~RiseFall()
{
  stringDelete(short_name_);
}

void
RiseFall::setShortName(const char *short_name)
{
  stringDelete(short_name_);
  short_name_ = stringCopy(short_name);
}

RiseFall *
RiseFall::opposite() const
{
  if (this == &rise_)
    return &fall_;
  else
    return &rise_;
}

RiseFall *
RiseFall::find(const char *tr_str)
{
  if (stringEq(tr_str, rise_.name()))
    return &rise_;
  else if (stringEq(tr_str, fall_.name()))
    return &fall_;
  else
    return nullptr;
}

RiseFall *
RiseFall::find(int index)
{
  if (index == rise_.index())
    return &rise_;
  else
    return &fall_;
}

RiseFallBoth *
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

Transition *
RiseFall::asTransition() const
{
  if (this == &rise_)
    return Transition::rise();
  else
    return Transition::fall();
}

////////////////////////////////////////////////////////////////

RiseFallBoth RiseFallBoth::rise_("rise", "^", 0,
					   RiseFall::rise(),
					   {RiseFall::rise()},
					   {RiseFall::riseIndex()});
RiseFallBoth RiseFallBoth::fall_("fall", "v", 1,
					   RiseFall::fall(),
					   {RiseFall::fall()},
					   {RiseFall::fallIndex()});
RiseFallBoth RiseFallBoth::rise_fall_("rise_fall", "rf", 2,
						nullptr,
						{RiseFall::rise(),
						 RiseFall::fall()},
						{RiseFall::riseIndex(),
						 RiseFall::fallIndex()});

RiseFallBoth::RiseFallBoth(const char *name,
				     const char *short_name,
				     int sdf_triple_index,
				     RiseFall *as_rise_fall,
				     std::vector<RiseFall*> range,
				     std::vector<int> range_index) :
  name_(name),
  short_name_(stringCopy(short_name)),
  sdf_triple_index_(sdf_triple_index),
  as_rise_fall_(as_rise_fall),
  range_(range),
  range_index_(range_index)
{
}

RiseFallBoth::~RiseFallBoth()
{
  stringDelete(short_name_);
}

RiseFallBoth *
RiseFallBoth::find(const char *tr_str)
{
  if (stringEq(tr_str, rise_.name()))
    return &rise_;
  else if (stringEq(tr_str, fall_.name()))
    return &fall_;
  else if (stringEq(tr_str, rise_fall_.name()))
    return &rise_fall_;
  else
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

void
RiseFallBoth::setShortName(const char *short_name)
{
  stringDelete(short_name_);
  short_name_ = stringCopy(short_name);
}

////////////////////////////////////////////////////////////////

TransitionMap Transition::transition_map_;
int Transition::max_index_ = 0;

// Sdf triple order defined on Sdf 3.0 spec, pg 3-17.
Transition Transition::rise_{  "^", "01", RiseFall::rise(),  0};
Transition Transition::fall_ { "v", "10", RiseFall::fall(),  1};
Transition Transition::tr_0Z_{"0Z", "0Z", RiseFall::rise(),  2};
Transition Transition::tr_Z1_{"Z1", "Z1", RiseFall::rise(),  3};
Transition Transition::tr_1Z_{"1Z", "1Z", RiseFall::fall(),  4};
Transition Transition::tr_Z0_{"Z0", "Z0", RiseFall::fall(),  5};
Transition Transition::tr_0X_{"0X", "0X", RiseFall::rise(),  6};
Transition Transition::tr_X1_{"X1", "X1", RiseFall::rise(),  7};
Transition Transition::tr_1X_{"1X", "1X", RiseFall::fall(),  8};
Transition Transition::tr_X0_{"X0", "X0", RiseFall::fall(),  9};
Transition Transition::tr_XZ_{"XZ", "XZ",               nullptr, 10};
Transition Transition::tr_ZX_{"ZX", "ZX",               nullptr, 11};
Transition Transition::rise_fall_{"*", "**",               nullptr, -1};

Transition::Transition(const char *name,
		       const char *init_final,
		       RiseFall *as_rise_fall,
		       int sdf_triple_index) :
  name_(stringCopy(name)),
  init_final_(init_final),
  as_rise_fall_(as_rise_fall),
  sdf_triple_index_(sdf_triple_index)
{
  transition_map_[name_] = this;
  transition_map_[init_final_] = this;
  max_index_ = max(sdf_triple_index, max_index_);
}

Transition::~Transition()
{
  stringDelete(name_);
}

bool
Transition::matches(const Transition *tr) const
{
  return this == riseFall() || tr == this;
}

Transition *
Transition::find(const char *tr_str)
{
  return transition_map_.findKey(tr_str);
}

const RiseFallBoth *
Transition::asRiseFallBoth() const
{
  return reinterpret_cast<const RiseFallBoth*>(as_rise_fall_);
}

void
Transition::setName(const char *name)
{
  stringDelete(name_);
  name_ = stringCopy(name);
}

////////////////////////////////////////////////////////////////

RiseFallIterator::RiseFallIterator(const RiseFallBoth *rf)
{
  if (rf == RiseFallBoth::riseFall()) {
    index_ = 0;
    index_max_ = RiseFall::index_max;
  }
  else {
    index_ = rf->asRiseFall()->index();
    index_max_ = index_;
  }
}

void
RiseFallIterator::init()
{
  index_ = 0;
  index_max_ = RiseFall::index_max;
}

bool
RiseFallIterator::hasNext()
{
  return index_ <= index_max_;
}

RiseFall *
RiseFallIterator::next()
{
  return (index_++ == 0) ? RiseFall::rise() : RiseFall::fall();
}

} // namespace
