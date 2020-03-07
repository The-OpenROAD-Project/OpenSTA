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
#include "EnumNameMap.hh"
#include "FuncExpr.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "TimingModel.hh"
#include "TimingArc.hh"

namespace sta {

static bool
timingArcsEquiv(const TimingArcSet *set1,
		const TimingArcSet *set2);
static bool
timingArcsLess(const TimingArcSet *set1,
	       const TimingArcSet *set2);

////////////////////////////////////////////////////////////////

TimingArcAttrs::TimingArcAttrs() :
  timing_type_(TimingType::combinational),
  timing_sense_(TimingSense::unknown),
  cond_(nullptr),
  sdf_cond_(nullptr),
  sdf_cond_start_(nullptr),
  sdf_cond_end_(nullptr),
  mode_name_(nullptr),
  mode_value_(nullptr),
  ocv_arc_depth_(0.0),
  models_{nullptr, nullptr}
{
}

// Destructor does NOT delete contents because it is a component
// of TimingGroup (that is deleted after building the LibertyCell)
// and (potentially) multiple TimingArcSets.
TimingArcAttrs::~TimingArcAttrs()
{
}

void
TimingArcAttrs::deleteContents()
{
  if (cond_)
    cond_->deleteSubexprs();
  stringDelete(sdf_cond_);
  stringDelete(sdf_cond_start_);
  stringDelete(sdf_cond_end_);
  stringDelete(mode_name_);
  stringDelete(mode_value_);
  delete models_[RiseFall::riseIndex()];
  delete models_[RiseFall::fallIndex()];
}

void
TimingArcAttrs::setTimingType(TimingType type)
{
  timing_type_ = type;
}

void
TimingArcAttrs::setTimingSense(TimingSense sense)
{
  timing_sense_ = sense;
}

void
TimingArcAttrs::setSdfCond(const char *cond)
{
  stringDelete(sdf_cond_);
  sdf_cond_ = stringCopy(cond);
}

void
TimingArcAttrs::setSdfCondStart(const char *cond)
{
  stringDelete(sdf_cond_start_);
  sdf_cond_start_ = stringCopy(cond);
}

void
TimingArcAttrs::setSdfCondEnd(const char *cond)
{
  stringDelete(sdf_cond_end_);
  sdf_cond_end_ = stringCopy(cond);
}

void
TimingArcAttrs::setModeName(const char *name)
{
  stringDelete(mode_name_);
  mode_name_ = stringCopy(name);
}

void
TimingArcAttrs::setModeValue(const char *value)
{
  stringDelete(mode_value_);
  mode_value_ = stringCopy(value);
}

TimingModel *
TimingArcAttrs::model(RiseFall *rf) const
{
  return models_[rf->index()];
}

void
TimingArcAttrs::setModel(RiseFall *rf,
			 TimingModel *model)
{
  models_[rf->index()] = model;
}

void
TimingArcAttrs::setOcvArcDepth(float depth)
{
  ocv_arc_depth_ = depth;
}

////////////////////////////////////////////////////////////////

TimingArcSet *TimingArcSet::wire_timing_arc_set_ = nullptr;

TimingArcSet::TimingArcSet(LibertyCell *cell,
			   LibertyPort *from,
			   LibertyPort *to,
			   LibertyPort *related_out,
			   TimingRole *role,
			   TimingArcAttrs *attrs) :
  from_(from),
  to_(to),
  related_out_(related_out),
  role_(role),
  cond_(attrs->cond()),
  is_cond_default_(false),
  sdf_cond_start_(attrs->sdfCondStart()),
  sdf_cond_end_(attrs->sdfCondEnd()),
  mode_name_(attrs->modeName()),
  mode_value_(attrs->modeValue()),
  ocv_arc_depth_(attrs->ocvArcDepth()),
  index_(0),
  is_disabled_constraint_(false)
{
  const char *sdf_cond = attrs->sdfCond();
  if (sdf_cond)
    sdf_cond_start_ = sdf_cond_end_ = sdf_cond;

  init(cell);
}

TimingArcSet::TimingArcSet(TimingRole *role) :
  from_(nullptr),
  to_(nullptr),
  related_out_(nullptr),
  role_(role),
  cond_(nullptr),
  is_cond_default_(false),
  sdf_cond_start_(nullptr),
  sdf_cond_end_(nullptr),
  mode_name_(nullptr),
  mode_value_(nullptr),
  index_(0),
  is_disabled_constraint_(false)
{
  init(nullptr);
}

void
TimingArcSet::init(LibertyCell *cell)
{
  if (cell)
    index_ = cell->addTimingArcSet(this);

  for (auto tr_index : RiseFall::rangeIndex()) {
    from_arc1_[tr_index] = nullptr;
    from_arc2_[tr_index] = nullptr;
  }
}

TimingArcSet::~TimingArcSet()
{
  arcs_.deleteContents();
}

LibertyCell *
TimingArcSet::libertyCell() const
{
  if (from_)
    return from_->libertyCell();
  else
    // Wire timing arc set.
    return nullptr;
}

TimingArcSetArcIterator *
TimingArcSet::timingArcIterator()
{
  return new TimingArcSetArcIterator(this);
}

TimingArcIndex
TimingArcSet::addTimingArc(TimingArc *arc)
{
  TimingArcIndex arc_index = arcs_.size();
  if (arc_index > timing_arc_index_max)
    internalError("timing arc max index exceeded\n");
  arcs_.push_back(arc);

  int from_rf_index = arc->fromTrans()->asRiseFall()->index();
  if (from_arc1_[from_rf_index] == nullptr)
    from_arc1_[from_rf_index] = arc;
  else if (from_arc2_[from_rf_index] == nullptr)
    from_arc2_[from_rf_index] = arc;

  return arc_index;
}

void
TimingArcSet::deleteTimingArc(TimingArc *arc)
{
  TimingArc *last_arc = arcs_.back();
  if (arc == last_arc)
    arcs_.pop_back();
  else {
    last_arc->setIndex(arc->index());
    arcs_[arc->index()] = last_arc;
    arcs_.pop_back();
  }
  int from_rf_index = arc->fromTrans()->asRiseFall()->index();
  if (from_arc1_[from_rf_index] == arc) {
    from_arc1_[from_rf_index] = from_arc2_[from_rf_index];
    from_arc2_[from_rf_index] = nullptr;
  }
  else if (from_arc2_[from_rf_index] == arc)
    from_arc2_[from_rf_index] = nullptr;
  delete arc;
}

TimingArc *
TimingArcSet::findTimingArc(unsigned arc_index)
{
  return arcs_[arc_index];
}

void
TimingArcSet::setRole(TimingRole *role)
{
  role_ = role;
}

void
TimingArcSet::setIsCondDefault(bool is_default)
{
  is_cond_default_ = is_default;
}

void
TimingArcSet::arcsFrom(const RiseFall *from_rf,
		       // Return values.
		       TimingArc *&arc1,
		       TimingArc *&arc2)
{
  int tr_index = from_rf->index();
  arc1 = from_arc1_[tr_index];
  arc2 = from_arc2_[tr_index];
}

TimingSense
TimingArcSet::sense() const
{
  if (arcs_.size() == 1)
    return arcs_[0]->sense();
  else if (arcs_.size() == 2 && arcs_[0]->sense() == arcs_[1]->sense())
    return arcs_[0]->sense();
  else
    return TimingSense::non_unate;
}

RiseFall *
TimingArcSet::isRisingFallingEdge() const
{
  int arc_count = arcs_.size();
  if (arc_count == 2) {
    RiseFall *from_rf1 = arcs_[0]->fromTrans()->asRiseFall();
    RiseFall *from_rf2 = arcs_[1]->fromTrans()->asRiseFall();
    if (from_rf1 == from_rf2)
      return from_rf1;
  }
  if (arcs_.size() == 1)
    return arcs_[0]->fromTrans()->asRiseFall();
  else
    return nullptr;
}

void
TimingArcSet::setIsDisabledConstraint(bool is_disabled)
{
  is_disabled_constraint_ = is_disabled;
}

float
TimingArcSet::ocvArcDepth() const
{
  if (from_) {
    if (ocv_arc_depth_ != 0.0)
      return ocv_arc_depth_;
    else {
      LibertyCell *cell = from_->libertyCell();
      float depth = cell->ocvArcDepth();
      if (depth != 0.0)
	return depth;
      else {
	float depth = cell->libertyLibrary()->ocvArcDepth();
	if (depth != 0.0)
	  return depth;
      }
    }
  }
  // Wire timing arc set.
  return 1.0;
}

bool
TimingArcSet::equiv(const TimingArcSet *set1,
		    const TimingArcSet *set2)
{
  return LibertyPort::equiv(set1->from(), set2->from())
    && LibertyPort::equiv(set1->to(), set2->to())
    && set1->role() == set2->role()
    && FuncExpr::equiv(set1->cond(), set2->cond())
    && stringEqIf(set1->sdfCond(), set2->sdfCond())
    && stringEqIf(set1->sdfCondStart(), set2->sdfCondStart())
    && stringEqIf(set1->sdfCondEnd(), set2->sdfCondEnd())
    && timingArcsEquiv(set1, set2);
}

static bool
timingArcsEquiv(const TimingArcSet *set1,
		const TimingArcSet *set2)
{
  TimingArcSetArcIterator arc_iter1(set1);
  TimingArcSetArcIterator arc_iter2(set2);
  while (arc_iter1.hasNext() && arc_iter2.hasNext()) {
    TimingArc *arc1 = arc_iter1.next();
    TimingArc *arc2 = arc_iter2.next();
    if (!TimingArc::equiv(arc1, arc2))
      return false;
  }
  return !arc_iter1.hasNext() && !arc_iter2.hasNext();
}

bool
TimingArcSet::less(const TimingArcSet *set1,
		   const TimingArcSet *set2)
{
  return timingArcSetLess(set1, set2);
}

bool
timingArcSetLess(const TimingArcSet *set1,
		 const TimingArcSet *set2)
{
  LibertyPort *from1 = set1->from();
  LibertyPort *from2 = set2->from();
  if (LibertyPort::equiv(from1, from2)) {
    LibertyPort *to1 = set1->to();
    LibertyPort *to2 = set2->to();
    if (LibertyPort::equiv(to1, to2)) {
      TimingRole *role1 = set1->role();
      TimingRole *role2 = set2->role();
      if (role1 == role2) {
	const FuncExpr *cond1 = set1->cond();
	const FuncExpr *cond2 = set2->cond();
	if (FuncExpr::equiv(cond1, cond2)) {
	  const char *sdf_cond1 = set1->sdfCond();
	  const char *sdf_cond2 = set2->sdfCond();
	  if (stringEqIf(sdf_cond1, sdf_cond2)) {
	    const char *sdf_cond_start1 = set1->sdfCondStart();
	    const char *sdf_cond_start2 = set2->sdfCondStart();
	    if (stringEqIf(sdf_cond_start1, sdf_cond_start2)) {
	      const char *sdf_cond_end1 = set1->sdfCondEnd();
	      const char *sdf_cond_end2 = set2->sdfCondEnd();
	      if (stringEqIf(sdf_cond_end1, sdf_cond_end2)) {
		const char *mode_name1 = set1->modeName();
		const char *mode_name2 = set2->modeName();
		if (stringEqIf(mode_name1, mode_name2)) {
		  const char *mode_value1 = set1->modeValue();
		  const char *mode_value2 = set2->modeValue();
		  if (stringEqIf(mode_value1, mode_value2))
		    return timingArcsLess(set1, set2);
		  else
		    return stringLessIf(mode_value1, mode_value2);
		}
		else
		  return stringLessIf(mode_name1, mode_name2);
	      }
	      else
		return stringLessIf(sdf_cond_end1, sdf_cond_end2);
	    }
	    else
	      return stringLessIf(sdf_cond_start1, sdf_cond_start2);
	  }
	  else
	    return stringLessIf(sdf_cond1, sdf_cond2);
	}
	else
	  return FuncExpr::less(cond1, cond2);
      }
      else
	return TimingRole::less(role1, role2);
    }
    else
      return LibertyPort::less(to1, to2);
  }
  else
    return LibertyPort::less(from1, from2);
}

static bool
timingArcsLess(const TimingArcSet *set1,
	       const TimingArcSet *set2)
{
  TimingArcSetArcIterator arc_iter1(set1);
  TimingArcSetArcIterator arc_iter2(set2);
  while (arc_iter1.hasNext() && arc_iter2.hasNext()) {
    TimingArc *arc1 = arc_iter1.next();
    TimingArc *arc2 = arc_iter2.next();
    int from_index1 = arc1->fromTrans()->index();
    int from_index2 = arc2->fromTrans()->index();
    if (from_index1 < from_index2)
      return true;
    if (from_index1 > from_index2)
      return false;
    // from_index1 == from_index2
    int to_index1 = arc1->toTrans()->index();
    int to_index2 = arc2->toTrans()->index();
    if (to_index1 < to_index2)
      return true;
    if (to_index1 > to_index2)
      return false;
    // Continue if arc transitions are equal.
  }
  return !arc_iter1.hasNext() && arc_iter2.hasNext();
}

////////////////////////////////////////////////////////////////

int
TimingArcSet::wireArcIndex(const RiseFall *rf)
{
  return rf->index();
}

void
TimingArcSet::init()
{
  wire_timing_arc_set_ = new TimingArcSet(TimingRole::wire());
  new TimingArc(wire_timing_arc_set_, Transition::rise(),
		Transition::rise(), nullptr);
  new TimingArc(wire_timing_arc_set_, Transition::fall(),
		Transition::fall(), nullptr);
}

void
TimingArcSet::destroy()
{
  delete wire_timing_arc_set_;
  wire_timing_arc_set_ = nullptr;
}

////////////////////////////////////////////////////////////////

TimingArcSetArcIterator::TimingArcSetArcIterator(const TimingArcSet *set) :
  TimingArcSeq::ConstIterator(set->arcs())
{
}

////////////////////////////////////////////////////////////////

TimingArc::TimingArc(TimingArcSet *set,
		     Transition *from_rf,
		     Transition *to_rf,
		     TimingModel *model) :
  set_(set),
  from_rf_(from_rf),
  to_rf_(to_rf),
  model_(model),
  scaled_models_(nullptr)
{
  index_ = set->addTimingArc(this);
}

TimingArc::~TimingArc()
{
  // The models referenced by scaled_models_ are owned by the scaled
  // cells and are deleted by ~LibertyCell.
  delete scaled_models_;
}

TimingModel *
TimingArc::model(const OperatingConditions *op_cond) const
{
  if (scaled_models_) {
    TimingModel *model = scaled_models_->findKey(op_cond);
    if (model)
      return model;
    else
      return model_;
  }
  else
    return model_;
}

void
TimingArc::addScaledModel(const OperatingConditions *op_cond,
			  TimingModel *scaled_model)
{
  if (scaled_models_ == nullptr)
    scaled_models_ = new ScaledTimingModelMap;
  (*scaled_models_)[op_cond] = scaled_model;
}

bool
TimingArc::equiv(const TimingArc *arc1,
		 const TimingArc *arc2)
{
  return arc1->fromTrans() == arc2->fromTrans()
    && arc1->toTrans() == arc2->toTrans();
}

void
TimingArc::setIndex(unsigned index)
{
  index_ = index;
}

TimingArc *
TimingArc::cornerArc(int ap_index)
{
  if (ap_index < static_cast<int>(corner_arcs_.size())) {
    TimingArc *corner_arc = corner_arcs_[ap_index];
    if (corner_arc)
      return corner_arc;
  }
  return this;
}

void
TimingArc::setCornerArc(TimingArc *corner_arc,
			int ap_index)
{
  if (ap_index >= static_cast<int>(corner_arcs_.size()))
    corner_arcs_.resize(ap_index + 1);
  corner_arcs_[ap_index] = corner_arc;
}

////////////////////////////////////////////////////////////////

TimingSense
TimingArc::sense() const
{
  if ((from_rf_ == Transition::rise()
       && to_rf_ == Transition::rise())
      || (from_rf_ == Transition::fall()
	  && to_rf_ == Transition::fall()))
    return TimingSense::positive_unate;
  else if ((from_rf_ == Transition::rise()
       && to_rf_ == Transition::fall())
      || (from_rf_ == Transition::fall()
	  && to_rf_ == Transition::rise()))
    return TimingSense::negative_unate;
  else
    return TimingSense::non_unate;
}

static EnumNameMap<TimingSense> timing_sense_name_map =
  {{TimingSense::positive_unate, "positive_unate"},
   {TimingSense::negative_unate, "negative_unate"},
   {TimingSense::non_unate, "non_unate"},
   {TimingSense::none, "none"},
   {TimingSense::unknown, "unknown"}
  };

const char *
timingSenseString(TimingSense sense)
{
  return timing_sense_name_map.find(sense);
}

TimingSense
timingSenseOpposite(TimingSense sense)
{
  switch (sense) {
  case TimingSense::positive_unate:
    return TimingSense::negative_unate;
  case TimingSense::negative_unate:
    return TimingSense::positive_unate;
  case TimingSense::non_unate:
    return TimingSense::non_unate;
  case TimingSense::unknown:
    return TimingSense::unknown;
  case TimingSense::none:
    return TimingSense::none;
  }
  // Prevent warnings from lame compilers.
  return TimingSense::unknown;
}

////////////////////////////////////////////////////////////////

// Same order as enum TimingType.
EnumNameMap<TimingType> timing_type_name_map =
  {{TimingType::clear, "clear"},
   {TimingType::combinational, "combinational"},
   {TimingType::combinational_fall, "combinational_fall"},
   {TimingType::combinational_rise, "combinational_rise"},
   {TimingType::falling_edge, "falling_edge"},
   {TimingType::hold_falling, "hold_falling"},
   {TimingType::hold_rising, "hold_rising"},
   {TimingType::min_pulse_width, "min_pulse_width"},
   {TimingType::minimum_period, "minimum_period"},
   {TimingType::nochange_high_high, "nochange_high_high"},
   {TimingType::nochange_high_low, "nochange_high_low"},
   {TimingType::nochange_low_high, "nochange_low_high"},
   {TimingType::nochange_low_low, "nochange_low_low"},
   {TimingType::non_seq_hold_falling, "non_seq_hold_falling"},
   {TimingType::non_seq_hold_rising, "non_seq_hold_rising"},
   {TimingType::non_seq_setup_falling, "non_seq_setup_falling"},
   {TimingType::non_seq_setup_rising, "non_seq_setup_rising"},
   {TimingType::preset, "preset"},
   {TimingType::recovery_falling, "recovery_falling"},
   {TimingType::recovery_rising, "recovery_rising"},
   {TimingType::removal_falling, "removal_falling"},
   {TimingType::removal_rising, "removal_rising"},
   {TimingType::retaining_time, "retaining_time"},
   {TimingType::rising_edge, "rising_edge"},
   {TimingType::setup_falling, "setup_falling"},
   {TimingType::setup_rising, "setup_rising"},
   {TimingType::skew_falling, "skew_falling"},
   {TimingType::skew_rising, "skew_rising"},
   {TimingType::three_state_disable, "three_state_disable"},
   {TimingType::three_state_disable_fall, "three_state_disable_fall"},
   {TimingType::three_state_disable_rise, "three_state_disable_rise"},
   {TimingType::three_state_enable, "three_state_enable"},
   {TimingType::three_state_enable_fall, "three_state_enable_fall"},
   {TimingType::three_state_enable_rise, "three_state_enable_rise"},
   {TimingType::min_clock_tree_path, "min_clock_tree_path"},
   {TimingType::max_clock_tree_path, "max_clock_tree_path"},
   {TimingType::unknown, "unknown"}
  };

const char *
timingTypeString(TimingType type)
{
  return timing_type_name_map.find(type);
}

TimingType
findTimingType(const char *type_name)
{
  return timing_type_name_map.find(type_name, TimingType::unknown);
}

bool
timingTypeIsCheck(TimingType type)
{
  switch (type) {
  case TimingType::hold_falling:
  case TimingType::hold_rising:
  case TimingType::min_pulse_width:
  case TimingType::minimum_period:
  case TimingType::nochange_high_high:
  case TimingType::nochange_high_low:
  case TimingType::nochange_low_high:
  case TimingType::nochange_low_low:
  case TimingType::non_seq_hold_falling:
  case TimingType::non_seq_hold_rising:
  case TimingType::non_seq_setup_falling:
  case TimingType::non_seq_setup_rising:
  case TimingType::recovery_falling:
  case TimingType::recovery_rising:
  case TimingType::removal_falling:
  case TimingType::removal_rising:
  case TimingType::retaining_time:
  case TimingType::setup_falling:
  case TimingType::setup_rising:
  case TimingType::skew_falling:
  case TimingType::skew_rising:
    return true;
  default:
    return false;
  }
}

ScaleFactorType
timingTypeScaleFactorType(TimingType type)
{
  switch (type) {
  case TimingType::non_seq_setup_falling:
  case TimingType::non_seq_setup_rising:
  case TimingType::setup_falling:
  case TimingType::setup_rising:
    return ScaleFactorType::setup;
  case TimingType::hold_falling:
  case TimingType::hold_rising:
  case TimingType::non_seq_hold_falling:
  case TimingType::non_seq_hold_rising:
    return ScaleFactorType::hold;
  case TimingType::recovery_falling:
  case TimingType::recovery_rising:
    return ScaleFactorType::recovery;
  case TimingType::removal_falling:
  case TimingType::removal_rising:
    return ScaleFactorType::removal;
  case TimingType::skew_falling:
  case TimingType::skew_rising:
    return ScaleFactorType::skew;
  case TimingType::minimum_period:
    return ScaleFactorType::min_period;
  case TimingType::nochange_high_high:
  case TimingType::nochange_high_low:
  case TimingType::nochange_low_high:
  case TimingType::nochange_low_low:
    return ScaleFactorType::nochange;
  case TimingType::min_pulse_width:
    return ScaleFactorType::min_pulse_width;
  case TimingType::clear:
  case TimingType::combinational:
  case TimingType::combinational_fall:
  case TimingType::combinational_rise:
  case TimingType::falling_edge:
  case TimingType::preset:
  case TimingType::retaining_time:
  case TimingType::rising_edge:
  case TimingType::three_state_disable:
  case TimingType::three_state_disable_fall:
  case TimingType::three_state_disable_rise:
  case TimingType::three_state_enable:
  case TimingType::three_state_enable_fall:
  case TimingType::three_state_enable_rise:
  case TimingType::min_clock_tree_path:
  case TimingType::max_clock_tree_path:
    return ScaleFactorType::cell;
  case TimingType::unknown:
    return ScaleFactorType::unknown;
  }
  return ScaleFactorType::unknown;
}

} // namespace
