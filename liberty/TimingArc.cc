// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
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
  timing_type_(timing_type_combinational),
  timing_sense_(timing_sense_unknown),
  cond_(NULL),
  sdf_cond_(NULL),
  sdf_cond_start_(NULL),
  sdf_cond_end_(NULL),
  mode_name_(NULL),
  mode_value_(NULL),
  ocv_arc_depth_(0.0),
  models_{NULL, NULL}
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
  delete models_[0];
  delete models_[1];
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
TimingArcAttrs::model(TransRiseFall *tr) const
{
  return models_[tr->index()];
}

void
TimingArcAttrs::setModel(TransRiseFall *tr,
			 TimingModel *model)
{
  models_[tr->index()] = model;
}

void
TimingArcAttrs::setOcvArcDepth(float depth)
{
  ocv_arc_depth_ = depth;
}

////////////////////////////////////////////////////////////////

TimingArcSet *TimingArcSet::wire_timing_arc_set_ = NULL;

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
  from_(NULL),
  to_(NULL),
  related_out_(NULL),
  role_(role),
  cond_(NULL),
  is_cond_default_(false),
  sdf_cond_start_(NULL),
  sdf_cond_end_(NULL),
  mode_name_(NULL),
  mode_value_(NULL),
  index_(0),
  is_disabled_constraint_(false)
{
  init(NULL);
}

void
TimingArcSet::init(LibertyCell *cell)
{
  if (cell)
    index_ = cell->addTimingArcSet(this);

  TransRiseFallIterator tr_iter;
  while (tr_iter.hasNext()) {
    TransRiseFall *tr = tr_iter.next();
    int tr_index = tr->index();
    from_arc1_[tr_index] = NULL;
    from_arc2_[tr_index] = NULL;
  }
}

TimingArcSet::~TimingArcSet()
{
  arcs_.deleteContentsClear();
}

LibertyCell *
TimingArcSet::libertyCell() const
{
  if (from_)
    return from_->libertyCell();
  else
    // Wire timing arc set.
    return NULL;
}

