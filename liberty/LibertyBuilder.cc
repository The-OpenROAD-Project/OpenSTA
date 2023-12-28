// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include "LibertyBuilder.hh"

#include "PortDirection.hh"
#include "TimingRole.hh"
#include "FuncExpr.hh"
#include "TimingArc.hh"
#include "InternalPower.hh"
#include "LeakagePower.hh"
#include "Sequential.hh"
#include "Liberty.hh"

namespace sta {

void
LibertyBuilder::init(Debug *debug,
                     Report *report)
{
  debug_ = debug;
  report_ = report;
}

LibertyCell *
LibertyBuilder::makeCell(LibertyLibrary *library,
			 const char *name,
			 const char *filename)
{
  LibertyCell *cell = new LibertyCell(library, name, filename);
  library->addCell(cell);
  return cell;
}

LibertyPort *
LibertyBuilder::makePort(LibertyCell *cell,
			 const char *port_name)
{
  string sta_name = portLibertyToSta(port_name);
  LibertyPort *port = new LibertyPort(cell, sta_name.c_str(), false, nullptr,
                                      -1, -1, false, nullptr);
  cell->addPort(port);
  return port;
}

LibertyPort *
LibertyBuilder::makeBusPort(LibertyCell *cell,
			    const char *bus_name,
                            int from_index,
			    int to_index,
			    BusDcl *bus_dcl)
{
  string sta_name = portLibertyToSta(bus_name);
  LibertyPort *port = new LibertyPort(cell, sta_name.c_str(), true, bus_dcl,
                                      from_index, to_index,
				      false, new ConcretePortSeq);
  cell->addPort(port);
  makeBusPortBits(cell->library(), cell, port, sta_name.c_str(), from_index, to_index);
  return port;
}

void
LibertyBuilder::makeBusPortBits(ConcreteLibrary *library,
				LibertyCell *cell,
				ConcretePort *bus_port,
				const char *bus_name,
				int from_index,
				int to_index)
{
  if (from_index < to_index) {
    for (int index = from_index; index <= to_index; index++)
      makeBusPortBit(library, cell, bus_port, bus_name, index);
  }
  else {
    for (int index = from_index; index >= to_index; index--)
      makeBusPortBit(library, cell, bus_port, bus_name, index);
  }
}

void
LibertyBuilder::makeBusPortBit(ConcreteLibrary *library,
			       LibertyCell *cell,
			       ConcretePort *bus_port,
			       const char *bus_name,
			       int bit_index)
{
  string bit_name;
  stringPrint(bit_name, "%s%c%d%c",
              bus_name,
              library->busBrktLeft(),
              bit_index,
              library->busBrktRight());
  LibertyPort *port = makePort(cell, bit_name.c_str(), bit_index);
  bus_port->addPortBit(port);
  cell->addPortBit(port);
}

LibertyPort *
LibertyBuilder::makePort(LibertyCell *cell,
			 const char *bit_name,
			 int bit_index)
{
  LibertyPort *port = new LibertyPort(cell, bit_name, false, nullptr,
				      bit_index, bit_index, false, nullptr);
  return port;
}

LibertyPort *
LibertyBuilder::makeBundlePort(LibertyCell *cell,
			       const char *name,
			       ConcretePortSeq *members)
{
  LibertyPort *port = new LibertyPort(cell, name, false, nullptr, -1, -1, true, members);
  cell->addPort(port);
  return port;
}

////////////////////////////////////////////////////////////////

TimingArcSet *
LibertyBuilder::makeTimingArcs(LibertyCell *cell,
			       LibertyPort *from_port,
			       LibertyPort *to_port,
			       LibertyPort *related_out,
			       TimingArcAttrsPtr attrs,
                               int /* line */)
{
  FuncExpr *to_func = to_port->function();
  Sequential *seq = (to_func && to_func->port())
    ? cell->outputPortSequential(to_func->port())
    : nullptr;
  TimingType timing_type = attrs->timingType();
  // Register/latch timing group missing timing_type.
  if (attrs->timingType() == TimingType::combinational
      && seq) {
    if (seq->clock() && seq->clock()->hasPort(from_port)) {
      switch (seq->clock()->portTimingSense(from_port)) {
      case TimingSense::positive_unate:
        timing_type = TimingType::rising_edge;
        break;
      case TimingSense::negative_unate:
        timing_type = TimingType::rising_edge;
        break;
      default:
        break;
      }
    }
    else if (seq->clear() && seq->clear()->hasPort(from_port)) {
      timing_type = TimingType::clear;
      if (attrs->timingSense() == TimingSense::unknown) {
        // Missing timing_sense also.
        TimingSense timing_sense = seq->clear()->portTimingSense(from_port);
        attrs->setTimingSense(timing_sense);
      }
    }
    else if (seq->preset() && seq->preset()->hasPort(from_port)) {
      timing_type = TimingType::preset;
      if (attrs->timingSense() == TimingSense::unknown) {
        // Missing timing_sense also.
        TimingSense timing_sense = seq->preset()->portTimingSense(from_port);
        attrs->setTimingSense(timing_sense);
      }
    }
  }
  switch (timing_type) {
  case TimingType::combinational:
    if (seq
        && seq->isLatch()
        && seq->data()->hasPort(from_port))
      // Latch D->Q timing arcs.
      return makeLatchDtoQArcs(cell, from_port, to_port,
                               seq->data()->portTimingSense(from_port),
                               related_out, attrs);
    else
      return makeCombinationalArcs(cell, from_port, to_port, related_out,
				   true, true, attrs);
  case TimingType::combinational_fall:
    return makeCombinationalArcs(cell, from_port, to_port, related_out,
				 false, true, attrs);
  case TimingType::combinational_rise:
    return makeCombinationalArcs(cell, from_port, to_port, related_out,
				 true, false, attrs);
  case TimingType::setup_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::rise(), TimingRole::setup(),
				  attrs);
  case TimingType::setup_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::fall(), TimingRole::setup(),
				  attrs);
  case TimingType::hold_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::rise(), TimingRole::hold(),
				  attrs);
  case TimingType::hold_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::fall(), TimingRole::hold(),
				  attrs);
  case TimingType::rising_edge:
    return makeRegLatchArcs(cell, from_port, to_port, related_out,
			    RiseFall::rise(), attrs);
  case TimingType::falling_edge:
    return makeRegLatchArcs(cell, from_port, to_port, related_out,
			    RiseFall::fall(), attrs);
  case TimingType::preset:
    return makePresetClrArcs(cell, from_port, to_port, related_out,
			     RiseFall::rise(), attrs);
  case TimingType::clear:
    return makePresetClrArcs(cell, from_port, to_port, related_out,
			     RiseFall::fall(), attrs);
  case TimingType::recovery_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::rise(),TimingRole::recovery(),
				  attrs);
  case TimingType::recovery_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::fall(),TimingRole::recovery(),
				  attrs);
  case TimingType::removal_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::rise(), TimingRole::removal(),
				  attrs);
  case TimingType::removal_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::fall(), TimingRole::removal(),
				  attrs);
  case TimingType::three_state_disable:
    return makeTristateDisableArcs(cell, from_port, to_port, related_out,
				   true, true, attrs);
  case TimingType::three_state_disable_fall:
    return makeTristateDisableArcs(cell, from_port, to_port, related_out,
				   false, true, attrs);
  case TimingType::three_state_disable_rise:
    return makeTristateDisableArcs(cell, from_port, to_port, related_out,
				   true, false, attrs);
  case TimingType::three_state_enable:
    return makeTristateEnableArcs(cell, from_port, to_port, related_out,
				  true, true, attrs);
  case TimingType::three_state_enable_fall:
    return makeTristateEnableArcs(cell, from_port, to_port, related_out,
				  false, true, attrs);
  case TimingType::three_state_enable_rise:
    return makeTristateEnableArcs(cell, from_port, to_port, related_out,
				  true, false, attrs);
  case TimingType::skew_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::fall(), TimingRole::skew(),
				  attrs);
  case TimingType::skew_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::rise(), TimingRole::skew(),
				  attrs);
  case TimingType::non_seq_setup_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::rise(),
				  TimingRole::nonSeqSetup(), attrs);
  case TimingType::non_seq_setup_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::fall(),
				  TimingRole::nonSeqSetup(), attrs);
  case TimingType::non_seq_hold_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::rise(),
				  TimingRole::nonSeqHold(),
				  attrs);
  case TimingType::non_seq_hold_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  RiseFall::fall(),
				  TimingRole::nonSeqHold(),
				  attrs);
  case TimingType::min_clock_tree_path:
    return makeClockTreePathArcs(cell, to_port, related_out,
                                 TimingRole::clockTreePathMin(),
                                 attrs);
  case TimingType::max_clock_tree_path:
    return makeClockTreePathArcs(cell, to_port, related_out,
                                 TimingRole::clockTreePathMax(),
                                 attrs);
  case TimingType::min_pulse_width:
  case TimingType::minimum_period:
  case TimingType::nochange_high_high:
  case TimingType::nochange_high_low:
  case TimingType::nochange_low_high:
  case TimingType::nochange_low_low:
  case TimingType::retaining_time:
  case TimingType::unknown:
    return nullptr;
  }
  // Prevent warnings from lame compilers.
  return nullptr;
}

