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

#include "ExceptionPath.hh"

#include <algorithm>

#include "MinMax.hh"
#include "TimingRole.hh"
#include "Units.hh"
#include "Transition.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "Clock.hh"

namespace sta {

using std::string;

static bool
thrusIntersectPts(ExceptionThruSeq *thrus1,
		  ExceptionThruSeq *thrus2,
                  const Network *network);
static void
insertPinPairsThruHierPin(const Pin *hpin,
			  const Network *network,
			  PinPairSet *pairs);
static void
insertPinPairsThruNet(const Net *net,
		      const Network *network,
		      PinPairSet *pairs);
static void
deletePinPairsThruHierPin(const Pin *hpin, 
			  const Network *network,
			  PinPairSet *pairs);

////////////////////////////////////////////////////////////////

const char *
EmptyExpceptionPt::what() const noexcept
{
  return "empty exception from/through/to.";
}

void
checkFromThrusTo(ExceptionFrom *from,
		 ExceptionThruSeq *thrus,
		 ExceptionTo *to)
{
  bool found_empty = ((from && !from->hasObjects())
		      || (to
			  && (!to->hasObjects()
			      && to->transition()
			      == RiseFallBoth::riseFall()
			      && (to->endTransition()
 				  == RiseFallBoth::riseFall()))));
  if (thrus) {
    for (ExceptionThru *thru : *thrus) {
      if (!thru->hasObjects())
	found_empty = true;
    }
  }
  if (found_empty)
    throw EmptyExpceptionPt();
}

ExceptionPath::ExceptionPath(ExceptionFrom *from,
			     ExceptionThruSeq *thrus,
			     ExceptionTo *to,
			     const MinMaxAll *min_max,
			     bool own_pts,
			     int priority,
			     const char *comment) :
  SdcCmdComment(comment),
  from_(from),
  thrus_(thrus),
  to_(to),
  min_max_(min_max),
  own_pts_(own_pts),
  priority_(priority),
  id_(0)
{
  makeStates();
}

ExceptionPath::~ExceptionPath()
{
  if (own_pts_) {
    delete from_;
    delete to_;
    if (thrus_) {
      thrus_->deleteContents();
      delete thrus_;
    }
  }
  ExceptionState *state, *next_state;
  for (state = states_; state; state = next_state) {
    next_state = state->nextState();
    delete state;
  }
}

const char *
ExceptionPath::asString(const Network *network) const
{
  const char *from_thru_to = fromThruToString(network);
  const char *type = typeString();
  size_t length = strlen(type) + strlen(from_thru_to) + 1;
  char *result = makeTmpString(length);
  char *r = result;
  stringAppend(r, type);
  stringAppend(r, from_thru_to);
  return result;
}

void
ExceptionPath::setId(size_t id)
{
  id_ = id;
}

ExceptionPt *
ExceptionPath::firstPt()
{
  if (from_)
    return from_;
  else if (thrus_ && !thrus_->empty())
    return (*thrus_)[0];
  else if (to_)
    return to_;
  else
    return nullptr;
}

bool
ExceptionPath::matchesFirstPt(const RiseFall *to_rf,
			      const MinMax *min_max)
{
  ExceptionPt *first_pt = firstPt();
  return first_pt->transition()->matches(to_rf)
    && matches(min_max, false);
}

bool
ExceptionPath::matches(const MinMax *min_max,
		       bool) const
{
  return min_max_->matches(min_max);
}

void
ExceptionPath::setPriority(int priority)
{
  priority_ = priority;
}

////////////////////////////////////////////////////////////////
// Exception precedence relative to from/thru/to pins/clocks:
// Priority order:
//   1) -from pin/instance/port
//   2) -to pin/instance/port
//   3) -through pin
//   4) -from clock
//   5) -to clock
//
// Foreach priority level (from 1 to 5)
//   If the exception has this type of qualifier, it takes
//   priority over an exception without this type of qualifier.
int
ExceptionPath::fromThruToPriority(ExceptionFrom *from,
				  ExceptionThruSeq *thrus,
				  ExceptionTo *to)
{
  int priority = 0;
  if (from && (from->hasPins() || from->hasInstances()))
    priority |= (1 << 6);
  if (to && (to->hasPins() || to->hasInstances()))
    priority |= (1 << 5);
  if (thrus && !thrus->empty())
    priority |= (1 << 4);
  if (from && from->hasClocks())
    priority |= (1 << 3);
  if (to && to->hasClocks())
    priority |= (1 << 2);
  // Leave room for minMaxPriority() which uses bits 0 and 1.
  return priority;
}

size_t
ExceptionPath::hash() const
{
  return hash(nullptr);
}

size_t
ExceptionPath::hash(ExceptionPt *missing_pt) const
{
  size_t hash = typePriority();
  int pot = 32;
  ExceptionPtIterator pt_iter(this);
  while (pt_iter.hasNext()) {
    ExceptionPt *pt = pt_iter.next();
    if (pt != missing_pt)
      hash += pt->hash() * (pot - 1);
    pot *= 2;
  }
  return hash;
}

bool
ExceptionPath::mergeable(ExceptionPath *exception) const
{
  return stringEqualIf(comment_, exception->comment());
}

bool
ExceptionPath::mergeablePts(ExceptionPath *exception) const
{
  ExceptionPt *ignore;
  return mergeablePts(exception, nullptr, ignore);
}

bool
ExceptionPath::mergeablePts(ExceptionPath *exception2,
			    ExceptionPt *missing_pt2,
			    ExceptionPt *&missing_pt) const
{
  missing_pt = nullptr;
  ExceptionFrom *from2 = exception2->from();
  if ((from_ && from2
       && !(from_->transition() == from2->transition()
	    && (from2 == missing_pt2
		|| from_->equal(from2))))
       || (from_ && from2 == nullptr)
       || (from_ == nullptr && from2))
    return false;
  if (from2 == missing_pt2)
    missing_pt = from_;

  ExceptionThruSeq::Iterator thru_iter(thrus_);
  ExceptionThruSeq::Iterator thru_iter2(exception2->thrus());
  while (thru_iter.hasNext()
	 && thru_iter2.hasNext()) {
    ExceptionThru *thru = thru_iter.next();
    ExceptionThru *thru2 = thru_iter2.next();
    if (!(thru->transition() == thru2->transition()
	  && (thru2 == missing_pt2
	      || thru->equal(thru))))
      return false;
    if (thru2 == missing_pt2)
      missing_pt = thru;
  }
  if (thru_iter.hasNext()
      || thru_iter2.hasNext())
    return false;

  ExceptionTo *to2 = exception2->to();
  if ((to_ && to2
       && !(to_->transition() == to2->transition()
	    && to_->endTransition() == to2->endTransition()
	    && (to2 == missing_pt2
		|| to_->equal(to2))))
      || (to_ && to2 == nullptr)
      || (to_ == nullptr && to2))
    return false;
  if (to2 == missing_pt2)
    missing_pt = to_;
  return true;
}

bool
ExceptionPath::intersectsPts(ExceptionPath *exception,
                             const Network *network) const
{
  ExceptionFrom *from2 = exception->from();
  ExceptionThruSeq *thrus2 = exception->thrus();
  ExceptionTo *to2 = exception->to();
  if (((from_ == nullptr && from2 == nullptr)
       || (from_ && from2 && from_->intersectsPts(from2, network)))
      && ((thrus_ == nullptr && thrus2 == nullptr)
	  || (thrus_ && thrus2 && thrus_->size() == thrus2->size()))
      && ((to_ == nullptr && to2 == nullptr)
	  || (to_ && to2 && to_->intersectsPts(to2, network)))) {
    ExceptionThruSeq::Iterator thrus_iter1(thrus_);
    ExceptionThruSeq::Iterator thrus_iter2(thrus2);
    while (thrus_iter1.hasNext() && thrus_iter2.hasNext()) {
      ExceptionThru *thru1 = thrus_iter1.next();
      ExceptionThru *thru2 = thrus_iter2.next();
      if (!thru1->intersectsPts(thru2, network))
	return false;
    }
    return true;
  }
  return false;
}

const char *
ExceptionPath::fromThruToString(const Network *network) const
{
  string str;
  if (min_max_ != MinMaxAll::all()) {
    str += " -";
    str += min_max_->to_string();
  }

  if (from_)
    str += from_->asString(network);

  if (thrus_) {
    str += " -thru";
    bool first_thru = true;
    if (thrus_) {
      for (ExceptionThru *thru : *thrus_) {
        if (!first_thru)
          str += " &&";
        str += " {";
        str += thru->asString(network);
        str += "}";
        first_thru = false;
      }
    }
  }

  if (to_)
    str += to_->asString(network);

  char *result = makeTmpString(str.size() + 1);
  strcpy(result, str.c_str());
  return result;
}

ExceptionState *
ExceptionPath::firstState()
{
  return states_;
}

void
ExceptionPath::makeStates()
{
  if (thrus_) {
    ExceptionState *prev_state = nullptr;
    bool first = true;
    int index = 0;
    for (ExceptionThru *thru : *thrus_) {
      // No state for first -thru if no -from,since it kicks off the exception.
      if (!(from_ == nullptr && first)) {
        ExceptionState *state = new ExceptionState(this, thru, index);
        if (prev_state)
          prev_state->setNextState(state);
        else
          states_ = state;
        prev_state = state;
      }
      first = false;
      index++;
    }
    // Last state indicates all the thrus have been traversed.
    ExceptionState *state = new ExceptionState(this, nullptr, index);
    if (prev_state)
      prev_state->setNextState(state);
    else
      states_ = state;
  }
  else
    states_ = new ExceptionState(this, nullptr, 0);
}

bool
ExceptionPath::resetMatch(ExceptionFrom *from,
			  ExceptionThruSeq *thrus,
			  ExceptionTo *to,
			  const MinMaxAll *min_max,
                          const Network *network)
{
  // Only the reset expception points need to match.
  // For example, if the reset is -from, it matches any
  // exceptions that match the -from even if they are more specific.
  // -from
  return ((from && from_
	   && thrus == nullptr
	   && to == nullptr
	   && from_->intersectsPts(from, network))
	  // -thru
	  || (from == nullptr
	      && thrus && thrus_
	      && to == nullptr
	      && thrusIntersectPts(thrus_, thrus, network))
	  // -to
	  || (from == nullptr
	      && thrus == nullptr
	      && to && to_
	      && to_->intersectsPts(to, network))
	  // -from -thru
	  || (from && from_
	      && thrus && thrus_
	      && to == nullptr
	      && from_->intersectsPts(from, network)
	      && thrusIntersectPts(thrus_, thrus, network))
	  // -from -to
	  || (from && from_
	      && thrus == nullptr
	      && to && to_
	      && from_->intersectsPts(from, network)
	      && to_->intersectsPts(to, network))
	  // -thru -to
	  || (from == nullptr
	      && thrus && thrus_
	      && to && to_
	      && thrusIntersectPts(thrus_, thrus, network)
	      && to_->intersectsPts(to, network))
	  // -from -thru -to
	  || (from && from_
	      && thrus && thrus_
	      && to && to_
	      && from_->intersectsPts(from, network)
	      && thrusIntersectPts(thrus_, thrus, network)
	      && to_->intersectsPts(to, network)))
    && (min_max == MinMaxAll::all()
	|| min_max_ == min_max);
}

static bool
thrusIntersectPts(ExceptionThruSeq *thrus1,
		  ExceptionThruSeq *thrus2,
                  const Network *network)
{
  ExceptionThruSeq::Iterator thrus_iter1(thrus1);
  ExceptionThruSeq::Iterator thrus_iter2(thrus2);
  while (thrus_iter1.hasNext() && thrus_iter2.hasNext()) {
    ExceptionThru *thru1 = thrus_iter1.next();
    ExceptionThru *thru2 = thrus_iter2.next();
    if (!thru1->intersectsPts(thru2, network))
      return false;
  }
  return true;
}

void
ExceptionPath::deleteInstance(const Instance *inst,
                              const Network *network)
{
  if (from_)
    from_->deleteInstance(inst, network);
  if (thrus_) {
    for (ExceptionThru *thru : *thrus_)
      thru->deleteInstance(inst, network);
  }
  if (to_)
    to_->deleteInstance(inst, network);
}

////////////////////////////////////////////////////////////////

PathDelay::PathDelay(ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to,
		     const MinMax *min_max,
		     bool ignore_clk_latency,
                     bool break_path,
		     float delay,
		     bool own_pts,
		     const char *comment) :
  ExceptionPath(from, thrus, to, min_max->asMinMaxAll(), own_pts,
		pathDelayPriority() + fromThruToPriority(from, thrus, to),
		comment),
  ignore_clk_latency_(ignore_clk_latency),
  break_path_(break_path),
  delay_(delay)
{
}

ExceptionPath *
PathDelay::clone(ExceptionFrom *from,
		 ExceptionThruSeq *thrus,
		 ExceptionTo *to,
		 bool own_pts)
{
  return new PathDelay(from, thrus, to, min_max_->asMinMax(),
		       ignore_clk_latency_, break_path_, delay_,
                       own_pts, comment_);
}

int
PathDelay::typePriority() const
{
  return pathDelayPriority();
}

bool
PathDelay::tighterThan(ExceptionPath *exception) const
{
  if (min_max_->asMinMax() == MinMax::min())
    return delay_ > exception->delay();
  else
    return delay_ < exception->delay();
}

const char *
PathDelay::asString(const Network *network) const
{
  const char *from_thru_to = fromThruToString(network);
  const char *result = stringPrintTmp("PathDelay %.3fns%s",
				      delay_ * 1E+9F,
				      from_thru_to);
  return result;
}

const char *
PathDelay::typeString() const
{
  return "Path";
}

bool
PathDelay::mergeable(ExceptionPath *exception) const
{
  return ExceptionPath::mergeable(exception)
    && overrides(exception)
    && exception->ignoreClkLatency() == ignore_clk_latency_
    && exception->delay() == delay_
    // path delays -to pin/inst may be along the same path because they
    // can be internal pins and not restricted to normal endpoints.
    // This means that
    //   set_max_delay -to p1
    //   set_max_delay -to p2
    // is not the same as
    //   set_max_delay -to {p1 p2}
    // when p1 and p2 are on the same path because once endpoint
    // is encountered the exception is not complete.
    && to_ == nullptr
    && exception->to() == nullptr;
}

bool
PathDelay::overrides(ExceptionPath *exception) const
{
  return exception->isPathDelay()
    && exception->priority() == priority_
    && exception->minMax() == min_max_;
}

////////////////////////////////////////////////////////////////

FalsePath::FalsePath(ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to,
		     const MinMaxAll *min_max,
		     bool own_pts,
		     const char *comment) :
  ExceptionPath(from, thrus, to, min_max, own_pts,
		falsePathPriority() + fromThruToPriority(from, thrus, to),
		comment)
{
}

FalsePath::FalsePath(ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to,
		     const MinMaxAll *min_max,
		     bool own_pts,
		     int priority,
		     const char *comment) :
  ExceptionPath(from, thrus, to, min_max, own_pts, priority, comment)
{
}

ExceptionPath *
FalsePath::clone(ExceptionFrom *from,
		 ExceptionThruSeq *thrus,
		 ExceptionTo *to,
		 bool own_pts)
{
  return new FalsePath(from, thrus, to, min_max_, own_pts, comment_);
}

int
FalsePath::typePriority() const
{
  return falsePathPriority();
}

bool
FalsePath::tighterThan(ExceptionPath *) const
{
  return false;
}

const char *
FalsePath::typeString() const
{
  return "False";
}

bool
FalsePath::mergeable(ExceptionPath *exception) const
{
  return ExceptionPath::mergeable(exception)
    && overrides(exception);
}

bool
FalsePath::overrides(ExceptionPath *exception) const
{
  return exception->priority() == priority()
    && exception->minMax() == min_max_;
}

////////////////////////////////////////////////////////////////

LoopPath::LoopPath(ExceptionThruSeq *thrus, bool own_pts) :
  FalsePath(nullptr, thrus, nullptr, MinMaxAll::all(), own_pts,
	    falsePathPriority() + fromThruToPriority(nullptr, thrus, nullptr),
	    nullptr)
{
}

const char *
LoopPath::typeString() const
{
  return "Loop";
}

bool
LoopPath::mergeable(ExceptionPath *) const
{
  return false;
}

////////////////////////////////////////////////////////////////

MultiCyclePath::MultiCyclePath(ExceptionFrom *from,
			       ExceptionThruSeq *thrus,
			       ExceptionTo *to,
			       const MinMaxAll *min_max,
			       bool use_end_clk,
			       int path_multiplier,
			       bool own_pts,
			       const char *comment) :
  ExceptionPath(from, thrus, to, min_max, own_pts,
		multiCyclePathPriority() + fromThruToPriority(from, thrus, to),
		comment),
  use_end_clk_(use_end_clk),
  path_multiplier_(path_multiplier)
{
}

ExceptionPath *
MultiCyclePath::clone(ExceptionFrom *from,
		      ExceptionThruSeq *thrus,
		      ExceptionTo *to,
		      bool own_pts)
{
  return new MultiCyclePath(from, thrus, to, min_max_, use_end_clk_,
			    path_multiplier_, own_pts, comment_);
}

int
MultiCyclePath::pathMultiplier(const MinMax *min_max) const
{
  if (min_max_ == MinMaxAll::all()
      && min_max == MinMax::min())
    // Path multiplier is zero if no -setup/-hold is specified.
    return 0;
  else
    return path_multiplier_;
}

int
MultiCyclePath::priority(const MinMax *min_max) const
{
  if (min_max_ == MinMaxAll::all())
    return priority_ + 1;
  else if (min_max_->asMinMax() == min_max)
    return priority_ + 2;
  else
    return priority_;
}

int
MultiCyclePath::typePriority() const
{
  return multiCyclePathPriority();
}

bool
MultiCyclePath::tighterThan(ExceptionPath *exception) const
{
  return path_multiplier_ < exception->pathMultiplier();
}

bool
MultiCyclePath::matches(const MinMax *min_max,
			bool exactly) const
{
  return min_max_->matches(min_max)
    // set_multicycle_path -setup determines hold check accounting,
    // so they must be propagated for min (hold) paths.
    || (!exactly && min_max == MinMax::min());
}

const char *
MultiCyclePath::asString(const Network *network) const
{
  const char *from_thru_to = fromThruToString(network);
  const char *result = stringPrintTmp("Multicycle %s %d%s",
				      (use_end_clk_) ? "-end" : "-start",
				      path_multiplier_,
				      from_thru_to);
  return result;
}

const char *
MultiCyclePath::typeString() const
{
  return "Multicycle";
}

bool
MultiCyclePath::mergeable(ExceptionPath *exception) const
{
  return ExceptionPath::mergeable(exception)
    && overrides(exception)
    && exception->pathMultiplier() == path_multiplier_;
}

bool
MultiCyclePath::overrides(ExceptionPath *exception) const
{
  return exception->isMultiCycle()
    && exception->priority() == priority()
    && exception->minMax() == min_max_;
}

////////////////////////////////////////////////////////////////

FilterPath::FilterPath(ExceptionFrom *from,
		       ExceptionThruSeq *thrus,
		       ExceptionTo *to,
		       bool own_pts) :
  ExceptionPath(from, thrus, to, MinMaxAll::all(), own_pts,
		filterPathPriority() + fromThruToPriority(from, thrus, to),
		nullptr)
{
}

const char *
FilterPath::typeString() const
{
  return "Filter";
}

ExceptionPath *
FilterPath::clone(ExceptionFrom *from,
		  ExceptionThruSeq *thrus,
		  ExceptionTo *to,
		  bool own_pts)
{
  return new FilterPath(from, thrus, to, own_pts);
}

int
FilterPath::typePriority() const
{
  return filterPathPriority();
}

bool
FilterPath::tighterThan(ExceptionPath *) const
{
  return false;
}

// Filter paths are used for report -from/-thru/-to as well as
// generated clock insertion delays so do not let them merge.
bool
FilterPath::mergeable(ExceptionPath *) const
{
  return false;
}

bool
FilterPath::overrides(ExceptionPath *) const
{
  return false;
}

bool
FilterPath::resetMatch(ExceptionFrom *,
		       ExceptionThruSeq *,
		       ExceptionTo *,
		       const MinMaxAll *,
                       const Network *)
{
  return false;
}

////////////////////////////////////////////////////////////////

GroupPath::GroupPath(const char *name,
		     bool is_default,
		     ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to,
		     bool own_pts,
		     const char *comment) :
  ExceptionPath(from, thrus, to, MinMaxAll::all(), own_pts,
		groupPathPriority() + fromThruToPriority(from, thrus, to),
		comment),
  name_(stringCopy(name)),
  is_default_(is_default)
{
}

GroupPath::~GroupPath()
{
  stringDelete(name_);
}

const char *
GroupPath::typeString() const
{
  return "Group";
}

ExceptionPath *
GroupPath::clone(ExceptionFrom *from,
		 ExceptionThruSeq *thrus,
		 ExceptionTo *to,
		 bool own_pts)
{
  return new GroupPath(name_, is_default_, from, thrus, to, own_pts,
		       comment_);
}

int
GroupPath::typePriority() const
{
  return groupPathPriority();
}

bool
GroupPath::tighterThan(ExceptionPath *) const
{
  return false;
}

bool
GroupPath::mergeable(ExceptionPath *exception) const
{
  return stringEqIf(name_, exception->name())
    && ExceptionPath::mergeable(exception)
    && overrides(exception);
}

bool
GroupPath::overrides(ExceptionPath *exception) const
{
  return exception->isGroupPath()
    && is_default_ == exception->isDefault()
    && stringEqIf(name_, exception->name());
}

////////////////////////////////////////////////////////////////

const int ExceptionPt::as_string_max_objects_ = 20;

ExceptionPt::ExceptionPt(const RiseFallBoth *rf,
			 bool own_pts) :
  rf_(rf),
  own_pts_(own_pts),
  hash_(0)
{
}

// ExceptionPt initialization functions set hash_ and incrementally
// maintain the value.
size_t
ExceptionPt::hash() const
{
  return hash_;
}

ExceptionFromTo::ExceptionFromTo(PinSet *pins,
				 ClockSet *clks,
				 InstanceSet *insts,
				 const RiseFallBoth *rf,
				 bool own_pts,
                                 const Network *network) :
  ExceptionPt(rf, own_pts),
  pins_(pins),
  clks_(clks),
  insts_(insts)
{
  // Delete empty sets.
  if (pins_ && pins_->empty()) {
    if (own_pts)
      delete pins_;
    pins_ = nullptr;
  }
  if (clks_ && clks_->empty()) {
    if (own_pts)
      delete clks_;
    clks_ = nullptr;
  }
  if (insts_ && insts_->empty()) {
    if (own_pts)
      delete insts_;
    insts_ = nullptr;
  }
  findHash(network);
}

ExceptionFromTo::~ExceptionFromTo()
{
  if (own_pts_) {
    delete pins_;
    delete clks_;
    delete insts_;
  }
}

bool
ExceptionFromTo::hasPins() const
{
  return pins_ != nullptr && !pins_->empty();
}

bool
ExceptionFromTo::hasClocks() const
{
  return clks_ != nullptr && !clks_->empty();
}

bool
ExceptionFromTo::hasInstances() const
{
  return insts_ != nullptr && !insts_->empty();
}

bool
ExceptionFromTo::hasObjects() const
{
  return hasPins() || hasClocks() || hasInstances();
}

PinSet
ExceptionFromTo::allPins(const Network *network)
{
  PinSet pins(network);
  if (pins_) {
    for (const Pin *pin : *pins_)
      pins.insert(pin);
  }
  if (insts_) {
    for (const Instance *inst : *insts_) {
      InstancePinIterator *pin_iter = network->pinIterator(inst);
      while (pin_iter->hasNext()) {
	const Pin *pin = pin_iter->next();
	pins.insert(pin);
      }
      delete pin_iter;
    }
  }
  return pins;
}

void
ExceptionFromTo::findHash(const Network *network)
{
  hash_ = 0;
  if (pins_) {
    size_t hash = 0;
    for (const Pin *pin : *pins_)
      hash += network->id(pin);
    hash_ += hash * hash_pin;
  }
  if (clks_) {
    size_t hash = 0;
    for (Clock *clk : *clks_)
      hash += clk->index();
    hash_ += hash * hash_clk;
  }
  if (insts_) {
    size_t hash = 0;
    for (const Instance *inst : *insts_)
      hash += network->id(inst);
    hash_ += hash * hash_inst;
  }
}

bool
ExceptionFromTo::equal(ExceptionFromTo *from_to) const
{
  return PinSet::equal(from_to->pins_, pins_)
    && ClockSet::equal(from_to->clks_, clks_)
    && InstanceSet::equal(from_to->insts_, insts_)
    && from_to->transition() == rf_;
}

int
ExceptionFromTo::compare(ExceptionPt *pt2,
			 const Network *network) const
{
  int priority_cmp = typePriority() - pt2->typePriority();
  if (priority_cmp == 0) {
    int pin_cmp = PinSet::compare(pins_, pt2->pins(), network);
    if (pin_cmp == 0) {
      int clk_cmp = sta::compare(clks_, pt2->clks());
      if (clk_cmp == 0) {
	int inst_cmp = InstanceSet::compare(insts_, pt2->instances(), network);
	if (inst_cmp == 0)
	  return rf_->index() - pt2->transition()->index();
	else
	  return inst_cmp;
      }
      else
	return clk_cmp;
    }
    else
      return pin_cmp;
  }
  else
    return priority_cmp;
}

void
ExceptionFromTo::mergeInto(ExceptionPt *pt,
                           const Network *network)
{
  if (pins_) {
    for (const Pin *pin : *pins_)
      pt->addPin(pin, network);
  }
  if (clks_) {
    for (Clock *clk : *clks_)
      pt->addClock(clk);
  }
  if (insts_) {
    for (const Instance *inst : *insts_)
      pt->addInstance(inst, network);
  }
}

void
ExceptionFromTo::deleteObjects(ExceptionFromTo *pt,
                               const Network *network)
{
  PinSet *pins = pt->pins();
  if (pins && pins_) {
    for (const Pin *pin : *pins)
      deletePin(pin, network);
  }
  ClockSet *clks = pt->clks();
  if (clks && clks_) {
    for (Clock *clk : *clks)
      deleteClock(clk);
  }
  InstanceSet *insts = pt->instances();
  if (insts && insts_) {
    for (const Instance *inst : *insts)
      deleteInstance(inst, network);
  }
}

void
ExceptionFromTo::addPin(const Pin *pin,
                        const Network *network)
{
  if (pins_ == nullptr)
    pins_ = new PinSet(network);
  if (!pins_->hasKey(pin)) {
    pins_->insert(pin);
    // Incrementally update hash.
    hash_ += network->id(pin) * hash_pin;
  }
}

void
ExceptionFromTo::addClock(Clock *clk)
{
  if (clks_ == nullptr)
    clks_ = new ClockSet;
  if (!clks_->hasKey(clk)) {
    clks_->insert(clk);
    // Incrementally update hash.
    hash_ += clk->index() * hash_clk;
  }
}

void
ExceptionFromTo::addInstance(const Instance *inst,
                             const Network *network)
{
  if (insts_ == nullptr)
    insts_ = new InstanceSet(network);
  if (!insts_->hasKey(inst)) {
    insts_->insert(inst);
    // Incrementally update hash.
    hash_ += network->id(inst) * hash_inst;
  }
}

void
ExceptionFromTo::deletePin(const Pin *pin,
                           const Network *network)
{
  if (pins_) {
    pins_->erase(pin);
    // Incrementally update hash.
    hash_ -= network->id(pin) * hash_pin;
  }
}

void
ExceptionFromTo::deleteClock(Clock *clk)
{
  if (clks_) {
    clks_->erase(clk);
    // Incrementally update hash.
    hash_ -= clk->index() * hash_clk;
  }
}

void
ExceptionFromTo::deleteInstance(const Instance *inst,
                                const Network *network)
{
  if (insts_) {
    insts_->erase(inst);
    // Incrementally update hash.
    hash_ -= network->id(inst) * hash_inst;
  }
}

const char *
ExceptionFromTo::asString(const Network *network) const
{
  string str;
  str += " ";
  str += cmdKeyword();
  str += " {";

  int obj_count = 0;
  bool first = true;
  if (pins_) {
    PinSeq pins = sortByPathName(pins_, network);
    for (const Pin *pin : pins) {
      if (!first)
	str += ", ";
      str += network->pathName(pin);
      first = false;
      obj_count++;
      if (obj_count > as_string_max_objects_)
        break;
    }
  }
  if (clks_) {
    ClockSeq clks = sortByName(clks_);
    for (Clock *clk : clks) {
      if (!first)
	str += ", ";
      str += clk->name();
      first = false;
      obj_count++;
      if (obj_count > as_string_max_objects_)
        break;
    }
  }
  if (insts_) {
    InstanceSeq insts = sortByPathName(insts_, network);
    for (const Instance *inst : insts) {
      if (!first)
	str += ", ";
      str += network->pathName(inst);
      first = false;
      obj_count++;
      if (obj_count > as_string_max_objects_)
        break;
    }
  }
  if (obj_count == as_string_max_objects_)
    str += ", ...";

  str += "}";

  char *result = makeTmpString(str.size() + 1);
  strcpy(result, str.c_str());
  return result;
}

size_t
ExceptionFromTo::objectCount() const
{
  size_t count = 0;
  if (pins_)
    count += pins_->size();
  if (clks_)
    count += clks_->size();
  if (insts_)
    count += insts_->size();
  return count;
}

////////////////////////////////////////////////////////////////

ExceptionFrom::ExceptionFrom(PinSet *pins,
			     ClockSet *clks,
			     InstanceSet *insts,
			     const RiseFallBoth *rf,
			     bool own_pts,
                             const Network *network) :
  ExceptionFromTo(pins, clks, insts, rf, own_pts, network)
{
}

void
ExceptionFrom::findHash(const Network *network)
{
  ExceptionFromTo::findHash(network);
  hash_ += rf_->index() * 31 + 29;
}

ExceptionFrom *
ExceptionFrom::clone(const Network *network)
{
  PinSet *pins = nullptr;
  if (pins_)
    pins = new PinSet(*pins_);
  ClockSet *clks = nullptr;
  if (clks_)
    clks = new ClockSet(*clks_);
  InstanceSet *insts = nullptr;
  if (insts_)
    insts = new InstanceSet(*insts_);
  return new ExceptionFrom(pins, clks, insts, rf_, true, network);
}

bool
ExceptionFrom::intersectsPts(ExceptionFrom *from,
                             const Network *network) const
{
  return from->transition() == rf_
    && ((pins_ && PinSet::intersects(pins_, from->pins(), network))
	|| (clks_ && ClockSet::intersects(clks_, from->clks(), ClockIndexLess()))
	|| (insts_ && InstanceSet::intersects(insts_, from->instances(), network)));
}

const char *
ExceptionFrom::cmdKeyword() const
{
  if (rf_ == RiseFallBoth::rise())
    return "-rise_from";
  else if (rf_ == RiseFallBoth::fall())
    return "-fall_from";
  else
    return "-from";
}

////////////////////////////////////////////////////////////////

ExceptionTo::ExceptionTo(PinSet *pins,
			 ClockSet *clks,
			 InstanceSet *insts,
			 const RiseFallBoth *rf,
			 const RiseFallBoth *end_rf,
			 bool own_pts,
                         const Network *network) :
  ExceptionFromTo(pins, clks, insts, rf, own_pts, network),
  end_rf_(end_rf)
{
}

ExceptionTo *
ExceptionTo::clone(const Network *network)
{
  PinSet *pins = nullptr;
  if (pins_)
    pins = new PinSet(*pins_);
  ClockSet *clks = nullptr;
  if (clks_)
    clks = new ClockSet(*clks_);
  InstanceSet *insts = nullptr;
  if (insts_)
    insts = new InstanceSet(*insts_);
  return new ExceptionTo(pins, clks, insts, rf_, end_rf_, true, network);
}

const char *
ExceptionTo::asString(const Network *network) const
{
  string str;
  if (hasObjects())
    str += ExceptionFromTo::asString(network);

  if (end_rf_ != RiseFallBoth::riseFall())
    str += (end_rf_ == RiseFallBoth::rise()) ? " -rise" : " -fall";

  char *result = makeTmpString(str.size() + 1);
  strcpy(result, str.c_str());
  return result;
}

bool
ExceptionTo::intersectsPts(ExceptionTo *to,
                           const Network *network) const
{
  return to->transition() == rf_
    && to->endTransition() == end_rf_
    && ((pins_ && PinSet::intersects(pins_, to->pins(), network))
	|| (clks_ && ClockSet::intersects(clks_, to->clks(), ClockIndexLess()))
	|| (insts_ && InstanceSet::intersects(insts_, to->instances(), network)));
}

bool
ExceptionTo::matchesFilter(const Pin *pin,
			   const ClockEdge *clk_edge,
			   const RiseFall *end_rf,
			   const Network *network) const
{
  // "report -to reg" matches clock pins.
  return matches(pin, clk_edge, end_rf, true, network);
}

bool
ExceptionTo::matches(const Pin *pin,
		     const ClockEdge *clk_edge,
 		     const RiseFall *end_rf,
		     const Network *network) const
{
  // "exception -to reg" does not match reg clock pins.
  return matches(pin, clk_edge, end_rf, false, network);
}

bool
ExceptionTo::matches(const Pin *pin,
		     const ClockEdge *clk_edge,
 		     const RiseFall *end_rf,
		     bool inst_matches_reg_clk_pin,
		     const Network *network) const

{
  return (pins_
	  && pins_->hasKey(const_cast<Pin*>(pin))
	  && rf_->matches(end_rf)
	  && end_rf_->matches(end_rf))
    || (clk_edge
	&& clks_
	&& clks_->hasKey(const_cast<Clock*>(clk_edge->clock()))
	&& rf_->matches(clk_edge->transition())
	&& end_rf_->matches(end_rf))
    || (insts_
	&& (inst_matches_reg_clk_pin
	    || !network->isRegClkPin(pin))
	&& insts_->hasKey(network->instance(pin))
	&& (network->direction(pin)->isAnyInput()
            || network->direction(pin)->isInternal())
	&& rf_->matches(end_rf)
	&& end_rf_->matches(end_rf))
    || (pins_ == nullptr
	&& clks_ == nullptr
	&& insts_ == nullptr
	&& end_rf_->matches(end_rf));
}

bool
ExceptionTo::matches(const Pin *pin,
 		     const RiseFall *end_rf,
		     const Network *network) const
{
  return (pins_
	  && pins_->hasKey(const_cast<Pin*>(pin))
	  && rf_->matches(end_rf)
	  && end_rf_->matches(end_rf))
    || (insts_
	&& insts_->hasKey(network->instance(pin))
	&& (network->direction(pin)->isAnyInput()
            || network->direction(pin)->isInternal())
	&& rf_->matches(end_rf)
	&& end_rf_->matches(end_rf));
}

bool
ExceptionTo::matches(const Pin *pin,
		     const RiseFall *end_rf) const
{
  return (pins_
	  && pins_->hasKey(const_cast<Pin*>(pin))
	  && rf_->matches(end_rf)
	  && end_rf_->matches(end_rf))
    || (pins_ == nullptr
	&& clks_ == nullptr
	&& insts_ == nullptr
	&& end_rf_->matches(end_rf));
}

bool
ExceptionTo::matches(const Clock *clk) const
{
  return clks_
    && clks_->hasKey(const_cast<Clock*>(clk));
}

const char *
ExceptionTo::cmdKeyword() const
{
  if (rf_ == RiseFallBoth::rise())
    return "-rise_to";
  else if (rf_ == RiseFallBoth::fall())
    return "-fall_to";
  else
    return "-to";
}

int
ExceptionTo::compare(ExceptionPt *pt2,
		     const Network *network) const
{
  ExceptionTo *to2 = dynamic_cast<ExceptionTo*>(pt2);
  int cmp = ExceptionFromTo::compare(pt2, network);
  if (cmp == 0)
    return end_rf_->index() - to2->endTransition()->index();
  else
    return cmp;
}

////////////////////////////////////////////////////////////////

ExceptionThru::ExceptionThru(PinSet *pins,
			     NetSet *nets,
			     InstanceSet *insts,
			     const RiseFallBoth *rf,
			     bool own_pts,
			     const Network *network) :
  ExceptionPt(rf, own_pts),
  pins_(pins),
  edges_(nullptr),
  nets_(nets),
  insts_(insts)
{
  // Delete empty sets.
  if (pins_ && pins_->empty()) {
    if (own_pts)
      delete pins_;
    pins_ = nullptr;
  }
  if (nets_ && nets_->empty()) {
    if (own_pts)
      delete nets_;
    nets_ = nullptr;
  }
  if (insts_ && insts_->empty()) {
    if (own_pts)
      delete insts_;
    insts_ = nullptr;
  }
  makeAllEdges(network);
  findHash(network);
}

void
ExceptionThru::makeAllEdges(const Network *network)
{
  if (pins_)
    makePinEdges(network);
  if (nets_)
    makeNetEdges(network);
  if (insts_)
    makeInstEdges(network);
}

void
ExceptionThru::makePinEdges(const Network *network)
{
  for (const Pin *pin : *pins_) {
    if (network->isHierarchical(pin))
      makeHpinEdges(pin, network);
  }
}

// Call after the pin has been deleted from pins_,
// but before the pin has been deleted from the netlist.
void
ExceptionThru::deletePinEdges(const Pin *pin,
			      Network *network)
{
  // Incrementally delete only edges through (hier) or from/to (leaf) the pin.
  if (edges_ && network->net(pin)) {
    if (network->isHierarchical(pin)) {
      // Use driver lookup to minimize potentially expensive calls to
      // deletePinPairsThruHierPin.
      PinSet *drvrs = network->drivers(pin);
      if (drvrs) {
	// Some edges originating at drvrs may not actually go through pin, so
	// still must use deletePinPairsThruHierPin to identify specific edges.
	if (edges_) {
	  for (const EdgePins &edge_pins : *edges_) {
            const Pin *p_first = edge_pins.first;
            if (drvrs->hasKey(p_first)) {
              deletePinPairsThruHierPin(pin, network, edges_);
              break;
            }
          }
        }
      }
    }
    else {
      // erase prevents range iteration.
      EdgePinsSet::Iterator edge_iter(edges_);
      while (edge_iter.hasNext()) {
        const EdgePins &edge_pins = edge_iter.next();
        if (edge_pins.first == pin
            || edge_pins.second == pin) {
          edges_->erase(edge_pins);
        }
      }
    }
  }
}

void
ExceptionThru::makeHpinEdges(const Pin *pin,
			     const Network *network)
{
  if (edges_ == nullptr)
    edges_ = new EdgePinsSet(network);
  // Add edges thru pin to edges_.
  insertPinPairsThruHierPin(pin, network, edges_);
}

void
ExceptionThru::makeNetEdges(const Network *network)
{
  for (const Net *net : *nets_) {
    if (edges_ == nullptr)
      edges_ = new EdgePinsSet(network);
    // Add edges thru pin to edges_.
    insertPinPairsThruNet(net, network, edges_);
  }
}

void
ExceptionThru::makeNetEdges(const Net *net,
			    const Network *network)
{
  if (edges_ == nullptr)
    edges_ = new EdgePinsSet(network);
  // Add edges thru pin to edges_.
  insertPinPairsThruNet(net, network, edges_);
}

void
ExceptionThru::makeInstEdges(const Network *network)
{
  for (const Instance *inst : *insts_) {
    if (network->isHierarchical(inst)) {
      InstancePinIterator *pin_iter = network->pinIterator(inst);
      while (pin_iter->hasNext()) {
	Pin *pin = pin_iter->next();
	makeHpinEdges(pin, network);
      }
      delete pin_iter;
    }
  }
}

void
ExceptionThru::makeInstEdges(Instance *inst,
			     Network *network)
{
  if (network->isHierarchical(inst)) {
    InstancePinIterator *pin_iter = network->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      makeHpinEdges(pin, network);
    }
    delete pin_iter;
  }
}

