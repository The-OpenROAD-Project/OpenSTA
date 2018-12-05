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
#include "PortDirection.hh"
#include "TimingRole.hh"
#include "FuncExpr.hh"
#include "TimingArc.hh"
#include "InternalPower.hh"
#include "LeakagePower.hh"
#include "Sequential.hh"
#include "Liberty.hh"
#include "LibertyBuilder.hh"

namespace sta {

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
			 const char *name)
{
  LibertyPort *port = new LibertyPort(cell, name, false, -1, -1, false, NULL);
  cell->addPort(port);
  return port;
}

LibertyPort *
LibertyBuilder::makeBusPort(LibertyCell *cell,
			    const char *name,
			    int from_index,
			    int to_index)
{
  LibertyPort *port = new LibertyPort(cell, name, true, from_index, to_index,
				      false, new ConcretePortSeq);
  cell->addPort(port);
  makeBusPortBits(cell->library(), cell, port, name, from_index, to_index);
  return port;
}

void
LibertyBuilder::makeBusPortBits(ConcreteLibrary *library,
				LibertyCell *cell,
				ConcretePort *bus_port,
				const char *name,
				int from_index,
				int to_index)
{
  if (from_index < to_index) {
    for (int index = from_index; index <= to_index; index++)
      makeBusPortBit(library, cell, bus_port, name, index);
  }
  else {
    for (int index = from_index; index >= to_index; index--)
      makeBusPortBit(library, cell, bus_port, name, index);
  }
}

void
LibertyBuilder::makeBusPortBit(ConcreteLibrary *library,
			       LibertyCell *cell,
			       ConcretePort *bus_port,
			       const char *bus_name,
			       int bit_index)
{
  char *bit_name = stringPrintTmp(strlen(bus_name) + 6, "%s%c%d%c",
				  bus_name,
				  library->busBrktLeft(),
				  bit_index,
				  library->busBrktRight());
  ConcretePort *port = makePort(cell, bit_name, bit_index);
  bus_port->addPortBit(port);
  cell->addPortBit(port);
}

ConcretePort *
LibertyBuilder::makePort(LibertyCell *cell,
			 const char *bit_name,
			 int bit_index)
{
  ConcretePort *port = new LibertyPort(cell, bit_name, false,
				       bit_index, bit_index, false, NULL);
  return port;
}

LibertyPort *
LibertyBuilder::makeBundlePort(LibertyCell *cell,
			       const char *name,
			       ConcretePortSeq *members)
{
  LibertyPort *port = new LibertyPort(cell, name, false, -1, -1, true, members);
  cell->addPort(port);
  return port;
}

////////////////////////////////////////////////////////////////