TimingArcSet *
LibertyBuilder::makeCombinationalArcs(LibertyCell *cell,
				      LibertyPort *from_port,
				      LibertyPort *to_port,
				      LibertyPort *related_out,
				      bool to_rise,
				      bool to_fall,
				      TimingArcAttrsPtr attrs)
{
  FuncExpr *func = to_port->function();
  FuncExpr *enable = to_port->tristateEnable();
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port, related_out,
					   TimingRole::combinational(), attrs);
  TimingSense sense = attrs->timingSense();
  if (sense == TimingSense::unknown) {
    // Timing sense not specified - find it from function.
    if (func && func->hasPort(from_port))
      sense = func->portTimingSense(from_port);
    // Check tristate enable.
    else if (to_port->direction()->isAnyTristate()
             && enable
             && enable->hasPort(from_port))
      sense = TimingSense::non_unate;
    // Don't warn for functions that reference ff/latch/lut internal ports.
    //else if (func->port() && !func->port()->direction()->isInternal())
    //  report_->fileWarn(172, cell->filename(), line,
    //                    "timing sense cannot be inferred because pin function does not reference related pin %s.",
    //                   from_port->name());
  }

  TimingModel *model;
  RiseFall *to_rf;
  switch (sense) {
  case TimingSense::positive_unate:
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, RiseFall::rise(), to_rf, model);
    }
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, RiseFall::fall(), to_rf, model);
    }
    break;
  case TimingSense::negative_unate:
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, RiseFall::rise(), to_rf, model);
    }
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, RiseFall::fall(), to_rf, model);
    }
    break;
  case TimingSense::non_unate:
  case TimingSense::unknown:
  case TimingSense::none:
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model) {
	makeTimingArc(arc_set, RiseFall::fall(), to_rf, model);
	makeTimingArc(arc_set, RiseFall::rise(), to_rf, model);
      }
    }
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model) {
	makeTimingArc(arc_set, RiseFall::rise(), to_rf, model);
	makeTimingArc(arc_set, RiseFall::fall(), to_rf, model);
      }
    }
    break;
  }
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makeLatchDtoQArcs(LibertyCell *cell,
				  LibertyPort *from_port,
				  LibertyPort *to_port,
                                  TimingSense sense,
				  LibertyPort *related_out,
				  TimingArcAttrsPtr attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port,
					   related_out,
					   TimingRole::latchDtoQ(), attrs);
  TimingModel *model;
  RiseFall *to_rf = RiseFall::rise();
  model = attrs->model(to_rf);
  if (model) {
    RiseFall *from_rf = (sense == TimingSense::negative_unate) ?
      to_rf->opposite() : to_rf;
    makeTimingArc(arc_set, from_rf, to_rf, model);
  }
  to_rf = RiseFall::fall();
  model = attrs->model(to_rf);
  if (model) {
    RiseFall *from_rf = (sense == TimingSense::negative_unate) ?
      to_rf->opposite() : to_rf;
    makeTimingArc(arc_set, from_rf, to_rf, model);
  }
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makeRegLatchArcs(LibertyCell *cell,
				 LibertyPort *from_port,
				 LibertyPort *to_port,
				 LibertyPort *related_out,
				 RiseFall *from_rf,
				 TimingArcAttrsPtr attrs)
{
  FuncExpr *to_func = to_port->function();
  FuncExprPortIterator port_iter(to_func);
  while (port_iter.hasNext()) {
    LibertyPort *func_port = port_iter.next();
    Sequential *seq = cell->outputPortSequential(func_port);
    if (seq) {
      if (seq->clock() && seq->clock()->hasPort(from_port)) {
	TimingRole *role = seq->isRegister() ?
	  TimingRole::regClkToQ() : TimingRole::latchEnToQ();
	return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				      from_rf, role, attrs);
      }
      else if (seq->isLatch()
	       && seq->data()
	       && seq->data()->hasPort(from_port))
	return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				      from_rf, TimingRole::latchDtoQ(), attrs);
      else if ((seq->clear() && seq->clear()->hasPort(from_port))
	       || (seq->preset() && seq->preset()->hasPort(from_port)))
	return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				      from_rf, TimingRole::regSetClr(), attrs);
    }
  }
  // No associated ff/latch - assume register clk->q.
  cell->setHasInferedRegTimingArcs(true);
  return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				from_rf, TimingRole::regClkToQ(), attrs);
}