// Call after the inst has been deleted from insts_,
// but before the inst has been deleted from the netlist.
void
ExceptionThru::deleteInstEdges(Instance *inst,
			       Network *network)
{
  // Incrementally delete edges through each hier pin.
  if (edges_) {
    InstancePinIterator *pin_iter = network->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      deletePinEdges(pin, network);
    }
    delete pin_iter;
  }
}

ExceptionThru::~ExceptionThru()
{
  if (own_pts_) {
    delete pins_;
    delete nets_;
    delete insts_;
    delete edges_;
  }
}

const char *
ExceptionThru::asString(const Network *network) const
{
  string str;
  bool first = true;
  int obj_count = 0;
  if (pins_) {
    PinSeq pins = sortByPathName(pins_, network);
    for (const Pin *pin : pins) {
      if (!first)
	str += ", ";
      str += network->pathName(pin);
      first = false;
      obj_count++;
      if (obj_count > as_string_max_objects_)
        break;
    }
  }
  if (nets_) {
    NetSeq nets = sortByPathName(nets_, network);
    for (const Net *net : nets) {
      if (!first)
	str += ", ";
      str += network->pathName(net);
      first = false;
      obj_count++;
      if (obj_count > as_string_max_objects_)
        break;
    }
  }
  if (insts_) {
    InstanceSeq insts = sortByPathName(insts_, network);
    for (const Instance *inst : insts) {
      if (!first)
	str += ", ";
      str += network->pathName(inst);
      first = false;
      obj_count++;
      if (obj_count > as_string_max_objects_)
        break;
    }
  }
  if (obj_count == as_string_max_objects_)
    str += ", ...";
  if (rf_ == RiseFallBoth::rise())
    str += " rise";
  else if (rf_ == RiseFallBoth::fall())
    str += " fall";

  char *result = makeTmpString(str.size() + 1);
  strcpy(result, str.c_str());
  return result;
}

