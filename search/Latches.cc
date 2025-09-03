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

#include "Latches.hh"

#include "Debug.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "ExceptionPath.hh"
#include "Sdc.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "Sim.hh"
#include "PathEnd.hh"
#include "PathAnalysisPt.hh"
#include "Search.hh"
#include "Crpr.hh"

namespace sta {

Latches::Latches(StaState *sta) :
  StaState(sta)
{
}

void
Latches::latchRequired(const Path *data_path,
		       const Path *enable_path,
		       const Path *disable_path,
		       const MultiCyclePath *mcp,
		       const PathDelay *path_delay,
		       Arrival src_clk_latency,
		       const ArcDelay &margin,
		       // Return values.
		       Required &required,
		       Arrival &borrow,
		       Arrival &adjusted_data_arrival,
		       Delay &time_given_to_startpoint) const
{
  const Arrival data_arrival = data_path->arrival();
  float max_delay = 0.0;
  bool ignore_clk_latency = false;
  if (path_delay) {
    max_delay = path_delay->delay();
    ignore_clk_latency = path_delay->ignoreClkLatency();
  }
  if (ignore_clk_latency) {
      required = max_delay + src_clk_latency;
      borrow = 0.0;
      adjusted_data_arrival = data_arrival;
      time_given_to_startpoint = 0.0;
  }
  else if (enable_path && disable_path) {
    Delay open_latency, latency_diff, max_borrow;
    float nom_pulse_width, open_uncertainty;
    Crpr open_crpr, crpr_diff;
    bool borrow_limit_exists;
    latchBorrowInfo(data_path, enable_path, disable_path, margin,
		    ignore_clk_latency,
		    nom_pulse_width, open_latency, latency_diff,
		    open_uncertainty, open_crpr, crpr_diff, max_borrow,
		    borrow_limit_exists);
    const ClockEdge *data_clk_edge = data_path->clkEdge(this);
    const ClockEdge *enable_clk_edge = enable_path->clkEdge(this);
    const TimingRole *check_role =
      enable_path->clkInfo(this)->isPulseClk()
      ? TimingRole::setup()
      : TimingRole::latchSetup();
    CycleAccting *acct = sdc_->cycleAccting(data_clk_edge,
                                            enable_clk_edge);
    // checkTgtClkTime
    float tgt_clk_time = path_delay ? 0.0 : acct->requiredTime(check_role);
    // checkTgtClkArrival broken down into components.
    Arrival enable_arrival = max_delay
      + tgt_clk_time
      + open_latency
      + open_uncertainty
      + PathEnd::checkSetupMcpAdjustment(data_clk_edge, enable_clk_edge, mcp,
					 1, sdc_)
      + open_crpr;
    debugPrint(debug_, "latch", 1, "latch data %s %s enable %s",
               network_->pathName(data_path->pin(this)),
               delayAsString(data_arrival, this),
               delayAsString(enable_arrival, this));
    if (delayLessEqual(data_arrival, enable_arrival, this)) {
      // Data arrives before latch opens.
      required = enable_arrival;
      borrow = 0.0;
      adjusted_data_arrival = data_arrival;
      time_given_to_startpoint = 0.0;
    }
    else {
      // Data arrives while latch is transparent.
      borrow = data_arrival - enable_arrival;
      if (delayLessEqual(borrow, max_borrow, this))
	required = data_arrival;
      else {
	borrow = max_borrow;
	required = enable_arrival + max_borrow;
      }
      time_given_to_startpoint = borrow + open_uncertainty + open_crpr;

      // Cycle accounting for required time is with respect to the
      // data clock zeroth cycle.  The data departs the latch
      // with respect to the enable clock zeroth cycle.
      float data_shift_to_enable_clk = acct->sourceTimeOffset(check_role)
	- acct->targetTimeOffset(check_role);
      adjusted_data_arrival = required + data_shift_to_enable_clk;
    }
  }
  else if (disable_path) {
    required = max_delay + search_->clkPathArrival(disable_path) - margin;
    // Borrow cannot be determined without enable path.
    borrow = 0.0;
    adjusted_data_arrival = data_arrival;
    time_given_to_startpoint = 0.0;
  }
  else {
    required = max_delay;
    borrow = 0.0;
    adjusted_data_arrival = data_arrival;
    time_given_to_startpoint = 0.0;
  }
}

void
Latches::latchBorrowInfo(const Path *data_path,
			 const Path *enable_path,
			 const Path *disable_path,
			 const ArcDelay &margin,
			 bool ignore_clk_latency,
			 // Return values.
			 float &nom_pulse_width,
			 Delay &open_latency,
			 Delay &latency_diff,
			 float &open_uncertainty,
			 Crpr &open_crpr,
			 Crpr &crpr_diff,
			 Delay &max_borrow,
			 bool &borrow_limit_exists) const
{
  if (data_path && enable_path && disable_path) {
    const ClockEdge *data_clk_edge = data_path->clkEdge(this);
    const ClockEdge *enable_clk_edge = enable_path->clkEdge(this);
    const ClockEdge *disable_clk_edge = disable_path->clkEdge(this);
    bool is_pulse_clk = enable_path->clkInfo(this)->isPulseClk();
    nom_pulse_width = is_pulse_clk ? 0.0F : enable_clk_edge->pulseWidth();
    open_uncertainty = PathEnd::checkClkUncertainty(data_clk_edge, enable_clk_edge,
                                                    enable_path,
                                                    TimingRole::latchSetup(), this);
    if (ignore_clk_latency) {
      open_latency = 0.0;
      latency_diff = 0.0;
      open_crpr = 0.0;
      crpr_diff = 0.0;
    }
    else {
      CheckCrpr *check_crpr = search_->checkCrpr();
      open_crpr = check_crpr->checkCrpr(data_path, enable_path);
      Crpr close_crpr = check_crpr->checkCrpr(data_path, disable_path);
      crpr_diff = open_crpr - close_crpr;
      open_latency = PathEnd::checkTgtClkDelay(enable_path, enable_clk_edge,
                                               TimingRole::setup(), this);
      Arrival close_latency = PathEnd::checkTgtClkDelay(disable_path,
                                                        disable_clk_edge,
                                                        TimingRole::latchSetup(),
                                                        this);
      latency_diff = open_latency - close_latency;
    }
    float borrow_limit;
    sdc_->latchBorrowLimit(data_path->pin(this), disable_path->pin(this),
                           enable_clk_edge->clock(),
                           borrow_limit, borrow_limit_exists);
    if (borrow_limit_exists)
      max_borrow = borrow_limit;
    else
      max_borrow = nom_pulse_width - delayAsFloat(latency_diff)
        - delayAsFloat(crpr_diff) - delayAsFloat(margin);
  }
  else {
    nom_pulse_width = 0.0;
    open_uncertainty = 0.0;
    open_latency = 0.0;
    latency_diff = 0.0;
    open_crpr = 0.0;
    crpr_diff = 0.0;
  }
}

void
Latches::latchRequired(const Path *data_path,
		       const Path *enable_path,
		       const Path *disable_path,
		       const PathAnalysisPt *path_ap,
		       // Return values.
		       Required &required,
		       Arrival &borrow,
		       Arrival &adjusted_data_arrival,
		       Delay &time_given_to_startpoint) const
{
  Vertex *data_vertex = data_path->vertex(this);
  const RiseFall *data_rf = data_path->transition(this);
  ArcDelay setup = latchSetupMargin(data_vertex,data_rf,disable_path,path_ap);
  ExceptionPath *excpt = search_->exceptionTo(ExceptionPathType::any,
					      data_path, data_vertex->pin(),
					      data_rf,
					      enable_path->clkEdge(this),
					      path_ap->pathMinMax(), false,
					      false);
  MultiCyclePath *mcp = dynamic_cast<MultiCyclePath*>(excpt);
  PathDelay *path_delay = dynamic_cast<PathDelay*>(excpt);
  Arrival src_clk_latency = 0.0;
  if (path_delay && path_delay->ignoreClkLatency())
    src_clk_latency = search_->pathClkPathArrival(data_path);
  latchRequired(data_path, enable_path, disable_path, mcp,
		path_delay, src_clk_latency, setup,
		required, borrow, adjusted_data_arrival,
		time_given_to_startpoint);
}

// Find the latch enable open/close path from the close/open path.
Path *
Latches::latchEnableOtherPath(const Path *path,
                              const PathAnalysisPt *tgt_clk_path_ap) const
{
  Vertex *vertex = path->vertex(this);
  const ClockEdge *clk_edge = path->clkEdge(this);
  const ClockEdge *other_clk_edge =
    path->clkInfo(this)->isPulseClk() ? clk_edge:clk_edge->opposite();
  const RiseFall *other_rf = path->transition(this)->opposite();
  VertexPathIterator path_iter(vertex, other_rf, tgt_clk_path_ap, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    if (path->isClock(this)
	&& path->clkEdge(this) == other_clk_edge) {
      return path;
    }
  }
  return nullptr;
}

Path *
Latches::latchEnablePath(const Path *q_path,
			 const Edge *d_q_edge) const

{
  const ClockEdge *en_clk_edge = q_path->clkEdge(this);
  PathAnalysisPt *path_ap = q_path->pathAnalysisPt(this);
  const PathAnalysisPt *tgt_clk_path_ap = path_ap->tgtClkAnalysisPt();
  const Instance *latch = network_->instance(q_path->pin(this));
  Vertex *en_vertex;
  const RiseFall *en_rf;
  LatchEnableState state;
  latchDtoQEnable(d_q_edge, latch, en_vertex, en_rf, state);
  if (state == LatchEnableState::enabled) {
    VertexPathIterator path_iter(en_vertex, en_rf, tgt_clk_path_ap, this);
    while (path_iter.hasNext()) {
      Path *path = path_iter.next();
      const ClockEdge *clk_edge = path->clkEdge(this);
      if (path->isClock(this)
	  && clk_edge == en_clk_edge) {
	return path;
      }
    }
  }
  return nullptr;
}

// The arrival time for a latch D->Q edge is clipped to the window of
// time when the latch is transparent using the open/close arrival
// times of the enable.  The to_tag for Q is adjusted to the that of
// the enable open edge.
void
Latches::latchOutArrival(const Path *data_path,
			 const TimingArc *d_q_arc,
			 const Edge *d_q_edge,
			 const PathAnalysisPt *path_ap,
			 // Return values.
			 Tag *&q_tag,
			 ArcDelay &arc_delay,
			 Arrival &q_arrival)
{
  Vertex *data_vertex = d_q_edge->from(graph_);
  const Instance *inst = network_->instance(data_vertex->pin());
  Vertex *enable_vertex;
  const RiseFall *enable_rf;
  LatchEnableState state;
  latchDtoQEnable(d_q_edge, inst, enable_vertex, enable_rf, state);
  // Latch enable may be missing if library is malformed.
  switch (state) {
  case LatchEnableState::closed:
    // Latch is disabled by constant enable.
    break;
  case LatchEnableState::open: {
    ExceptionPath *excpt = exceptionTo(data_path, nullptr);
    if (!(excpt && excpt->isFalse())) {
      arc_delay = search_->deratedDelay(data_vertex, d_q_arc, d_q_edge,
					false, path_ap);
      q_arrival = data_path->arrival() + arc_delay;
      q_tag = data_path->tag(this);
    }
  }
    break;
  case LatchEnableState::enabled: {
    const PathAnalysisPt *tgt_clk_path_ap = path_ap->tgtClkAnalysisPt();
    VertexPathIterator enable_iter(enable_vertex, enable_rf,
				   tgt_clk_path_ap, this);
    while (enable_iter.hasNext()) {
      Path *enable_path = enable_iter.next();
       ClkInfo *en_clk_info = enable_path->clkInfo(this);
       const ClockEdge *en_clk_edge = en_clk_info->clkEdge();
       if (enable_path->isClock(this)) {
	 ExceptionPath *excpt = exceptionTo(data_path, en_clk_edge);
	 // D->Q is disabled when if there is a path delay -to D or EN clk.
	 if (!(excpt && (excpt->isFalse()
			 || excpt->isPathDelay()))) {
	   Path *disable_path = latchEnableOtherPath(enable_path, tgt_clk_path_ap);
	   Delay borrow, time_given_to_startpoint;
	   Arrival adjusted_data_arrival;
	   Required required;
	   latchRequired(data_path, enable_path, disable_path, path_ap,
			 required, borrow, adjusted_data_arrival,
			 time_given_to_startpoint);
	   if (delayGreater(borrow, 0.0, this)) {
	     // Latch is transparent when data arrives.
	     arc_delay = search_->deratedDelay(data_vertex, d_q_arc, d_q_edge,
					       false, path_ap);
	     q_arrival = adjusted_data_arrival + arc_delay;
	     // Tag switcheroo - data passing thru gets latch enable tag.
	     // States and path ap come from Q, everything else from enable.
	     Path *crpr_clk_path = crprActive() ? enable_path : nullptr;
	     ClkInfo *q_clk_info = 
	       search_->findClkInfo(en_clk_edge,
				    en_clk_info->clkSrc(),
				    en_clk_info->isPropagated(),
				    en_clk_info->genClkSrc(),
				    en_clk_info->isGenClkSrcPath(),
				    en_clk_info->pulseClkSense(),
				    en_clk_info->insertion(),
				    en_clk_info->latency(),
				    en_clk_info->uncertainties(),
				    path_ap,
				    crpr_clk_path);
	     const RiseFall *q_rf = d_q_arc->toEdge()->asRiseFall();
	     ExceptionStateSet *states = nullptr;
	     // Latch data pin is a valid exception -from pin.
	     if (sdc_->exceptionFromStates(data_path->pin(this),
						   data_path->transition(this),
						   nullptr, nullptr, // clk below
						   MinMax::max(), states)
		 // -from enable non-filter exceptions apply.
		 && sdc_->exceptionFromStates(enable_vertex->pin(),
						      enable_rf,
						      en_clk_edge->clock(),
						      en_clk_edge->transition(),
						      MinMax::max(), false, states))
	       q_tag = search_->findTag(q_rf, path_ap, q_clk_info, false,
					nullptr, false, states, true);
	   }
	   return;
	 }
       }
    }
    // No enable path found.
  }
    break;
  }
}

ExceptionPath *
Latches::exceptionTo(const Path *data_path,
 		     const ClockEdge *en_clk_edge)
{
  // Look for exceptions -to data or -to enable clk.
  return search_->exceptionTo(ExceptionPathType::any,
			      data_path,
			      data_path->pin(this),
			      data_path->transition(this),
			      en_clk_edge,
			      data_path->minMax(this),
			      false, false);
}

ArcDelay
Latches::latchSetupMargin(Vertex *data_vertex,
			  const RiseFall *data_rf,
			  const Path *disable_path,
			  const PathAnalysisPt *path_ap) const
{
  if (disable_path) {
    Vertex *enable_vertex = disable_path->vertex(this);
    const RiseFall *disable_rf = disable_path->transition(this);
    VertexInEdgeIterator edge_iter(data_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      const TimingRole *role = edge->role();
      Vertex *from_vertex = edge->from(graph_);
      if (role == TimingRole::setup()
	  && from_vertex == enable_vertex
	  && !edge->isDisabledCond()
	  && !sdc_->isDisabledCondDefault(edge)) {
	TimingArcSet *arc_set = edge->timingArcSet();
        for (TimingArc *check_arc : arc_set->arcs()) {
	  if (check_arc->toEdge()->asRiseFall() == data_rf
	      && check_arc->fromEdge()->asRiseFall() == disable_rf)
	    return search_->deratedDelay(from_vertex, check_arc, edge,
					 false, path_ap);
	}
      }
    }
  }
  return 0.0;
}

void
Latches::latchTimeGivenToStartpoint(const Path *d_path,
				    const Path *q_path,
				    const Edge *d_q_edge,
				    // Return values.
				    Arrival &time_given,
				    Path *&enable_path) const
{
  enable_path = latchEnablePath(q_path, d_q_edge);
  if (enable_path
      && enable_path->isClock(this)) {
    const PathAnalysisPt *path_ap = q_path->pathAnalysisPt(this);
    const PathAnalysisPt *tgt_clk_path_ap = path_ap->tgtClkAnalysisPt();
    Path *disable_path = latchEnableOtherPath(enable_path, tgt_clk_path_ap);
    Delay borrow;
    Required required;
    Arrival adjusted_data_arrival;
    latchRequired(d_path, enable_path, disable_path, path_ap,
		  required, borrow, adjusted_data_arrival, time_given);
  }
  else {
    time_given = 0.0;
    enable_path = nullptr;
  }
}

void
Latches::latchDtoQEnable(const Edge *d_q_edge,
			 const Instance *inst,
			 // Return values.
			 Vertex *&enable_vertex,
			 const RiseFall *&enable_rf,
			 LatchEnableState &state) const
{
  enable_vertex = nullptr;
  state = LatchEnableState::open;
  LibertyCell *cell = network_->libertyCell(inst);
  if (cell) {
    const TimingArcSet *d_q_set = d_q_edge->timingArcSet();
    const LibertyPort *enable_port;
    const FuncExpr *enable_func;
    cell->latchEnable(d_q_set, enable_port, enable_func, enable_rf);
    if (enable_port) {
      Pin *enable_pin = network_->findPin(inst, enable_port);
      if (enable_pin) {
	enable_vertex = graph_->pinLoadVertex(enable_pin);
	if (enable_vertex->isDisabledConstraint())
	  state = LatchEnableState::open;
	else {
	  // See if constant values in the latch enable expression force
	  // it to be continuously open or closed.
	  LogicValue enable_value = enable_func
	    ? sim_->evalExpr(enable_func, inst)
	    : sim_->logicValue(enable_pin);
	  switch (enable_value) {
	  case LogicValue::zero:
	  case LogicValue::fall:
	    state = LatchEnableState::closed;
	    break;
	  case LogicValue::one:
	  case LogicValue::rise:
	    state = LatchEnableState::open;
	    break;
	  case LogicValue::unknown:
	    state = LatchEnableState::enabled;
	    break;
	  }
	}
      }
    }
  }
}

LatchEnableState
Latches::latchDtoQState(const Edge *edge) const
{
  const Vertex *from_vertex = edge->from(graph_);
  const Pin *from_pin = from_vertex->pin();
  const Instance *inst = network_->instance(from_pin);
  Vertex *enable_vertex;
  const RiseFall *enable_rf;
  LatchEnableState state;
  latchDtoQEnable(edge, inst, enable_vertex, enable_rf, state);
  return state;
}

// Latch D->Q arc looks combinational when the enable pin is disabled
// or constant.
bool
Latches::isLatchDtoQ(const Edge *edge) const
{
  return edge->role() == TimingRole::latchDtoQ()
    && latchDtoQState(edge) == LatchEnableState::enabled;
}

} // namespace