TimingArcSet *
LibertyBuilder::makeFromTransitionArcs(LibertyCell *cell,
				       LibertyPort *from_port,
				       LibertyPort *to_port,
				       LibertyPort *related_out,
				       RiseFall *from_rf,
				       TimingRole *role,
				       TimingArcAttrsPtr attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port,
					   related_out, role, attrs);
  for (auto to_rf : RiseFall::range()) {
    TimingModel *model = attrs->model(to_rf);
    if (model)
      makeTimingArc(arc_set, from_rf, to_rf, model);
  }
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makePresetClrArcs(LibertyCell *cell,
				  LibertyPort *from_port,
				  LibertyPort *to_port,
				  LibertyPort *related_out,
				  RiseFall *to_rf,
				  TimingArcAttrsPtr attrs)
{
  TimingArcSet *arc_set = nullptr;
  TimingModel *model = attrs->model(to_rf);
  if (model) {
    arc_set = makeTimingArcSet(cell, from_port, to_port, related_out,
			       TimingRole::regSetClr(), attrs);
    RiseFall *opp_rf = to_rf->opposite();
    switch (attrs->timingSense()) {
    case TimingSense::positive_unate:
      makeTimingArc(arc_set, to_rf, to_rf, model);
      break;
    case TimingSense::negative_unate:
      makeTimingArc(arc_set, opp_rf, to_rf, model);
      break;
    case TimingSense::non_unate:
    case TimingSense::unknown:
      makeTimingArc(arc_set, to_rf, to_rf, model);
      makeTimingArc(arc_set, opp_rf, to_rf, model);
      break;
    case TimingSense::none:
      break;
    }
  }
  return arc_set;
}