ExceptionThruSeq *
exceptionThrusClone(ExceptionThruSeq *thrus,
		    const Network *network)
{
  if (thrus) {
    ExceptionThruSeq *thrus_cpy = new ExceptionThruSeq;
    for (ExceptionThru *thru : *thrus) {
      ExceptionThru *thru_cpy = thru->clone(network);
      thrus_cpy->push_back(thru_cpy);
    }
    return thrus_cpy;
  }
  else
    return nullptr;
}

ExceptionThru *
ExceptionThru::clone(const Network *network)
{
  PinSet *pins = nullptr;
  if (pins_)
    pins = new PinSet(*pins_);
  NetSet *nets = nullptr;
  if (nets_)
    nets = new NetSet(*nets_);
  InstanceSet *insts = nullptr;
  if (insts_)
    insts = new InstanceSet(*insts_);
  return new ExceptionThru(pins, nets, insts, rf_, true, network);
}

bool
ExceptionThru::hasObjects() const
{
  return (pins_ != nullptr && !pins_->empty())
    || (nets_ != nullptr && !nets_->empty())
    || (insts_ != nullptr && !insts_->empty());
}


void
ExceptionThru::addPin(const Pin *pin,
                      const Network *network)
{
  if (pins_ == nullptr)
    pins_ = new PinSet(network);
  if (!pins_->hasKey(pin)) {
    pins_->insert(pin);
    // Incrementally update hash.
    hash_ += network->id(pin) * hash_pin;
  }
}