TimingArcSet *
LibertyBuilder::makeTimingArcs(LibertyCell *cell,
			       LibertyPort *from_port,
			       LibertyPort *to_port,
			       LibertyPort *related_out,
			       TimingArcAttrs *attrs)
{
  FuncExpr *to_func;
  Sequential *seq = NULL;
  switch (attrs->timingType()) {
  case timing_type_combinational:
    to_func = to_port->function();
    if (to_func && to_func->op() == FuncExpr::op_port)
      seq = cell->outputPortSequential(to_func->port());
    if (seq && seq->isLatch())
      return makeLatchDtoQArcs(cell, from_port, to_port, related_out, attrs);
    else
      return makeCombinationalArcs(cell, from_port, to_port, related_out,
				   true, true, attrs);
  case timing_type_combinational_fall:
    return makeCombinationalArcs(cell, from_port, to_port, related_out,
				 false, true, attrs);
  case timing_type_combinational_rise:
    return makeCombinationalArcs(cell, from_port, to_port, related_out,
				 true, false, attrs);
  case timing_type_setup_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::rise(), TimingRole::setup(),
				  attrs);
  case timing_type_setup_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::fall(), TimingRole::setup(),
				  attrs);
  case timing_type_hold_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::rise(), TimingRole::hold(),
				  attrs);
  case timing_type_hold_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::fall(), TimingRole::hold(),
				  attrs);
  case timing_type_rising_edge:
    return makeRegLatchArcs(cell, from_port, to_port, related_out,
			    TransRiseFall::rise(), attrs);
  case timing_type_falling_edge:
    return makeRegLatchArcs(cell, from_port, to_port, related_out,
			    TransRiseFall::fall(), attrs);
  case timing_type_preset:
    return makePresetClrArcs(cell, from_port, to_port, related_out,
			     TransRiseFall::rise(), attrs);
  case timing_type_clear:
    return makePresetClrArcs(cell, from_port, to_port, related_out,
			     TransRiseFall::fall(), attrs);
  case timing_type_recovery_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::rise(),TimingRole::recovery(),
				  attrs);
  case timing_type_recovery_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::fall(),TimingRole::recovery(),
				  attrs);
  case timing_type_removal_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::rise(), TimingRole::removal(),
				  attrs);
  case timing_type_removal_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::fall(), TimingRole::removal(),
				  attrs);
  case timing_type_three_state_disable:
    return makeTristateDisableArcs(cell, from_port, to_port, related_out,
				   true, true, attrs);
  case timing_type_three_state_disable_fall:
    return makeTristateDisableArcs(cell, from_port, to_port, related_out,
				   false, true, attrs);
  case timing_type_three_state_disable_rise:
    return makeTristateDisableArcs(cell, from_port, to_port, related_out,
				   true, false, attrs);
  case timing_type_three_state_enable:
    return makeTristateEnableArcs(cell, from_port, to_port, related_out,
				  true, true, attrs);
  case timing_type_three_state_enable_fall:
    return makeTristateEnableArcs(cell, from_port, to_port, related_out,
				  false, true, attrs);
  case timing_type_three_state_enable_rise:
    return makeTristateEnableArcs(cell, from_port, to_port, related_out,
				  true, false, attrs);
  case timing_type_skew_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::fall(), TimingRole::skew(),
				  attrs);
  case timing_type_skew_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::rise(), TimingRole::skew(),
				  attrs);
  case timing_type_non_seq_setup_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::rise(),
				  TimingRole::nonSeqSetup(), attrs);
  case timing_type_non_seq_setup_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::fall(),
				  TimingRole::nonSeqSetup(), attrs);
  case timing_type_non_seq_hold_rising:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::rise(),
				  TimingRole::nonSeqHold(),
				  attrs);
  case timing_type_non_seq_hold_falling:
    return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				  TransRiseFall::fall(),
				  TimingRole::nonSeqHold(),
				  attrs);
  case timing_type_min_pulse_width:
  case timing_type_minimum_period:
  case timing_type_nochange_high_high:
  case timing_type_nochange_high_low:
  case timing_type_nochange_low_high:
  case timing_type_nochange_low_low:
  case timing_type_retaining_time:
  case timing_type_unknown:
  case timing_type_min_clock_tree_path:
  case timing_type_max_clock_tree_path:
    return NULL;
  }
  // Prevent warnings from lame compilers.
  return NULL;
}