TimingArcIndex
TimingArcSet::addTimingArc(TimingArc *arc)
{
  TimingArcIndex arc_index = arcs_.size();
  if (arc_index > timing_arc_index_max)
    internalError("timing arc max index exceeded\n");
  arcs_.push_back(arc);

  int from_tr_index = arc->fromTrans()->asRiseFall()->index();
  if (from_arc1_[from_tr_index] == NULL)
    from_arc1_[from_tr_index] = arc;
  else if (from_arc2_[from_tr_index] == NULL)
    from_arc2_[from_tr_index] = arc;

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
  int from_tr_index = arc->fromTrans()->asRiseFall()->index();
  if (from_arc1_[from_tr_index] == arc) {
    from_arc1_[from_tr_index] = from_arc2_[from_tr_index];
    from_arc2_[from_tr_index] = NULL;
  }
  else if (from_arc2_[from_tr_index] == arc)
    from_arc2_[from_tr_index] = NULL;
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
TimingArcSet::arcsFrom(const TransRiseFall *from_tr,
		       // Return values.
		       TimingArc *&arc1,
		       TimingArc *&arc2)
{
  int tr_index = from_tr->index();
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
    return timing_sense_non_unate;
}

TransRiseFall *
TimingArcSet::isRisingFallingEdge() const
{
  int arc_count = arcs_.size();
  if (arc_count == 2) {
    TransRiseFall *from_tr1 = arcs_[0]->fromTrans()->asRiseFall();
    TransRiseFall *from_tr2 = arcs_[1]->fromTrans()->asRiseFall();
    if (from_tr1 == from_tr2)
      return from_tr1;
  }
  if (arcs_.size() == 1)
    return arcs_[0]->fromTrans()->asRiseFall();
  else
    return NULL;
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
TimingArcSet::wireArcIndex(const TransRiseFall *tr)
{
  return tr->index();
}

void
TimingArcSet::init()
{
  wire_timing_arc_set_ = new TimingArcSet(TimingRole::wire());
  new TimingArc(wire_timing_arc_set_, Transition::rise(),
		Transition::rise(), NULL);
  new TimingArc(wire_timing_arc_set_, Transition::fall(),
		Transition::fall(), NULL);
}

void
TimingArcSet::destroy()
{
  delete wire_timing_arc_set_;
  wire_timing_arc_set_ = NULL;
}

////////////////////////////////////////////////////////////////

TimingArcSetArcIterator::TimingArcSetArcIterator(const TimingArcSet *set) :
  TimingArcSeq::ConstIterator(set->arcs())
{
}

////////////////////////////////////////////////////////////////

TimingArc::TimingArc(TimingArcSet *set,
		     Transition *from_tr,
		     Transition *to_tr,
		     TimingModel *model) :
  set_(set),
  from_tr_(from_tr),
  to_tr_(to_tr),
  model_(model),
  scaled_models_(NULL)
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
  if (scaled_models_ == NULL)
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
  if ((from_tr_ == Transition::rise()
       && to_tr_ == Transition::rise())
      || (from_tr_ == Transition::fall()
	  && to_tr_ == Transition::fall()))
    return timing_sense_positive_unate;
  else if ((from_tr_ == Transition::rise()
       && to_tr_ == Transition::fall())
      || (from_tr_ == Transition::fall()
	  && to_tr_ == Transition::rise()))
    return timing_sense_negative_unate;
  else
    return timing_sense_non_unate;
}

const char *
timingSenseString(TimingSense sense)
{
  static const char *sense_string[] = {"positive_unate",
				       "negative_unate",
				       "non_unate",
				       "none",
				       "unknown"};
  return sense_string[sense];
}

TimingSense
timingSenseOpposite(TimingSense sense)
{
  switch (sense) {
  case timing_sense_positive_unate:
    return timing_sense_negative_unate;
  case timing_sense_negative_unate:
    return timing_sense_positive_unate;
  case timing_sense_non_unate:
    return timing_sense_non_unate;
  case timing_sense_unknown:
    return timing_sense_unknown;
  case timing_sense_none:
    return timing_sense_none;
  }
  // Prevent warnings from lame compilers.
  return timing_sense_unknown;
}

////////////////////////////////////////////////////////////////

// Same order as enum TimingType.
static const char *timing_type_strings[] = {
  "clear",
  "combinational",
  "combinational_fall",
  "combinational_rise",
  "falling_edge",
  "hold_falling",
  "hold_rising",
  "min_pulse_width",
  "minimum_period",
  "nochange_high_high",
  "nochange_high_low",
  "nochange_low_high",
  "nochange_low_low",
  "non_seq_hold_falling",
  "non_seq_hold_rising",
  "non_seq_setup_falling",
  "non_seq_setup_rising",
  "preset",
  "recovery_falling",
  "recovery_rising",
  "removal_falling",
  "removal_rising",
  "retaining_time",
  "rising_edge",
  "setup_falling",
  "setup_rising",
  "skew_falling",
  "skew_rising",
  "three_state_disable",
  "three_state_disable_fall",
  "three_state_disable_rise",
  "three_state_enable",
  "three_state_enable_fall",
  "three_state_enable_rise",
  "min_clock_tree_path",
  "max_clock_tree_path",
  "unknown"
};

typedef Map<const char*,TimingType,CharPtrLess> TimingTypeMap;

static TimingTypeMap *timing_type_string_map = NULL;

void
makeTimingTypeMap()
{
  timing_type_string_map = new TimingTypeMap();
  int count = sizeof(timing_type_strings) / sizeof(const char*);
  for (int i = 0; i < count; i++) {
    const char *type_name = timing_type_strings[i];
    (*timing_type_string_map)[type_name] = (TimingType) i;
  }
}

void
deleteTimingTypeMap()
{
  delete timing_type_string_map;
  timing_type_string_map = NULL;
}

const char *
timingTypeString(TimingType type)
{
  return timing_type_strings[type];
}

TimingType
findTimingType(const char *type_name)
{
  TimingType type;
  bool exists;
  timing_type_string_map->findKey(type_name, type, exists);
  if (exists)
    return type;
  else
    return timing_type_unknown;
}

bool
timingTypeIsCheck(TimingType type)
{
  switch (type) {
  case timing_type_hold_falling:
  case timing_type_hold_rising:
  case timing_type_min_pulse_width:
  case timing_type_minimum_period:
  case timing_type_nochange_high_high:
  case timing_type_nochange_high_low:
  case timing_type_nochange_low_high:
  case timing_type_nochange_low_low:
  case timing_type_non_seq_hold_falling:
  case timing_type_non_seq_hold_rising:
  case timing_type_non_seq_setup_falling:
  case timing_type_non_seq_setup_rising:
  case timing_type_recovery_falling:
  case timing_type_recovery_rising:
  case timing_type_removal_falling:
  case timing_type_removal_rising:
  case timing_type_retaining_time:
  case timing_type_setup_falling:
  case timing_type_setup_rising:
  case timing_type_skew_falling:
  case timing_type_skew_rising:
    return true;
  default:
    return false;
  }
}

ScaleFactorType
timingTypeScaleFactorType(TimingType type)
{
  switch (type) {
  case timing_type_non_seq_setup_falling:
  case timing_type_non_seq_setup_rising:
  case timing_type_setup_falling:
  case timing_type_setup_rising:
    return scale_factor_setup;
  case timing_type_hold_falling:
  case timing_type_hold_rising:
  case timing_type_non_seq_hold_falling:
  case timing_type_non_seq_hold_rising:
    return scale_factor_hold;
  case timing_type_recovery_falling:
  case timing_type_recovery_rising:
    return scale_factor_recovery;
  case timing_type_removal_falling:
  case timing_type_removal_rising:
    return scale_factor_removal;
  case timing_type_skew_falling:
  case timing_type_skew_rising:
    return scale_factor_skew;
  case timing_type_minimum_period:
    return scale_factor_min_period;
  case timing_type_nochange_high_high:
  case timing_type_nochange_high_low:
  case timing_type_nochange_low_high:
  case timing_type_nochange_low_low:
    return scale_factor_nochange;
  case timing_type_min_pulse_width:
    return scale_factor_min_pulse_width;
  case timing_type_clear:
  case timing_type_combinational:
  case timing_type_combinational_fall:
  case timing_type_combinational_rise:
  case timing_type_falling_edge:
  case timing_type_preset:
  case timing_type_retaining_time:
  case timing_type_rising_edge:
  case timing_type_three_state_disable:
  case timing_type_three_state_disable_fall:
  case timing_type_three_state_disable_rise:
  case timing_type_three_state_enable:
  case timing_type_three_state_enable_fall:
  case timing_type_three_state_enable_rise:
  case timing_type_min_clock_tree_path:
  case timing_type_max_clock_tree_path:
    return scale_factor_cell;
  case timing_type_unknown:
    return scale_factor_unknown;
  }
  return scale_factor_unknown;
}

} // namespace