void
ExceptionThru::addNet(const Net *net,
                      const Network *network)
{
  if (nets_ == nullptr)
    nets_ = new NetSet(network);
  if (!nets_->hasKey(net)) {
    nets_->insert(net);
    // Incrementally update hash.
    hash_ += network->id(net) * hash_net;
  }
}

void
ExceptionThru::addInstance(const Instance *inst,
                           const Network *network)
{
  if (insts_ == nullptr)
    insts_ = new InstanceSet(network);
  if (!insts_->hasKey(inst)) {
    insts_->insert(inst);
    // Incrementally update hash.
    hash_ += network->id(inst) * hash_inst;
  }
}

void
ExceptionThru::addEdge(const EdgePins &edge,
                       const Network *network)
{
  if (edges_ == nullptr)
    edges_ = new EdgePinsSet(network);
  edges_->insert(edge);
  // Hash is unchanged because edges are derived from hierarchical pins.
}

void
ExceptionThru::deletePin(const Pin *pin,
                         const Network *network)
{
  if (pins_) {
    pins_->erase(pin);
    // Incrementally update hash.
    hash_ -= network->id(pin) * hash_pin;
  }
}

void
ExceptionThru::deleteNet(const Net *net,
                         const Network *network)
{
  if (nets_) {
    nets_->erase(net);
    // Incrementally update hash.
    hash_ -= network->id(net) * hash_net;
  }
}