// To rise/fall for Z transitions is as follows:
//  0Z, Z1 rise
//  1Z, Z0 fall
TimingArcSet *
LibertyBuilder::makeTristateEnableArcs(LibertyCell *cell,
				       LibertyPort *from_port,
				       LibertyPort *to_port,
				       LibertyPort *related_out,
				       bool to_rise,
				       bool to_fall,
				       TimingArcAttrsPtr attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port, related_out,
					   TimingRole::tristateEnable(), attrs);
  FuncExpr *tristate_enable = to_port->tristateEnable();
  TimingSense sense = attrs->timingSense();
  if (sense == TimingSense::unknown && tristate_enable)
    sense = tristate_enable->portTimingSense(from_port);
  TimingModel *model;
  RiseFall *to_rf;
  switch (sense) {
  case TimingSense::positive_unate:
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, Transition::rise(), Transition::trZ1(), model);
    }
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, Transition::rise(), Transition::trZ0(), model);
    }
    break;
  case TimingSense::negative_unate:
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, Transition::fall(), Transition::trZ1(), model);
    }
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, Transition::fall(), Transition::trZ0(), model);
    }
    break;
  case TimingSense::non_unate:
  case TimingSense::unknown:
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model) {
	makeTimingArc(arc_set, Transition::rise(), Transition::trZ1(), model);
	makeTimingArc(arc_set, Transition::fall(), Transition::trZ1(), model);
      }
    }
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model) {
	makeTimingArc(arc_set, Transition::rise(), Transition::trZ0(), model);
	makeTimingArc(arc_set, Transition::fall(), Transition::trZ0(), model);
      }
    }
    break;
  case TimingSense::none:
    break;
  }
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makeTristateDisableArcs(LibertyCell *cell,
					LibertyPort *from_port,
					LibertyPort *to_port,
					LibertyPort *related_out,
					bool to_rise,
					bool to_fall,
					TimingArcAttrsPtr attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port,
					   related_out,
					   TimingRole::tristateDisable(),
					   attrs);
  TimingSense sense = attrs->timingSense();
  FuncExpr *tristate_enable = to_port->tristateEnable();
  if (sense == TimingSense::unknown && tristate_enable)
    sense = timingSenseOpposite(tristate_enable->portTimingSense(from_port));
  TimingModel *model;
  RiseFall *to_rf;
  switch (sense) {
  case TimingSense::positive_unate:
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, Transition::rise(), Transition::tr0Z(), model);
    }
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, Transition::rise(), Transition::tr1Z(), model);
    }
    break;
  case TimingSense::negative_unate:
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, Transition::fall(), Transition::tr0Z(), model);
    }
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model)
	makeTimingArc(arc_set, Transition::fall(), Transition::tr1Z(), model);
    }
    break;
  case TimingSense::non_unate:
  case TimingSense::unknown:
    if (to_rise) {
      to_rf = RiseFall::rise();
      model = attrs->model(to_rf);
      if (model) {
	makeTimingArc(arc_set, Transition::fall(), Transition::tr0Z(), model);
	makeTimingArc(arc_set, Transition::rise(), Transition::tr0Z(), model);
      }
    }
    if (to_fall) {
      to_rf = RiseFall::fall();
      model = attrs->model(to_rf);
      if (model) {
	makeTimingArc(arc_set, Transition::fall(), Transition::tr1Z(), model);
	makeTimingArc(arc_set, Transition::rise(), Transition::tr1Z(), model);
      }
    }
    break;
  case TimingSense::none:
    break;
  }
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makeClockTreePathArcs(LibertyCell *cell,
				       LibertyPort *to_port,
				       LibertyPort *related_out,
				       TimingRole *role,
				       TimingArcAttrsPtr attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, nullptr, to_port,
					   related_out, role, attrs);
  for (auto to_rf : RiseFall::range()) {
    TimingModel *model = attrs->model(to_rf);
    if (model)
      makeTimingArc(arc_set, nullptr, to_rf, model);
  }
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makeTimingArcSet(LibertyCell *cell,
				 LibertyPort *from,
				 LibertyPort *to,
				 LibertyPort *related_out,
				 TimingRole *role,
				 TimingArcAttrsPtr attrs)
{
  return new TimingArcSet(cell, from, to, related_out, role, attrs);
}

TimingArc *
LibertyBuilder::makeTimingArc(TimingArcSet *set,
			      RiseFall *from_rf,
			      RiseFall *to_rf,
			      TimingModel *model)
{
  return new TimingArc(set, from_rf ? from_rf->asTransition() : nullptr,
		       to_rf->asTransition(), model);
}

TimingArc *
LibertyBuilder::makeTimingArc(TimingArcSet *set,
			      Transition *from_rf,
			      Transition *to_rf,
			      TimingModel *model)
{
  return new TimingArc(set, from_rf, to_rf, model);
}

////////////////////////////////////////////////////////////////

InternalPower *
LibertyBuilder::makeInternalPower(LibertyCell *cell,
				  LibertyPort *port,
				  LibertyPort *related_port,
				  InternalPowerAttrs *attrs)
{
  return new InternalPower(cell, port, related_port, attrs);
}

LeakagePower *
LibertyBuilder::makeLeakagePower(LibertyCell *cell,
				 LeakagePowerAttrs *attrs)
{
  return new LeakagePower(cell, attrs);
}

} // namespace