TimingArcSet *
LibertyBuilder::makeCombinationalArcs(LibertyCell *cell,
				      LibertyPort *from_port,
				      LibertyPort *to_port,
				      LibertyPort *related_out,
				      bool to_rise,
				      bool to_fall,
				      TimingArcAttrs *attrs)
{
  FuncExpr *func = to_port->function();
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port, related_out,
					   TimingRole::combinational(), attrs);
  TimingSense sense = attrs->timingSense();
  if (sense == timing_sense_unknown && func) {
    // Timing sense not specified - find it from function.
    sense = func->portTimingSense(from_port);
    if (sense == timing_sense_none
	&& to_port->direction()->isAnyTristate()) {
      // from_port is not an input to function, check tristate enable.
      FuncExpr *enable = to_port->tristateEnable();
      if (enable && enable->hasPort(from_port))
	sense = timing_sense_non_unate;
    }
  }
  TimingModel *model;
  TransRiseFall *to_tr;
  switch (sense) {
  case timing_sense_positive_unate:
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, TransRiseFall::rise(), to_tr, model);
    }
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, TransRiseFall::fall(), to_tr, model);
    }
    break;
  case timing_sense_negative_unate:
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, TransRiseFall::rise(), to_tr, model);
    }
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, TransRiseFall::fall(), to_tr, model);
    }
    break;
  case timing_sense_non_unate:
  case timing_sense_unknown:
    // Timing sense none means function does not mention from_port.
    // This can happen if the function references an internal port,
    // as in fpga lut cells.
  case timing_sense_none:
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model) {
	makeTimingArc(arc_set, TransRiseFall::fall(), to_tr, model);
	makeTimingArc(arc_set, TransRiseFall::rise(), to_tr, model);
      }
    }
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model) {
	makeTimingArc(arc_set, TransRiseFall::rise(), to_tr, model);
	makeTimingArc(arc_set, TransRiseFall::fall(), to_tr, model);
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
				  LibertyPort *related_out,
				  TimingArcAttrs *attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port,
					   related_out,
					   TimingRole::latchDtoQ(), attrs);
  TimingModel *model;
  TransRiseFall *to_tr = TransRiseFall::rise();
  model = attrs->model(to_tr);
  TimingSense sense = attrs->timingSense();
  if (model) {
    TransRiseFall *from_tr = (sense == timing_sense_negative_unate) ?
      to_tr->opposite() : to_tr;
    makeTimingArc(arc_set, from_tr, to_tr, model);
  }
  to_tr = TransRiseFall::fall();
  model = attrs->model(to_tr);
  if (model) {
    TransRiseFall *from_tr = (sense == timing_sense_negative_unate) ?
      to_tr->opposite() : to_tr;
    makeTimingArc(arc_set, from_tr, to_tr, model);
  }
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makeRegLatchArcs(LibertyCell *cell,
				 LibertyPort *from_port,
				 LibertyPort *to_port,
				 LibertyPort *related_out,
				 TransRiseFall *from_tr,
				 TimingArcAttrs *attrs)
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
				      from_tr, role, attrs);
      }
      else if (seq->isLatch()
	       && seq->data()
	       && seq->data()->hasPort(from_port))
	return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				      from_tr, TimingRole::latchDtoQ(), attrs);
      else if ((seq->clear() && seq->clear()->hasPort(from_port))
	       || (seq->preset() && seq->preset()->hasPort(from_port)))
	return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				      from_tr, TimingRole::regSetClr(), attrs);
    }
  }
  // No associated ff/latch - assume register clk->q.
  cell->setHasInferedRegTimingArcs(true);
  return makeFromTransitionArcs(cell, from_port, to_port, related_out,
				from_tr, TimingRole::regClkToQ(), attrs);
}