void
ExceptionThru::deleteInstance(const Instance *inst,
                              const Network *network)
{
  if (insts_) {
    insts_->erase(inst);
    // Incrementally update hash.
    hash_ -= network->id(inst) * hash_inst;
  }
}

void
ExceptionThru::deleteEdge(const EdgePins &edge)
{
  if (edges_)
    // Hash is unchanged because edges are derived from hierarchical pins.
    edges_->erase(edge);
}

PinSet
ExceptionThru::allPins(const Network *network)
{
  PinSet pins(network);
  if (pins_) {
    for (const Pin *pin : *pins_)
      pins.insert(pin);
  }
  if (insts_) {
    for (const Instance *inst : *insts_) {
      InstancePinIterator *pin_iter = network->pinIterator(inst);
      while (pin_iter->hasNext()) {
	Pin *pin = pin_iter->next();
	pins.insert(pin);
      }
      delete pin_iter;
    }
  }
  if (nets_) {
    for (const Net *net : *nets_) {
      NetConnectedPinIterator *pin_iter = network->connectedPinIterator(net);
      while (pin_iter->hasNext()) {
	const Pin *pin = pin_iter->next();
	pins.insert(pin);
      }
      delete pin_iter;
    }
  }
  return pins;
}

bool
ExceptionThru::matches(const Pin *from_pin,
		       const Pin *to_pin,
		       const RiseFall *to_rf,
		       const Network *network)
{
  EdgePins edge_pins(from_pin, to_pin);
  return ((pins_ && to_pin && pins_->hasKey(to_pin))
	  || (edges_ && from_pin && to_pin && edges_->hasKey(edge_pins))
	  || (nets_ && to_pin && nets_->hasKey(network->net(to_pin)))
	  || (insts_ && to_pin && insts_->hasKey(network->instance(to_pin))))
    && rf_->matches(to_rf);
}

void
ExceptionThru::findHash(const Network *network)
{
  hash_ = 0;
  if (pins_) {
    size_t hash = 0;
    for (const Pin *pin : *pins_)
      hash += network->id(pin);
    hash_ += hash * hash_pin;
  }
  if (nets_) {
    size_t hash = 0;
    for (const Net *net : *nets_)
      hash += network->id(net);
    hash_ += hash * hash_net;
  }
  if (insts_) {
    size_t hash = 0;
    for (const Instance *inst : *insts_)
      hash += network->id(inst);
    hash_ += hash * hash_inst;
  }
  hash_ += rf_->index() * 13;
}

bool
ExceptionThru::equal(ExceptionThru *thru) const
{
  // Edges_ are derived from pins_ so matching pins is sufficient.
  return PinSet::equal(thru->pins_, pins_)
    && NetSet::equal(thru->nets_, nets_)
    && InstanceSet::equal(thru->insts_, insts_)
    && rf_ == thru->rf_;
}

int
ExceptionThru::compare(ExceptionPt *pt2,
		       const Network *network) const
{
  int priority_cmp = typePriority() - pt2->typePriority();
  if (priority_cmp == 0) {
    int pin_cmp = PinSet::compare(pins_, pt2->pins(), network);
    if (pin_cmp == 0) {
      int net_cmp = NetSet::compare(nets_, pt2->nets(), network);
      if (net_cmp == 0) {
	int inst_cmp = InstanceSet::compare(insts_, pt2->instances(), network);
	if (inst_cmp == 0)
	  return rf_->index() - pt2->transition()->index();
	else
	  return inst_cmp;
      }
      else
	return net_cmp;
    }
    else
      return pin_cmp;
  }
  else
    return priority_cmp;
}