TimingArcSet *
LibertyBuilder::makeFromTransitionArcs(LibertyCell *cell,
				       LibertyPort *from_port,
				       LibertyPort *to_port,
				       LibertyPort *related_out,
				       TransRiseFall *from_tr,
				       TimingRole *role,
				       TimingArcAttrs *attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port,
					   related_out, role, attrs);
  TimingModel *model;
  TransRiseFall *to_tr;
  to_tr = TransRiseFall::rise();
  model = attrs->model(to_tr);
  if (model)
    makeTimingArc(arc_set, from_tr, to_tr, model);
  to_tr = TransRiseFall::fall();
  model = attrs->model(to_tr);
  if (model)
    makeTimingArc(arc_set, from_tr, to_tr, model);
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makePresetClrArcs(LibertyCell *cell,
				  LibertyPort *from_port,
				  LibertyPort *to_port,
				  LibertyPort *related_out,
				  TransRiseFall *to_tr,
				  TimingArcAttrs *attrs)
{
  TimingArcSet *arc_set = NULL;
  TimingModel *model = attrs->model(to_tr);
  if (model) {
    arc_set = makeTimingArcSet(cell, from_port, to_port, related_out,
			       TimingRole::regSetClr(), attrs);
    TransRiseFall *opp_tr = to_tr->opposite();
    switch (attrs->timingSense()) {
    case timing_sense_positive_unate:
      makeTimingArc(arc_set, to_tr, to_tr, model);
      break;
    case timing_sense_negative_unate:
      makeTimingArc(arc_set, opp_tr, to_tr, model);
      break;
    case timing_sense_non_unate:
    case timing_sense_unknown:
      makeTimingArc(arc_set, to_tr, to_tr, model);
      makeTimingArc(arc_set, opp_tr, to_tr, model);
      break;
    case timing_sense_none:
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
				       TimingArcAttrs *attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port, related_out,
					   TimingRole::tristateEnable(),attrs);
  FuncExpr *tristate_enable = to_port->tristateEnable();
  TimingSense sense = attrs->timingSense();
  if (sense == timing_sense_unknown && tristate_enable)
    sense = tristate_enable->portTimingSense(from_port);
  TimingModel *model;
  TransRiseFall *to_tr;
  switch (sense) {
  case timing_sense_positive_unate:
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, Transition::rise(), Transition::trZ1(), model);
    }
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, Transition::rise(), Transition::trZ0(), model);
    }
    break;
  case timing_sense_negative_unate:
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, Transition::fall(), Transition::trZ1(), model);
    }
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, Transition::fall(), Transition::trZ0(), model);
    }
    break;
  case timing_sense_non_unate:
  case timing_sense_unknown:
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model) {
	makeTimingArc(arc_set, Transition::rise(), Transition::trZ1(), model);
	makeTimingArc(arc_set, Transition::fall(), Transition::trZ1(), model);
      }
    }
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model) {
	makeTimingArc(arc_set, Transition::rise(), Transition::trZ0(), model);
	makeTimingArc(arc_set, Transition::fall(), Transition::trZ0(), model);
      }
    }
    break;
  case timing_sense_none:
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
					TimingArcAttrs *attrs)
{
  TimingArcSet *arc_set = makeTimingArcSet(cell, from_port, to_port,
					   related_out,
					   TimingRole::tristateDisable(),
					   attrs);
  TimingSense sense = attrs->timingSense();
  FuncExpr *tristate_enable = to_port->tristateEnable();
  if (sense == timing_sense_unknown && tristate_enable)
    sense = timingSenseOpposite(tristate_enable->portTimingSense(from_port));
  TimingModel *model;
  TransRiseFall *to_tr;
  switch (sense) {
  case timing_sense_positive_unate:
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, Transition::rise(), Transition::tr0Z(), model);
    }
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, Transition::rise(), Transition::tr1Z(), model);
    }
    break;
  case timing_sense_negative_unate:
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, Transition::fall(), Transition::tr0Z(), model);
    }
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model)
	makeTimingArc(arc_set, Transition::fall(), Transition::tr1Z(), model);
    }
    break;
  case timing_sense_non_unate:
  case timing_sense_unknown:
    if (to_rise) {
      to_tr = TransRiseFall::rise();
      model = attrs->model(to_tr);
      if (model) {
	makeTimingArc(arc_set, Transition::fall(), Transition::tr0Z(), model);
	makeTimingArc(arc_set, Transition::rise(), Transition::tr0Z(), model);
      }
    }
    if (to_fall) {
      to_tr = TransRiseFall::fall();
      model = attrs->model(to_tr);
      if (model) {
	makeTimingArc(arc_set, Transition::fall(), Transition::tr1Z(), model);
	makeTimingArc(arc_set, Transition::rise(), Transition::tr1Z(), model);
      }
    }
    break;
  case timing_sense_none:
    break;
  }
  return arc_set;
}

TimingArcSet *
LibertyBuilder::makeTimingArcSet(LibertyCell *cell,
				 LibertyPort *from,
				 LibertyPort *to,
				 LibertyPort *related_out,
				 TimingRole *role,
				 TimingArcAttrs *attrs)
{
  return new TimingArcSet(cell, from, to, related_out, role, attrs);
}

TimingArc *
LibertyBuilder::makeTimingArc(TimingArcSet *set,
			      TransRiseFall *from_tr,
			      TransRiseFall *to_tr,
			      TimingModel *model)
{
  return new TimingArc(set, from_tr->asTransition(),
		       to_tr->asTransition(), model);
}

TimingArc *
LibertyBuilder::makeTimingArc(TimingArcSet *set,
			      Transition *from_tr,
			      Transition *to_tr,
			      TimingModel *model)
{
  return new TimingArc(set, from_tr, to_tr, model);
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