void
ExceptionThru::mergeInto(ExceptionPt *pt,
                         const Network *network)
{
  if (pins_) {
    for (const Pin *pin : *pins_)
      pt->addPin(pin, network);
  }
  if (edges_) {
    for (const EdgePins &edge : *edges_)
      pt->addEdge(edge, network);
    // EdgePins are now owned by acquirer.
    edges_->clear();
  }
  if (nets_) {
    for (const Net *net : *nets_)
      pt->addNet(net, network);
  }
  if (insts_) {
    for (const Instance *inst : *insts_)
      pt->addInstance(inst, network);
  }
}

void
ExceptionThru::deleteObjects(ExceptionThru *pt,
                             const Network *network)
{
  PinSet *pins = pt->pins();
  if (pins && pins_) {
    for (const Pin *pin : *pins)
      deletePin(pin, network);
  }
  EdgePinsSet *edges = pt->edges();
  if (edges && edges_) {
    for (const EdgePins &edge : *edges)
      deleteEdge(edge);
  }
  NetSet *nets = pt->nets();
  if (nets && nets_) {
    for (const Net *net : *nets)
      deleteNet(net, network);
  }
  InstanceSet *insts = pt->instances();
  if (insts && insts_) {
    for (const Instance *inst : *insts)
      deleteInstance(inst, network);
  }
}

bool
ExceptionThru::intersectsPts(ExceptionThru *thru,
                             const Network *network) const
{
  return thru->transition() == rf_
    && ((pins_ && PinSet::intersects(pins_, thru->pins(), network))
	|| (nets_ && NetSet::intersects(nets_, thru->nets(), network))
	|| (insts_ && InstanceSet::intersects(insts_, thru->instances(), network)));
}

size_t
ExceptionThru::objectCount() const
{
  size_t count = 0;
  if (pins_)
    count += pins_->size();
  if (nets_)
    count += nets_->size();
  if (insts_)
    count += insts_->size();
  return count;
}

void
ExceptionThru::connectPinAfter(PinSet *drvrs,
			       Network *network)
{
  //  - Tricky to detect exactly what needs to be updated. In theory,
  //    at most, only edges starting/ending (pin is leaf) or spanning
  //    (pin is hier) the pin may need to be added. Trick is avoiding
  //    adding edges through the newly connected pin that don't belong.
  //  - some examples:
  //    a. leaf driver connected, with downstream hnet in nets_, only
  //       the edges from pin through hier_net should be added.
  //    b. hpin connected, but only some other hpin/hnet along the overall
  //       net resides in pins_/nets_, only add edges through those other
  //       hpin/hnets.
  //    c. hier inst resides in insts_, it gets a new pin added/connected, so
  //       should add new edges through that pin.

  // Use driver lookups to minimize potentially expensive calls that
  // traverse hier pins.

  // No enabled edges if no driver.
  if (drvrs && !drvrs->empty()) {
    if (pins_) {
      for (const Pin *thru_pin : *pins_) {
        if (network->isHierarchical(thru_pin)) {
          PinSet *thru_pin_drvrs = network->drivers(thru_pin);
          if (PinSet::intersects(drvrs, thru_pin_drvrs, network))
            makePinEdges(thru_pin, network);
        }
      }
    }
    if (insts_) {
      for (const Instance *inst : *insts_) {
        if (network->isHierarchical(inst)) {
          InstancePinIterator *inst_pin_iter = network->pinIterator(inst);
          while (inst_pin_iter->hasNext()) {
            Pin *inst_pin = inst_pin_iter->next();
            PinSet *inst_pin_drvrs = network->drivers(inst_pin);
            if (PinSet::intersects(drvrs, inst_pin_drvrs, network))
              makePinEdges(inst_pin, network);
          }
          delete inst_pin_iter;
        }
      }
    }
    if (nets_) {
      for (const Net *net : *nets_) {
        PinSet *net_drvrs = network->drivers(net);
        if (PinSet::intersects(drvrs, net_drvrs, network))
          makeNetEdges(net, network);
      }
    }
  }
}

void
ExceptionThru::makePinEdges(const Pin *pin,
			    const Network *network)
{
  if (network->isHierarchical(pin))
    makeHpinEdges(pin, network);
}

void
ExceptionThru::disconnectPinBefore(const Pin *pin,
 				   Network *network)
{
  // Remove edges from/to leaf pin and through hier pin.
  deletePinEdges(pin, network);
}

////////////////////////////////////////////////////////////////

ExceptionPtIterator::ExceptionPtIterator(const ExceptionPath *exception) :
  exception_(exception),
  from_done_(false),
  to_done_(false)
{
  if (exception->thrus())
    thru_iter_.init(exception->thrus());
}

bool
ExceptionPtIterator::hasNext()
{
  return (!from_done_ && exception_->from())
    || thru_iter_.hasNext()
    || (!to_done_ && exception_->to());
}


ExceptionPt *
ExceptionPtIterator::next()
{
  if (!from_done_ && exception_->from()) {
    from_done_ = true;
    return exception_->from();
  }
  else if (thru_iter_.hasNext())
    return thru_iter_.next();
  else {
    to_done_ = true;
    return exception_->to();
  }
}

////////////////////////////////////////////////////////////////

ExpandedExceptionVisitor::ExpandedExceptionVisitor(ExceptionPath *exception,
						   const Network *network) :
  exception_(exception),
  network_(network)
{
}

void
ExpandedExceptionVisitor::visitExpansions()
{
  ExceptionFrom *from = exception_->from();
  if (from) {
    const RiseFallBoth *rf = from->transition();
    if (from->pins()) {
      for (const Pin *pin : *from->pins()) {
        PinSet pins(network_);
        pins.insert(pin);
        ExceptionFrom expanded_from(&pins, nullptr, nullptr, rf, false, network_);
        expandThrus(&expanded_from);
      }
    }
    if (from->clks()) {
      for (Clock *clk : *from->clks()) {
        ClockSet clks;
        clks.insert(clk);
        ExceptionFrom expanded_from(nullptr, &clks, nullptr, rf, false, network_);
        expandThrus(&expanded_from);
      }
    }
    if (from->instances()) {
      for (const Instance *inst : *from->instances()) {
        InstanceSet insts(network_);
        insts.insert(inst);
        ExceptionFrom expanded_from(nullptr, nullptr, &insts, rf, false, network_);
        expandThrus(&expanded_from);
      }
    }
  }
  else
    expandThrus(0);
}

void
ExpandedExceptionVisitor::expandThrus(ExceptionFrom *expanded_from)
{
  ExceptionThruSeq *thrus = exception_->thrus();
  if (thrus) {
    // Use tail recursion to expand the exception points in the thrus.
    ExceptionThruSeq expanded_thrus;
    expandThru(expanded_from, 0, &expanded_thrus);
  }
  else
    expandTo(expanded_from, nullptr);
}

void
ExpandedExceptionVisitor::expandThru(ExceptionFrom *expanded_from,
				     size_t next_thru_idx,
				     ExceptionThruSeq *expanded_thrus)
{
  ExceptionThruSeq *thrus = exception_->thrus();
  if (next_thru_idx < thrus->size()) {
    ExceptionThru *thru = (*thrus)[next_thru_idx];
    const RiseFallBoth *rf = thru->transition();
    if (thru->pins()) {
      for (const Pin *pin : *thru->pins()) {
	PinSet pins(network_);
	pins.insert(pin);
	ExceptionThru expanded_thru(&pins, nullptr, nullptr, rf, false, network_);
	expanded_thrus->push_back(&expanded_thru);
	expandThru(expanded_from, next_thru_idx + 1, expanded_thrus);
	expanded_thrus->pop_back();
      }
    }
    if (thru->nets()) {
      for (const Net *net : *thru->nets()) {
	NetSet nets(network_);
	nets.insert(net);
	ExceptionThru expanded_thru(nullptr, &nets, nullptr, rf, false, network_);
	expanded_thrus->push_back(&expanded_thru);
	expandThru(expanded_from, next_thru_idx + 1, expanded_thrus);
	expanded_thrus->pop_back();
      }
    }
    if (thru->instances()) {
      for (const Instance *inst : *thru->instances()) {
	InstanceSet insts(network_);
	insts.insert(inst);
	ExceptionThru expanded_thru(nullptr, nullptr, &insts, rf, false, network_);
	expanded_thrus->push_back(&expanded_thru);
	expandThru(expanded_from, next_thru_idx + 1, expanded_thrus);
	expanded_thrus->pop_back();
      }
    }
  }
  else
    // End of thrus tail recursion.
    expandTo(expanded_from, expanded_thrus);
}

void
ExpandedExceptionVisitor::expandTo(ExceptionFrom *expanded_from,
				   ExceptionThruSeq *expanded_thrus)
{
  ExceptionTo *to = exception_->to();
  if (to) {
    const RiseFallBoth *rf = to->transition();
    const RiseFallBoth *end_rf = to->endTransition();
    if (to->pins()) {
      for (const Pin *pin : *to->pins()) {
        PinSet pins(network_);
        pins.insert(pin);
        ExceptionTo expanded_to(&pins, nullptr, nullptr, rf, end_rf, false, network_);
        visit(expanded_from, expanded_thrus, &expanded_to);
      }
    }
    if (to->clks()) {
      for (Clock *clk : *to->clks()) {
        ClockSet clks;
        clks.insert(clk);
        ExceptionTo expanded_to(nullptr, &clks, nullptr, rf, end_rf, false, network_);
        visit(expanded_from, expanded_thrus, &expanded_to);
      }
    }
    if (to->instances()) {
      for (const Instance *inst : *to->instances()) {
        InstanceSet insts(network_);
        insts.insert(inst);
        ExceptionTo expanded_to(nullptr, nullptr, &insts, rf, end_rf, false, network_);
        visit(expanded_from, expanded_thrus, &expanded_to);
      }
    }
  }
  else
    visit(expanded_from, expanded_thrus, nullptr);
}

////////////////////////////////////////////////////////////////

ExceptionState::ExceptionState(ExceptionPath *exception,
			       ExceptionThru *next_thru,
			       int index) :
  exception_(exception),
  next_thru_(next_thru),
  next_state_(nullptr),
  index_(index)
{
}

void
ExceptionState::setNextState(ExceptionState *next_state)
{
  next_state_ = next_state;
}

bool
ExceptionState::matchesNextThru(const Pin *from_pin,
				const Pin *to_pin,
				const RiseFall *to_rf,
				const MinMax *min_max,
				const Network *network) const
{
  // Don't advance the state if the exception is complete (no next_thru_).
  return next_thru_
    && exception_->matches(min_max, false)
    && next_thru_->matches(from_pin, to_pin, to_rf, network);
}

bool
ExceptionState::isComplete() const
{
  return next_thru_ == nullptr
    && exception_->to() == nullptr;
}

size_t
ExceptionState::hash() const
{
  return hashSum(exception_->hash(), index_);
}

bool
ExceptionStateLess::operator()(const ExceptionState *state1,
                               const ExceptionState *state2) const
{
  const ExceptionPath *except1 = state1->exception();
  const ExceptionPath *except2 = state2->exception();
    return except1->id() < except2->id()
  //return except1 < except2
    || (except1 == except2
        && state1->index() < state2->index());
}

////////////////////////////////////////////////////////////////

ExceptionPathLess::ExceptionPathLess(const Network *network) :
  network_(network)
{
}

bool
ExceptionPathLess::operator()(const ExceptionPath *except1,
                              const ExceptionPath *except2) const
{
  int priority1 = except1->typePriority() + except1->minMax()->index();
  int priority2 = except2->typePriority() + except2->minMax()->index();
  if (priority1 == priority2) {
    ExceptionPtIterator pt_iter1(except1);
    ExceptionPtIterator pt_iter2(except2);
    while (pt_iter1.hasNext() && pt_iter2.hasNext()) {
      ExceptionPt *pt1 = pt_iter1.next();
      ExceptionPt *pt2 = pt_iter2.next();
      int cmp = pt1->compare(pt2, network_);
      if (cmp != 0)
	return cmp < 0;
    }
    // Lesser has fewer exception pts.
    return !pt_iter1.hasNext() && pt_iter2.hasNext();
  }
  else
    return (priority1 < priority2);
}

////////////////////////////////////////////////////////////////

class InsertPinPairsThru : public HierPinThruVisitor
{
public:
  InsertPinPairsThru(PinPairSet *pairs,
		     const Network *network);

protected:
  virtual void visit(const Pin *drvr,
		     const Pin *load);

  PinPairSet *pairs_;
  const Network *network_;
};

InsertPinPairsThru::InsertPinPairsThru(PinPairSet *pairs,
				       const Network *network) :
  HierPinThruVisitor(),
  pairs_(pairs),
  network_(network)
{
}

void
InsertPinPairsThru::visit(const Pin *drvr,
			  const Pin *load)
{
  PinPair pair(drvr, load);
  pairs_->insert(pair);
}

static void
insertPinPairsThruHierPin(const Pin *hpin,
			  const Network *network,
			  PinPairSet *pairs)
{
  InsertPinPairsThru visitor(pairs, network);
  visitDrvrLoadsThruHierPin(hpin, network, &visitor);
}

static void
insertPinPairsThruNet(const Net *net,
		      const Network *network,
		      PinPairSet *pairs)
{
  InsertPinPairsThru visitor(pairs, network);
  visitDrvrLoadsThruNet(net, network, &visitor);
}

class DeletePinPairsThru : public HierPinThruVisitor
{
public:
  DeletePinPairsThru(PinPairSet *pairs,
		     const Network *network);

protected:
  virtual void visit(const Pin *drvr,
                     const Pin *load);

  PinPairSet *pairs_;
  const Network *network_;
};

DeletePinPairsThru::DeletePinPairsThru(PinPairSet *pairs,
				       const Network *network) :
  HierPinThruVisitor(),
  pairs_(pairs),
  network_(network)
{
}

void
DeletePinPairsThru::visit(const Pin *drvr,
			  const Pin *load)
{
  PinPair pair(drvr, load);
  pairs_->erase(pair);
}

static void
deletePinPairsThruHierPin(const Pin *hpin,
			  const Network *network,
			  PinPairSet *pairs)
{
  DeletePinPairsThru visitor(pairs, network);
  visitDrvrLoadsThruHierPin(hpin, network, &visitor);
}

} // namespace
