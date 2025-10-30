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

#include "PathEnd.hh"

#include "Debug.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "PortDelay.hh"
#include "DataCheck.hh"
#include "Sdc.hh"
#include "ExceptionPath.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "PathAnalysisPt.hh"
#include "Search.hh"
#include "ReportPath.hh"
#include "Sim.hh"
#include "Latches.hh"
#include "StaState.hh"
#include "PathExpanded.hh"
#include "search/Crpr.hh"

namespace sta {

PathEnd::PathEnd(Path *path) :
  path_(path),
  path_group_(nullptr)
{
}

PathEnd::~PathEnd()
{
  if (path_->isEnum())
    delete path_;
}

void
PathEnd::setPath(Path *path)
{
  path_ = path;
}

void
PathEnd::setPathGroup(PathGroup *path_group)
{
  path_group_ = path_group;
}

Vertex *
PathEnd::vertex(const StaState *sta) const
{
  return path_->vertex(sta);
}

const MinMax *
PathEnd::minMax(const StaState *sta) const
{
  return path_->pathAnalysisPt(sta)->pathMinMax();
}

const EarlyLate *
PathEnd::pathEarlyLate(const StaState *sta) const
{
  return path_->pathAnalysisPt(sta)->pathMinMax();
}

const EarlyLate *
PathEnd::clkEarlyLate(const StaState *sta) const
{
  return checkRole(sta)->tgtClkEarlyLate();
}

const RiseFall *
PathEnd::transition(const StaState *sta) const
{
  return path_->transition(sta);
}

PathAPIndex
PathEnd::pathIndex(const StaState *sta) const
{
  return path_->pathAnalysisPtIndex(sta);
}

PathAnalysisPt *
PathEnd::pathAnalysisPt(const StaState *sta) const
{
  return path_->pathAnalysisPt(sta);
}

const ClockEdge *
PathEnd::sourceClkEdge(const StaState *sta) const
{
  return path_->clkEdge(sta);
}

Arrival
PathEnd::dataArrivalTime(const StaState *) const
{
  return path_->arrival();
}

Arrival
PathEnd::dataArrivalTimeOffset(const StaState *sta) const
{
  return dataArrivalTime(sta) + sourceClkOffset(sta);
}

Required
PathEnd::requiredTimeOffset(const StaState *sta) const
{
  return requiredTime(sta) + sourceClkOffset(sta);
}

const RiseFall *
PathEnd::targetClkEndTrans(const StaState *sta) const
{
  const Path *clk_path = targetClkPath();
  if (clk_path)
    return clk_path->transition(sta);
  else {
    const ClockEdge *clk_edge = targetClkEdge(sta);
    if (clk_edge)
      return clk_edge->transition();
    else
      return nullptr;
  }
}

const TimingRole *
PathEnd::checkGenericRole(const StaState *sta) const
{
  return checkRole(sta)->genericRole();
}

Delay
PathEnd::sourceClkLatency(const StaState *) const
{
  return delay_zero;
}

Delay
PathEnd::sourceClkInsertionDelay(const StaState *) const
{
  return delay_zero;
}

const Clock *
PathEnd::targetClk(const StaState *) const
{
  return nullptr;
}

const ClockEdge *
PathEnd::targetClkEdge(const StaState *) const
{
  return nullptr;
}

float
PathEnd::targetClkTime(const StaState *) const
{
  return 0.0;
}

float
PathEnd::targetClkOffset(const StaState *) const
{
  return 0.0;
}

Arrival
PathEnd::targetClkArrival(const StaState *) const
{
  return delay_zero;
}

Delay
PathEnd::targetClkDelay(const StaState *) const
{
  return delay_zero;
}

Delay
PathEnd::targetClkInsertionDelay(const StaState *) const
{
  return delay_zero;
}

float
PathEnd::targetNonInterClkUncertainty(const StaState *) const
{
  return 0.0;
}

float
PathEnd::interClkUncertainty(const StaState *) const
{
  return 0.0;
}

float
PathEnd::targetClkUncertainty(const StaState *) const
{
  return 0.0;
}

float
PathEnd::targetClkMcpAdjustment(const StaState *) const
{
  return 0.0;
}

const TimingRole *
PathEnd::checkRole(const StaState *) const
{
  return nullptr;
}

Path *
PathEnd::targetClkPath()
{
  return nullptr;
}

const Path *
PathEnd::targetClkPath() const
{
  return nullptr;
}

bool
PathEnd::pathDelayMarginIsExternal() const
{
  return false;
}

PathDelay *
PathEnd::pathDelay() const
{
  return nullptr;
}

Arrival
PathEnd::borrow(const StaState *) const
{
  return 0.0;
}

Crpr
PathEnd::checkCrpr(const StaState *sta) const
{
  if (checkRole(sta)->genericRole() == TimingRole::hold())
    return -crpr(sta);
  else
    return crpr(sta);;
}

Crpr
PathEnd::crpr(const StaState *) const
{
  return 0.0;
}

MultiCyclePath *
PathEnd::multiCyclePath() const
{
  return nullptr;
}

int
PathEnd::exceptPathCmp(const PathEnd *path_end,
		       const StaState *) const
{
  Type type1 = type();
  Type type2 = path_end->type();
  if (type1 == type2)
    return 0;
  else if (type1 < type2)
    return -1;
  else
    return 1;
}

////////////////////////////////////////////////////////////////

Delay
PathEnd::checkTgtClkDelay(const Path *tgt_clk_path,
			  const ClockEdge *tgt_clk_edge,
			  const TimingRole *check_role,
			  const StaState *sta)
{
  Delay insertion, latency;
  checkTgtClkDelay(tgt_clk_path, tgt_clk_edge, check_role, sta,
		   insertion, latency);
  return Delay(insertion + latency);
}

void
PathEnd::checkTgtClkDelay(const Path *tgt_clk_path,
			  const ClockEdge *tgt_clk_edge,
			  const TimingRole *check_role,
			  const StaState *sta,
			  // Return values.
			  Delay &insertion,
			  Delay &latency)
{
  if (tgt_clk_path) {
    Search *search = sta->search();
    // If propagated clk, adjust required time for target clk network delay.
    const MinMax *min_max = tgt_clk_path->minMax(sta);
    const EarlyLate *early_late = check_role->tgtClkEarlyLate();
    const PathAnalysisPt *tgt_path_ap = tgt_clk_path->pathAnalysisPt(sta);
    const ClkInfo *clk_info = tgt_clk_path->clkInfo(sta);
    const Pin *tgt_src_pin = clk_info->clkSrc();
    const Clock *tgt_clk = tgt_clk_edge->clock();
    const RiseFall *tgt_clk_rf = tgt_clk_edge->transition();
    insertion = search->clockInsertion(tgt_clk, tgt_src_pin, tgt_clk_rf,
				       min_max, early_late, tgt_path_ap);
    if (clk_info->isPropagated()
	// Data check target clock is always propagated.
	|| check_role->isDataCheck()) {
      // Propagated clock.  Propagated arrival is seeded with
      // early_late==path_min_max insertion delay.
      Arrival clk_arrival = tgt_clk_path->arrival();
      Delay path_insertion = search->clockInsertion(tgt_clk, tgt_src_pin,
						      tgt_clk_rf, min_max,
						      min_max, tgt_path_ap);
      latency = delayRemove(clk_arrival - tgt_clk_edge->time(), path_insertion);
    }
    else
      // Ideal clock.
      latency = clk_info->latency();
  }
  else {
    insertion = 0.0F;
    latency = 0.0F;
  }
}

float
PathEnd::checkClkUncertainty(const ClockEdge *src_clk_edge,
			     const ClockEdge *tgt_clk_edge,
			     const Path *tgt_clk_path,
			     const TimingRole *check_role,
			     const StaState *sta)
{

  float inter_clk;
  bool inter_exists;
  checkInterClkUncertainty(src_clk_edge, tgt_clk_edge, check_role, sta,
			   inter_clk, inter_exists);
  if (inter_exists)
    return inter_clk;
  else
    return checkTgtClkUncertainty(tgt_clk_path, tgt_clk_edge, check_role, sta);
}

float
PathEnd::checkTgtClkUncertainty(const Path *tgt_clk_path,
                                const ClockEdge *tgt_clk_edge,
                                const TimingRole *check_role,
                                const StaState *sta)
{
  const MinMax *min_max = check_role->pathMinMax();
  ClockUncertainties *uncertainties = nullptr;
  if (tgt_clk_path && tgt_clk_path->isClock(sta))
    uncertainties = tgt_clk_path->clkInfo(sta)->uncertainties();
  else if (tgt_clk_edge)
    uncertainties = tgt_clk_edge->clock()->uncertainties();
  float uncertainty = 0.0;
  if (uncertainties) {
    bool exists;
    float uncertainty1;
    uncertainties->value(min_max, uncertainty1, exists);
    if (exists)
      uncertainty = uncertainty1;
  }
  if (check_role->genericRole() == TimingRole::setup())
    uncertainty = -uncertainty;
  return uncertainty;
}

void
PathEnd::checkInterClkUncertainty(const ClockEdge *src_clk_edge,
				  const ClockEdge *tgt_clk_edge,
				  const TimingRole *check_role,
				  const StaState *sta,
				  float &uncertainty,
				  bool &exists)
{
  Sdc *sdc = sta->sdc();
  if (src_clk_edge
      && src_clk_edge != sdc->defaultArrivalClockEdge()
      && tgt_clk_edge) {
    sdc->clockUncertainty(src_clk_edge->clock(),
                          src_clk_edge->transition(),
                          tgt_clk_edge->clock(),
                          tgt_clk_edge->transition(),
                          check_role->pathMinMax(),
                          uncertainty, exists);
    if (exists
	&& check_role->genericRole() == TimingRole::setup())
      uncertainty = -uncertainty;
  }
  else
    exists = false;
}

////////////////////////////////////////////////////////////////

void
PathEndUnconstrained::reportFull(const ReportPath *report) const
{
  report->reportFull(this);
}

Slack
PathEndUnconstrained::slackNoCrpr(const StaState *) const
{
  return INF;
}

void
PathEndUnconstrained::reportShort(const ReportPath *report) const
{
  report->reportShort(this);
}

////////////////////////////////////////////////////////////////

PathEndUnconstrained::PathEndUnconstrained(Path *path) :
  PathEnd(path)
{
}

PathEnd *
PathEndUnconstrained::copy() const
{
  return new PathEndUnconstrained(path_);
}

bool
PathEndUnconstrained::isUnconstrained() const
{
  return true;
}

Required
PathEndUnconstrained::requiredTime(const StaState *sta) const
{
  return delayInitValue(minMax(sta)->opposite());
}

Required
PathEndUnconstrained::requiredTimeOffset(const StaState *sta) const
{
  return delayInitValue(minMax(sta)->opposite());
}

ArcDelay
PathEndUnconstrained::margin(const StaState *) const
{
  return delay_zero;
}

Slack
PathEndUnconstrained::slack(const StaState *) const
{
  return INF;
}


float
PathEndUnconstrained::sourceClkOffset(const StaState *) const
{
  return 0.0;
}

PathEnd::Type
PathEndUnconstrained::type() const
{
  return Type::unconstrained;
}

const char *
PathEndUnconstrained::typeName() const
{
  return "unconstrained";
}

////////////////////////////////////////////////////////////////

PathEndClkConstrained::PathEndClkConstrained(Path *path,
					     Path *clk_path) :
  PathEnd(path),
  clk_path_(clk_path),
  crpr_(0.0),
  crpr_valid_(false)
{
}

PathEndClkConstrained::PathEndClkConstrained(Path *path,
					     Path *clk_path,
					     Crpr crpr,
					     bool crpr_valid) :
  PathEnd(path),
  clk_path_(clk_path),
  crpr_(crpr),
  crpr_valid_(crpr_valid)
{
}

void
PathEndClkConstrained::setPath(Path *path)
{
  path_ = path;
  crpr_valid_ = false;
}

float
PathEndClkConstrained::sourceClkOffset(const StaState *sta) const
{
  return sourceClkOffset(sourceClkEdge(sta),
			 targetClkEdge(sta),
			 checkRole(sta),
			 sta);
}

float
PathEndClkConstrained::sourceClkOffset(const ClockEdge *src_clk_edge,
				       const ClockEdge *tgt_clk_edge,
				       const TimingRole *check_role,
				       const StaState *sta) const
{
  Sdc *sdc = sta->sdc();
  CycleAccting *acct = sdc->cycleAccting(src_clk_edge, tgt_clk_edge);
  return acct->sourceTimeOffset(check_role);
}

Arrival
PathEndClkConstrained::sourceClkLatency(const StaState *sta) const
{
  const ClkInfo *clk_info = path_->clkInfo(sta);
  return clk_info->latency();
}

Arrival
PathEndClkConstrained::sourceClkInsertionDelay(const StaState *sta) const
{
  const ClkInfo *clk_info = path_->clkInfo(sta);
  return clk_info->insertion();
}

Path *
PathEndClkConstrained::targetClkPath()
{
  return clk_path_;
}

const Path *
PathEndClkConstrained::targetClkPath() const
{
  return clk_path_;
}

float
PathEndClkConstrained::targetClkOffset(const StaState *sta) const
{
  const ClockEdge *src_clk_edge = sourceClkEdge(sta);
  const ClockEdge *tgt_clk_edge = targetClkEdge(sta);
  const TimingRole *check_role = checkRole(sta);
  Sdc *sdc = sta->sdc();
  CycleAccting *acct = sdc->cycleAccting(src_clk_edge, tgt_clk_edge);
  return acct->targetTimeOffset(check_role);
}

const ClockEdge *
PathEndClkConstrained::targetClkEdge(const StaState *sta) const
{
  if (clk_path_)
    return clk_path_->clkEdge(sta);
  else
    return nullptr;
}

const Clock *
PathEndClkConstrained::targetClk(const StaState *sta) const
{
  const ClockEdge *clk_edge = targetClkEdge(sta);
  if (clk_edge)
    return clk_edge->clock();
  else
    return nullptr;
}

float
PathEndClkConstrained::targetClkTime(const StaState *sta) const
{
  const ClockEdge *src_clk_edge = sourceClkEdge(sta);
  const ClockEdge *tgt_clk_edge = targetClkEdge(sta);
  const TimingRole *check_role = checkRole(sta);
  Sdc *sdc = sta->sdc();
  CycleAccting *acct = sdc->cycleAccting(src_clk_edge, tgt_clk_edge);
  return acct->requiredTime(check_role);
}

Arrival
PathEndClkConstrained::targetClkArrival(const StaState *sta) const
{
  return targetClkArrivalNoCrpr(sta)
    + checkCrpr(sta);
}
 
Arrival
PathEndClkConstrained::targetClkArrivalNoCrpr(const StaState *sta) const
{
  return targetClkTime(sta)
    + targetClkDelay(sta)
    + checkClkUncertainty(sourceClkEdge(sta),
 			  targetClkEdge(sta),
 			  targetClkPath(),
 			  checkRole(sta), sta)
    + targetClkMcpAdjustment(sta);
}

Delay
PathEndClkConstrained::targetClkDelay(const StaState *sta) const
{
  return checkTgtClkDelay(targetClkPath(), targetClkEdge(sta), checkRole(sta), sta);
}

Delay
PathEndClkConstrained::targetClkInsertionDelay(const StaState *sta) const
{
  Arrival insertion, latency;
  checkTgtClkDelay(targetClkPath(), targetClkEdge(sta),
		   checkRole(sta), sta,
		   insertion, latency);
  return insertion;
}

float
PathEndClkConstrained::targetNonInterClkUncertainty(const StaState *sta) const
{
  const ClockEdge *src_clk_edge = sourceClkEdge(sta);
  const ClockEdge *tgt_clk_edge = targetClkEdge(sta);
  const TimingRole *check_role = checkRole(sta);

  float inter_clk;
  bool inter_exists;
  checkInterClkUncertainty(src_clk_edge, tgt_clk_edge, check_role,
			   sta, inter_clk, inter_exists);
  if (inter_exists)
    // This returns non inter-clock uncertainty.
    return 0.0;
  else
    return checkTgtClkUncertainty(targetClkPath(), tgt_clk_edge, check_role, sta);
}

float
PathEndClkConstrained::interClkUncertainty(const StaState *sta) const
{
  float uncertainty;
  bool exists;
  checkInterClkUncertainty(sourceClkEdge(sta), targetClkEdge(sta),
			   checkRole(sta), sta,
			   uncertainty, exists);
  if (exists)
    return uncertainty;
  else
    return 0.0;
}

float
PathEndClkConstrained::targetClkUncertainty(const StaState *sta) const
{
  return checkClkUncertainty(sourceClkEdge(sta), targetClkEdge(sta),
			     targetClkPath(), checkRole(sta), sta);
}

Crpr
PathEndClkConstrained::crpr(const StaState *sta) const
{
  if (!crpr_valid_) {
    CheckCrpr *check_crpr = sta->search()->checkCrpr();
    crpr_ = check_crpr->checkCrpr(path_, targetClkPath());
    crpr_valid_ = true;
  }
  return crpr_;
}

Required
PathEndClkConstrained::requiredTime(const StaState *sta) const
{
  return requiredTimeNoCrpr(sta)
    + checkCrpr(sta);
}

Required
PathEndClkConstrained::requiredTimeNoCrpr(const StaState *sta) const
{
  Arrival tgt_clk_arrival = targetClkArrivalNoCrpr(sta);
  ArcDelay check_margin = margin(sta);
  if (checkGenericRole(sta) == TimingRole::setup())
    return tgt_clk_arrival - check_margin;
  else
    return tgt_clk_arrival + check_margin;
}

Slack
PathEndClkConstrained::slack(const StaState *sta) const
{
  Arrival arrival = dataArrivalTime(sta);
  Required required = requiredTime(sta);
  if (checkGenericRole(sta) == TimingRole::setup())
    return required - arrival;
  else
    return arrival - required;
}

int
PathEndClkConstrained::exceptPathCmp(const PathEnd *path_end,
				     const StaState *sta) const
{
  int cmp = PathEnd::exceptPathCmp(path_end, sta);
  if (cmp == 0) {
    const PathEndClkConstrained *path_end2 = 
      dynamic_cast<const PathEndClkConstrained*>(path_end);
    const Path *clk_path2 = path_end2->targetClkPath();
    return Path::cmp(targetClkPath(), clk_path2, sta);
  }
  else
    return cmp;
}

////////////////////////////////////////////////////////////////

PathEndClkConstrainedMcp::PathEndClkConstrainedMcp(Path *path,
						   Path *clk_path,
						   MultiCyclePath *mcp) :
  PathEndClkConstrained(path, clk_path),
  mcp_(mcp)
{
}

PathEndClkConstrainedMcp::PathEndClkConstrainedMcp(Path *path,
						   Path *clk_path,
						   MultiCyclePath *mcp,
						   Crpr crpr,
						   bool crpr_valid) :
  PathEndClkConstrained(path, clk_path, crpr, crpr_valid),
  mcp_(mcp)
{
}

float
PathEndClkConstrainedMcp::targetClkMcpAdjustment(const StaState *sta) const
{
  return checkMcpAdjustment(path_, targetClkEdge(sta), sta);
}

float
PathEndClkConstrainedMcp::checkMcpAdjustment(const Path *path,
					     const ClockEdge *tgt_clk_edge,
					     const StaState *sta) const
{
  if (mcp_) {
    const TimingRole *check_role = checkRole(sta);
    const MinMax *min_max = check_role->pathMinMax();
    const ClockEdge *src_clk_edge = path->clkEdge(sta);
    Sdc *sdc = sta->sdc();
    if (min_max == MinMax::max())
      return PathEnd::checkSetupMcpAdjustment(src_clk_edge, tgt_clk_edge,
					      mcp_, setupDefaultCycles(), sdc);
    else {
      // Hold check.
      // Default arrival clock is a proxy for the target clock.
      if (src_clk_edge == nullptr)
	src_clk_edge = tgt_clk_edge;
      else if (src_clk_edge->clock() == sdc->defaultArrivalClock())
	src_clk_edge = tgt_clk_edge->clock()->edge(src_clk_edge->transition());

      const MultiCyclePath *setup_mcp;
      const MultiCyclePath *hold_mcp;
      // Hold checks also need the setup mcp for cycle accounting.
      findHoldMcps(tgt_clk_edge, setup_mcp, hold_mcp, sta);
      if (setup_mcp && hold_mcp) {
	int setup_mult = setup_mcp->pathMultiplier(MinMax::max());
	int hold_mult = hold_mcp->pathMultiplier(MinMax::min());
	const ClockEdge *setup_clk_edge =
	  setup_mcp->useEndClk() ? tgt_clk_edge : src_clk_edge;
	float setup_period = setup_clk_edge->clock()->period();
	const ClockEdge *hold_clk_edge =
	  hold_mcp->useEndClk() ? tgt_clk_edge : src_clk_edge;
	float hold_period = hold_clk_edge->clock()->period();
	return (setup_mult - 1) * setup_period - hold_mult * hold_period;
      }
      else if (hold_mcp) {
	int mult = hold_mcp->pathMultiplier(min_max);
	const ClockEdge *clk_edge =
	  hold_mcp->useEndClk() ? tgt_clk_edge : src_clk_edge;
	float period = clk_edge->clock()->period();
	return -mult * period;
      }
      else if (setup_mcp) {
	int mult = setup_mcp->pathMultiplier(min_max);
	const ClockEdge *clk_edge =
	  setup_mcp->useEndClk() ? tgt_clk_edge : src_clk_edge;
	float period = clk_edge->clock()->period();
	return (mult - 1) * period;
      }
      else
	return 0.0;
    }
  }
  else
    return 0.0;
}

float
PathEnd::checkSetupMcpAdjustment(const ClockEdge *src_clk_edge,
				 const ClockEdge *tgt_clk_edge,
				 const MultiCyclePath *mcp,
				 int default_cycles,
				 Sdc *sdc)
{
  if (mcp) {
    // Default arrival clock is a proxy for the target clock.
    if (src_clk_edge == nullptr)
      src_clk_edge = tgt_clk_edge;
    else if (src_clk_edge->clock() == sdc->defaultArrivalClock())
      src_clk_edge = tgt_clk_edge->clock()->edge(src_clk_edge->transition());
    if (mcp->minMax()->matches(MinMax::max())) {
      int mult = mcp->pathMultiplier(MinMax::max());
      const ClockEdge *clk_edge =
	mcp->useEndClk() ? tgt_clk_edge : src_clk_edge;
      float period = clk_edge->clock()->period();
      return (mult - default_cycles) * period;
    }
    else
      return 0.0;
  }
  else
    return 0.0;
}

Slack
PathEndClkConstrained::slackNoCrpr(const StaState *sta) const
{
  Arrival arrival = dataArrivalTime(sta);
  Required required = requiredTimeNoCrpr(sta);
  if (checkGenericRole(sta) == TimingRole::setup())
    return required - arrival;
  else
    return arrival - required;
}

void
PathEndClkConstrainedMcp::findHoldMcps(const ClockEdge *tgt_clk_edge,
				       const MultiCyclePath *&setup_mcp,
				       const MultiCyclePath *&hold_mcp,
				       const StaState *sta) const

{
  Pin *pin = path_->pin(sta);
  const RiseFall *rf = path_->transition(sta);
  // Mcp may be setup, hold or setup_hold, since all match min paths.
  const MinMaxAll *mcp_min_max = mcp_->minMax();
  Search *search = sta->search();
  if (mcp_min_max->matches(MinMax::min())) {
    hold_mcp = mcp_;
    setup_mcp =
      dynamic_cast<MultiCyclePath*>(search->exceptionTo(ExceptionPathType::multi_cycle,
							path_, pin, rf,
							tgt_clk_edge,
							MinMax::max(), true,
							false));
  }
  else {
    setup_mcp = mcp_;
    hold_mcp =
      dynamic_cast<MultiCyclePath*>(search->exceptionTo(ExceptionPathType::multi_cycle,
							path_, pin, rf,
							tgt_clk_edge,
							MinMax::min(), true,
							false));
  }
}

int
PathEndClkConstrainedMcp::exceptPathCmp(const PathEnd *path_end,
					const StaState *sta) const
{
  int cmp = PathEndClkConstrained::exceptPathCmp(path_end, sta);
  if (cmp == 0) {
    const PathEndClkConstrainedMcp *path_end2 = 
      dynamic_cast<const PathEndClkConstrainedMcp*>(path_end);
    const MultiCyclePath *mcp2 = path_end2->mcp_;
    if (mcp_ == mcp2)
      return 0;
    else if (mcp_ < mcp2)
      return -1;
    else
      return 1;
  }
  else
    return cmp;
}

////////////////////////////////////////////////////////////////

PathEndCheck::PathEndCheck(Path *path,
			   TimingArc *check_arc,
			   Edge *check_edge,
			   Path *clk_path,
			   MultiCyclePath *mcp,
			   const StaState *) :
  PathEndClkConstrainedMcp(path, clk_path, mcp),
  check_arc_(check_arc),
  check_edge_(check_edge)
{
}

PathEndCheck::PathEndCheck(Path *path,
			   TimingArc *check_arc,
			   Edge *check_edge,
			   Path *clk_path,
			   MultiCyclePath *mcp,
			   Crpr crpr,
			   bool crpr_valid) :
  PathEndClkConstrainedMcp(path, clk_path, mcp, crpr, crpr_valid),
  check_arc_(check_arc),
  check_edge_(check_edge)
{
}

PathEnd *
PathEndCheck::copy() const
{
  return new PathEndCheck(path_, check_arc_, check_edge_,
			  clk_path_, mcp_, crpr_, crpr_valid_);
}

PathEnd::Type
PathEndCheck::type() const
{
  return Type::check;
}

const char *
PathEndCheck::typeName() const
{
  return "check";
}

void
PathEndCheck::reportFull(const ReportPath *report) const
{
  report->reportFull(this);
}

void
PathEndCheck::reportShort(const ReportPath *report) const
{
  report->reportShort(this);
}

const TimingRole *
PathEndCheck::checkRole(const StaState *) const
{
  return check_edge_->role();
}

ArcDelay
PathEndCheck::margin(const StaState *sta) const
{
  return sta->search()->deratedDelay(clk_path_->vertex(sta),
				     check_arc_, check_edge_, false,
				     pathAnalysisPt(sta));
}

int
PathEndCheck::exceptPathCmp(const PathEnd *path_end,
			    const StaState *sta) const
{
  int cmp = PathEndClkConstrainedMcp::exceptPathCmp(path_end, sta);
  if (cmp == 0) {
    const PathEndCheck *path_end2=dynamic_cast<const PathEndCheck*>(path_end);
    const TimingArc *check_arc2 = path_end2->check_arc_;
    if (check_arc_ == check_arc2)
      return 0;
    else if (check_arc_ < check_arc2)
      return -1;
    else
      return 1;
  }
  else
    return cmp;
}

Delay
PathEndCheck::clkSkew(const StaState *sta)
{
  return sourceClkDelay(sta) - targetClkDelay(sta) - crpr(sta)
    // Uncertainty decreases slack, but increases skew.
    - checkTgtClkUncertainty(clk_path_, clk_path_->clkEdge(sta),
                             checkRole(sta), sta);
}

Delay
PathEndCheck::sourceClkDelay(const StaState *sta) const
{
  PathExpanded expanded(path_, sta);
  const Path *src_clk_path = expanded.clkPath();
  if (src_clk_path) {
    const ClkInfo *src_clk_info = path_->tag(sta)->clkInfo();
    if (src_clk_info->isPropagated()) {
      // Propagated clock.  Propagated arrival is seeded with insertion delay.
      Arrival clk_arrival = src_clk_path->arrival();
      const ClockEdge *src_clk_edge = src_clk_info->clkEdge();
      Delay insertion = sourceClkInsertionDelay(sta);
      return delayRemove(clk_arrival - src_clk_edge->time(), insertion);
    }
    else
      // Ideal clock.
      return sourceClkLatency(sta);
  }
  else
    return 0.0;
}

Required
PathEndCheck::requiredTimeNoCrpr(const StaState *sta) const
{
  Arrival tgt_clk_arrival = targetClkArrivalNoCrpr(sta);
  ArcDelay check_margin = margin(sta);
  float macro_clk_tree_delay = macroClkTreeDelay(sta);
  if (checkGenericRole(sta) == TimingRole::setup())
    return tgt_clk_arrival - (check_margin + macro_clk_tree_delay);
  else
    return tgt_clk_arrival + (check_margin - macro_clk_tree_delay);
}

float
PathEndCheck::macroClkTreeDelay(const StaState *sta) const
{
  const ClockEdge *tgt_clk_edge = targetClkEdge(sta);
  const Clock *tgt_clk = tgt_clk_edge->clock();
  const Network *network = sta->network();
  const Pin *clk_pin = clk_path_->pin(sta);
  const Instance *inst = network->instance(clk_pin);
  const LibertyCell *inst_cell = network->libertyCell(inst);
  if (tgt_clk->isIdeal()
      && inst_cell && inst_cell->isMacro()) {
    LibertyPort *clk_port = network->libertyPort(clk_pin);
    if (clk_port) {
      const MinMax *min_max = clk_path_->minMax(sta);
      const RiseFall *rf = clk_path_->transition(sta);
      float slew = delayAsFloat(clk_path_->slew(sta));
      return clk_port->clkTreeDelay(slew, rf, min_max);
    }
  }
  return 0.0;
}

////////////////////////////////////////////////////////////////

PathEndLatchCheck::PathEndLatchCheck(Path *path,
				     TimingArc *check_arc,
				     Edge *check_edge,
				     Path *disable_path,
				     MultiCyclePath *mcp,
				     PathDelay *path_delay,
				     const StaState *sta) :
  PathEndCheck(path, check_arc, check_edge, nullptr, mcp, sta),
  disable_path_(disable_path),
  path_delay_(path_delay),
  src_clk_arrival_(0.0)
{
  Latches *latches = sta->latches();
  Path *enable_path =
    latches->latchEnableOtherPath(disable_path,
                                  disable_path->pathAnalysisPt(sta));
  clk_path_ = enable_path;
  Search *search = sta->search();
  // Same as PathEndPathDelay::findRequired.
  if (path_delay_ && ignoreClkLatency(sta))
    src_clk_arrival_ = search->pathClkPathArrival(path_);
}

PathEndLatchCheck::PathEndLatchCheck(Path *path,
				     TimingArc *check_arc,
				     Edge *check_edge,
				     Path *clk_path,
				     Path *disable_path,
				     MultiCyclePath *mcp,
				     PathDelay *path_delay,
 				     Delay src_clk_arrival,
				     Crpr crpr,
				     bool crpr_valid) :
  PathEndCheck(path, check_arc, check_edge, clk_path, mcp, crpr, crpr_valid),
  disable_path_(disable_path),
  path_delay_(path_delay),
  src_clk_arrival_(src_clk_arrival)
{
}

PathEnd *
PathEndLatchCheck::copy() const
{
  return new PathEndLatchCheck(path_, check_arc_, check_edge_,
			       clk_path_, disable_path_, mcp_, path_delay_,
			       src_clk_arrival_, crpr_, crpr_valid_);
}

PathEnd::Type
PathEndLatchCheck::type() const
{
  return Type::latch_check;
}

const char *
PathEndLatchCheck::typeName() const
{
  return "latch_check";
}

Path *
PathEndLatchCheck::latchDisable()
{
  return disable_path_;
}

const Path *
PathEndLatchCheck::latchDisable() const
{
  return disable_path_;
}

void
PathEndLatchCheck::reportFull(const ReportPath *report) const
{
  report->reportFull(this);
}

void
PathEndLatchCheck::reportShort(const ReportPath *report) const
{
  report->reportShort(this);
}

float
PathEndLatchCheck::sourceClkOffset(const StaState *sta) const
{
  if (path_delay_)
    return pathDelaySrcClkOffset(path_, path_delay_, src_clk_arrival_, sta);
  else
    return PathEndClkConstrained::sourceClkOffset(sourceClkEdge(sta),
						  disable_path_->clkEdge(sta),
						  TimingRole::setup(),
						  sta);
}

const TimingRole *
PathEndLatchCheck::checkRole(const StaState *sta) const
{
  if (clk_path_->clkInfo(sta)->isPulseClk())
    // Pulse latches use register cycle accounting.
    return TimingRole::setup();
  else
    // Setup cycle accting is slightly different because it is wrt
    // the enable opening edge, not the disable (setup check) edge.
    return TimingRole::latchSetup();
}

float
PathEndLatchCheck::targetClkTime(const StaState *sta) const
{
  if (path_delay_)
    return 0.0;
  else
    return PathEndClkConstrained::targetClkTime(sta);
}

float
PathEndLatchCheck::targetClkOffset(const StaState *sta) const
{
  if (path_delay_)
    return -targetClkEdge(sta)->time();
  else
    return PathEndClkConstrained::targetClkOffset(sta);
}

Required
PathEndLatchCheck::requiredTime(const StaState *sta) const
{
  Required required;
  Arrival borrow, adjusted_data_arrival, time_given_to_startpoint;
  Latches *latches = sta->latches();
  latches->latchRequired(path_, targetClkPath(), latchDisable(),
			 mcp_, path_delay_, src_clk_arrival_, margin(sta),
			 required, borrow, adjusted_data_arrival,
			 time_given_to_startpoint);
  return required;
}

Arrival
PathEndLatchCheck::borrow(const StaState *sta) const
{
  Latches *latches = sta->latches();
  Required required;
  Arrival  borrow, adjusted_data_arrival, time_given_to_startpoint;
  latches->latchRequired(path_, targetClkPath(), latchDisable(),
			 mcp_, path_delay_, src_clk_arrival_, margin(sta),
			 required, borrow, adjusted_data_arrival,
			 time_given_to_startpoint);
  return borrow;
}

void 
PathEndLatchCheck::latchRequired(const StaState *sta,
				 // Return values.
				 Required &required,
				 Delay &borrow,
				 Arrival &adjusted_data_arrival,
				 Delay &time_given_to_startpoint) const
{
  Latches *latches = sta->latches();
  latches->latchRequired(path_, targetClkPath(), latchDisable(),
			 mcp_, path_delay_, src_clk_arrival_, margin(sta),
			 required, borrow, adjusted_data_arrival,
			 time_given_to_startpoint);
}

void 
PathEndLatchCheck::latchBorrowInfo(const StaState *sta,
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
  Latches *latches = sta->latches();
  latches->latchBorrowInfo(path_, targetClkPath(), latchDisable(),
			   margin(sta),
			   path_delay_ && ignoreClkLatency(sta),
			   nom_pulse_width, open_latency,
			   latency_diff, open_uncertainty,
			   open_crpr, crpr_diff, max_borrow,
			   borrow_limit_exists);
}

Arrival
PathEndLatchCheck::targetClkWidth(const StaState *sta) const
{
  const Search *search = sta->search();
  Arrival disable_arrival = search->clkPathArrival(disable_path_);
  Arrival enable_arrival = search->clkPathArrival(clk_path_);
  const ClkInfo *enable_clk_info = clk_path_->clkInfo(sta);
  if (enable_clk_info->isPulseClk())
    return disable_arrival - enable_arrival;
  else {
    if (delayGreater(enable_arrival, disable_arrival, sta)) {
      const Clock *disable_clk = enable_clk_info->clock();
      if (disable_clk)
        disable_arrival += disable_clk->period();
    }
    return disable_arrival - enable_arrival;
  }
}

int
PathEndLatchCheck::exceptPathCmp(const PathEnd *path_end,
				 const StaState *sta) const
{
  int cmp = PathEndClkConstrainedMcp::exceptPathCmp(path_end, sta);
  if (cmp == 0) {
    const PathEndLatchCheck *path_end2 =
      dynamic_cast<const PathEndLatchCheck*>(path_end);
    const TimingArc *check_arc2 = path_end2->check_arc_;
    if (check_arc_ == check_arc2) {
      const Path *disable_path2 = path_end2->disable_path_;
      return Path::cmp(disable_path_, disable_path2, sta);
    }
    else if (check_arc_ < check_arc2)
      return -1;
    else
      return 1;
  }
  else
    return cmp;
}

bool
PathEndLatchCheck::ignoreClkLatency(const StaState *sta) const
{
  return PathEnd::ignoreClkLatency(path_, path_delay_, sta);
}

///////////////////////////////////////////////////////////////

PathEndOutputDelay::PathEndOutputDelay(OutputDelay *output_delay,
				       Path *path,
				       Path *clk_path,
				       MultiCyclePath *mcp,
				       const StaState *) :
  // No target clk_path_ for output delays.
  PathEndClkConstrainedMcp(path, clk_path, mcp),
  output_delay_(output_delay)
{
}

PathEndOutputDelay::PathEndOutputDelay(OutputDelay *output_delay,
				       Path *path,
				       Path *clk_path,
				       MultiCyclePath *mcp,
				       Crpr crpr,
				       bool crpr_valid) :
  PathEndClkConstrainedMcp(path, clk_path, mcp, crpr, crpr_valid),
  output_delay_(output_delay)
{
}

PathEnd *
PathEndOutputDelay::copy() const
{
  return new PathEndOutputDelay(output_delay_, path_, clk_path_,
				mcp_, crpr_, crpr_valid_);
}

PathEnd::Type
PathEndOutputDelay::type() const
{
  return Type::output_delay;
}

const char *
PathEndOutputDelay::typeName() const
{
  return "output_delay";
}

void
PathEndOutputDelay::reportFull(const ReportPath *report) const
{
  report->reportFull(this);
}

void
PathEndOutputDelay::reportShort(const ReportPath *report) const
{
  report->reportShort(this);
}

ArcDelay
PathEndOutputDelay::margin(const StaState *sta) const
{
  return outputDelayMargin(output_delay_, path_, sta);
}

float
PathEnd::outputDelayMargin(OutputDelay *output_delay,
			   const Path *path,
			   const StaState *sta)
{
  const RiseFall *rf = path->transition(sta);
  const MinMax *min_max = path->minMax(sta);
  float margin = output_delay->delays()->value(rf, min_max);
  if (min_max == MinMax::max())
    return margin;
  else
    return -margin;
}

const TimingRole *
PathEndOutputDelay::checkRole(const StaState *sta) const
{
  if (path_->minMax(sta) == MinMax::max())
    return TimingRole::outputSetup();
  else
    return TimingRole::outputHold();
}

const ClockEdge *
PathEndOutputDelay::targetClkEdge(const StaState *sta) const
{
  if (clk_path_)
    return clk_path_->clkEdge(sta);
  else
    return output_delay_->clkEdge();
}

Arrival
PathEndOutputDelay::targetClkArrivalNoCrpr(const StaState *sta) const
{
  if (clk_path_)
    return PathEndClkConstrained::targetClkArrivalNoCrpr(sta);
  else {
    const ClockEdge *tgt_clk_edge = targetClkEdge(sta);
    const TimingRole *check_role = checkRole(sta);
    return targetClkTime(sta)
      + tgtClkDelay(tgt_clk_edge, check_role, sta)
      + targetClkUncertainty(sta)
      + checkMcpAdjustment(path_, tgt_clk_edge, sta);
  }
}

Crpr
PathEndOutputDelay::crpr(const StaState *sta) const
{
  if (!crpr_valid_) {
    CheckCrpr *check_crpr = sta->search()->checkCrpr();
    crpr_ = check_crpr->outputDelayCrpr(path_, targetClkEdge(sta));
    crpr_valid_ = true;
  }
  return crpr_;
}

Delay
PathEndOutputDelay::targetClkDelay(const StaState *sta) const
{
  if (clk_path_)
    return PathEndClkConstrained::targetClkDelay(sta);
  else
    return tgtClkDelay(targetClkEdge(sta), checkRole(sta), sta);
}

Arrival
PathEndOutputDelay::tgtClkDelay(const ClockEdge *tgt_clk_edge,
				const TimingRole *check_role,
				const StaState *sta) const
{
  Arrival insertion, latency;
  tgtClkDelay(tgt_clk_edge, check_role, sta,
	      insertion, latency);
  return insertion + latency;
}

void
PathEndOutputDelay::tgtClkDelay(const ClockEdge *tgt_clk_edge,
				const TimingRole *check_role,
				const StaState *sta,
				// Return values.
				Arrival &insertion,
				Arrival &latency) const
{
  // Early late: setup early, hold late.
  const EarlyLate *early_late = check_role->tgtClkEarlyLate();
  // Latency min_max depends on bc_wc or ocv.
  const PathAnalysisPt *path_ap = path_->pathAnalysisPt(sta);
  const MinMax *latency_min_max = path_ap->tgtClkAnalysisPt()->pathMinMax();
  Clock *tgt_clk = tgt_clk_edge->clock();
  const RiseFall *tgt_clk_rf = tgt_clk_edge->transition();
  if (!output_delay_->sourceLatencyIncluded())
    insertion = sta->search()->clockInsertion(tgt_clk,
					      tgt_clk->defaultPin(),
					      tgt_clk_rf,
					      latency_min_max,
					      early_late, path_ap);
  else
    insertion = 0.0;
  const Sdc *sdc = sta->sdc();
  if (!tgt_clk->isPropagated()
      && !output_delay_->networkLatencyIncluded())
    latency = sdc->clockLatency(tgt_clk, tgt_clk_rf, latency_min_max);
  else
    latency = 0.0;
}

Delay
PathEndOutputDelay::targetClkInsertionDelay(const StaState *sta) const
{
  if (clk_path_)
    return PathEndClkConstrained::targetClkInsertionDelay(sta);
  else {
    Arrival insertion, latency;
    tgtClkDelay(targetClkEdge(sta), checkRole(sta), sta,
		insertion, latency);
    return insertion;
  }
}

int
PathEndOutputDelay::exceptPathCmp(const PathEnd *path_end,
				  const StaState *sta) const
{
  int cmp = PathEndClkConstrainedMcp::exceptPathCmp(path_end, sta);
  if (cmp == 0) {
    const PathEndOutputDelay *path_end2 =
      dynamic_cast<const PathEndOutputDelay*>(path_end);
    OutputDelay *output_delay2 = path_end2->output_delay_;
    if (output_delay_ == output_delay2)
      return 0;
    else if (output_delay_ < output_delay2)
      return -1;
    else
      return 1;
  }
  else
    return cmp;
}

////////////////////////////////////////////////////////////////

PathEndGatedClock::PathEndGatedClock(Path *gating_ref,
				     Path *clk_path,
				     const TimingRole *check_role,
				     MultiCyclePath *mcp,
				     ArcDelay margin,
				     const StaState *) :
  PathEndClkConstrainedMcp(gating_ref, clk_path, mcp),
  check_role_(check_role),
  margin_(margin)
{
}

PathEndGatedClock::PathEndGatedClock(Path *gating_ref,
				     Path *clk_path,
				     const TimingRole *check_role,
				     MultiCyclePath *mcp,
				     ArcDelay margin,
				     Crpr crpr,
				     bool crpr_valid) :
  PathEndClkConstrainedMcp(gating_ref, clk_path, mcp, crpr, crpr_valid),
  check_role_(check_role),
  margin_(margin)
{
}

PathEnd *
PathEndGatedClock::copy() const
{
  return new PathEndGatedClock(path_, clk_path_, check_role_,
			       mcp_, margin_, crpr_, crpr_valid_);
}

PathEnd::Type
PathEndGatedClock::type() const
{
  return Type::gated_clk;
}

const char *
PathEndGatedClock::typeName() const
{
  return "gated_clk";
}

const TimingRole *
PathEndGatedClock::checkRole(const StaState *) const
{
  return check_role_;
}

void
PathEndGatedClock::reportFull(const ReportPath *report) const
{
  report->reportFull(this);
}

void
PathEndGatedClock::reportShort(const ReportPath *report) const
{
  report->reportShort(this);
}

int
PathEndGatedClock::exceptPathCmp(const PathEnd *path_end,
				 const StaState *sta) const
{
  int cmp = PathEndClkConstrainedMcp::exceptPathCmp(path_end, sta);
  if (cmp == 0) {
    const PathEndGatedClock *path_end2 =
      dynamic_cast<const PathEndGatedClock*>(path_end);
    const TimingRole *check_role2 = path_end2->check_role_;
    if (check_role_ == check_role2)
      return 0;
    else if (check_role_ < check_role2)
      return -1;
    else
      return 1;
  }
  else
    return cmp;
}

////////////////////////////////////////////////////////////////

PathEndDataCheck::PathEndDataCheck(DataCheck *check,
				   Path *data_path,
				   Path *data_clk_path,
				   MultiCyclePath *mcp,
				   const StaState *sta) :
  PathEndClkConstrainedMcp(data_path, nullptr, mcp),
  data_clk_path_(data_clk_path),
  check_(check)
{
  clk_path_ = clkPath(data_clk_path, sta);
}


// PathExpanded::expand() and PathExpanded::clkPath().
Path *
PathEndDataCheck::clkPath(Path *path,
                          const StaState *sta)
{
  Path *p = path;
  while (p) {
    Path *prev_path = p->prevPath();
    TimingArc *prev_arc = p->prevArc(sta);

    if (p->isClock(sta))
      return p;
    if (prev_arc) {
      const TimingRole *prev_role = prev_arc->role();
      if (prev_role == TimingRole::regClkToQ()
	  || prev_role == TimingRole::latchEnToQ()) {
        prev_path = p->prevPath();
	return prev_path;
      }
      else if (prev_role == TimingRole::latchDtoQ()) {
	const Latches *latches = sta->latches();
	Edge *prev_edge = p->prevEdge(sta);
	Path *enable_path = latches->latchEnablePath(p, prev_edge);
	return enable_path;
      }
    }
    p = prev_path;
  }
  return nullptr;
}

PathEndDataCheck::PathEndDataCheck(DataCheck *check,
				   Path *data_path,
				   Path *data_clk_path,
				   Path *clk_path,
				   MultiCyclePath *mcp,
 				   Crpr crpr,
 				   bool crpr_valid) :
  PathEndClkConstrainedMcp(data_path, clk_path, mcp, crpr, crpr_valid),
  data_clk_path_(data_clk_path),
  check_(check)
{
}

PathEnd *
PathEndDataCheck::copy() const
{
  return new PathEndDataCheck(check_, path_, data_clk_path_,
 			      clk_path_, mcp_, crpr_, crpr_valid_);
}

PathEnd::Type
PathEndDataCheck::type() const
{
  return Type::data_check;
}

const char *
PathEndDataCheck::typeName() const
{
  return "data_check";
}

const ClockEdge *
PathEndDataCheck::targetClkEdge(const StaState *sta) const
{
  // clk_path_ can be null if data_clk_path is from an input port.
  return data_clk_path_->clkEdge(sta);
}

Arrival
PathEndDataCheck::requiredTimeNoCrpr(const StaState *sta) const
{
  Arrival data_clk_arrival = data_clk_path_->arrival();
  float data_clk_time = data_clk_path_->clkEdge(sta)->time();
  Arrival data_clk_delay = data_clk_arrival - data_clk_time;
  Arrival tgt_clk_arrival = targetClkTime(sta)
    + data_clk_delay
    + targetClkUncertainty(sta)
    + targetClkMcpAdjustment(sta);

  ArcDelay check_margin = margin(sta);
  if (checkGenericRole(sta) == TimingRole::setup())
    return tgt_clk_arrival - check_margin;
  else
    return tgt_clk_arrival + check_margin;
}

ArcDelay
PathEndDataCheck::margin(const StaState *sta) const
{
  float margin;
  bool margin_exists;
  check_->margin(data_clk_path_->transition(sta),
		 path_->transition(sta),
		 path_->minMax(sta),
		 margin, margin_exists);
  return margin;
}

const TimingRole *
PathEndDataCheck::checkRole(const StaState *sta) const
{
  if (path_->minMax(sta) == MinMax::max())
    return TimingRole::dataCheckSetup();
  else
    return TimingRole::dataCheckHold();
}

void
PathEndDataCheck::reportFull(const ReportPath *report) const
{
  report->reportFull(this);
}

void
PathEndDataCheck::reportShort(const ReportPath *report) const
{
  report->reportShort(this);
}

int
PathEndDataCheck::exceptPathCmp(const PathEnd *path_end,
				const StaState *sta) const
{
  int cmp = PathEndClkConstrainedMcp::exceptPathCmp(path_end, sta);
  if (cmp == 0) {
    const PathEndDataCheck *path_end2 = 
      dynamic_cast<const PathEndDataCheck*>(path_end);
    const DataCheck *check2 = path_end2->check_;
    if (check_ == check2)
      return 0;
    else if (check_ < check2)
      return -1;
    else
      return 1;
  }
  else
    return cmp;
}

////////////////////////////////////////////////////////////////

PathEndPathDelay::PathEndPathDelay(PathDelay *path_delay,
				   Path *path,
				   const StaState *sta):
  PathEndClkConstrained(path, nullptr),
  path_delay_(path_delay),
  check_arc_(nullptr),
  check_edge_(nullptr),
  output_delay_(nullptr)
{
  findSrcClkArrival(sta);
}

PathEndPathDelay::PathEndPathDelay(PathDelay *path_delay,
				   Path *path,
				   OutputDelay *output_delay,
				   const StaState *sta):
  PathEndClkConstrained(path, nullptr),
  path_delay_(path_delay),
  check_arc_(nullptr),
  check_edge_(nullptr),
  output_delay_(output_delay)
{
  findSrcClkArrival(sta);
}

PathEndPathDelay::PathEndPathDelay(PathDelay *path_delay,
				   Path *path,
				   Path *clk_path,
				   TimingArc *check_arc,
				   Edge *check_edge,
				   const StaState *sta) :
  PathEndClkConstrained(path, clk_path),
  path_delay_(path_delay),
  check_arc_(check_arc),
  check_edge_(check_edge),
  output_delay_(nullptr)
{
  findSrcClkArrival(sta);
}

PathEndPathDelay::PathEndPathDelay(PathDelay *path_delay,
				   Path *path,
				   Path *clk_path,
				   TimingArc *check_arc,
				   Edge *check_edge,
				   OutputDelay *output_delay,
				   Arrival src_clk_arrival,
				   Crpr crpr,
				   bool crpr_valid) :
  PathEndClkConstrained(path, clk_path, crpr, crpr_valid),
  path_delay_(path_delay),
  check_arc_(check_arc),
  check_edge_(check_edge),
  output_delay_(output_delay),
  src_clk_arrival_(src_clk_arrival)
{
}

PathEnd *
PathEndPathDelay::copy() const
{
  return new PathEndPathDelay(path_delay_, path_, clk_path_,
			      check_arc_, check_edge_, output_delay_,
			      src_clk_arrival_, crpr_, crpr_valid_);
}

PathEnd::Type
PathEndPathDelay::type() const
{
  return Type::path_delay;
}

const char *
PathEndPathDelay::typeName() const
{
  return "path_delay";
}

void
PathEndPathDelay::findSrcClkArrival(const StaState *sta)
{
  if (ignoreClkLatency(sta)) {
    Search *search = sta->search();
    src_clk_arrival_ = search->pathClkPathArrival(path_);
  }
  else
    src_clk_arrival_ = 0.0;

}

void
PathEndPathDelay::reportFull(const ReportPath *report) const
{
  report->reportFull(this);
}

void
PathEndPathDelay::reportShort(const ReportPath *report) const
{
  report->reportShort(this);
}

bool
PathEndPathDelay::pathDelayMarginIsExternal() const
{
  return check_arc_ == nullptr;
}

const TimingRole *
PathEndPathDelay::checkRole(const StaState *sta) const
{
  if (check_edge_)
    return check_edge_->role();
  else if (minMax(sta) == MinMax::max())
    return TimingRole::setup();
  else
    return TimingRole::hold();
}

ArcDelay
PathEndPathDelay::margin(const StaState *sta) const
{
  if (check_arc_) {
    return sta->search()->deratedDelay(check_edge_->from(sta->graph()),
				       check_arc_, check_edge_, false,
				       pathAnalysisPt(sta));
  }
  else if (output_delay_)
    return outputDelayMargin(output_delay_, path_, sta);
  else
    return delay_zero;
}

float
PathEndPathDelay::sourceClkOffset(const StaState *sta) const
{
  return pathDelaySrcClkOffset(path_, path_delay_, src_clk_arrival_, sta);
}

Delay
PathEnd::clkSkew(const StaState *)
{
  return 0.0;
}

// Helper shared by PathEndLatchCheck.
float
PathEnd::pathDelaySrcClkOffset(const Path *path,
			       PathDelay *path_delay,
			       Arrival src_clk_arrival,
			       const StaState *sta)
{
  float offset = 0.0;
  const ClockEdge *clk_edge = path->clkEdge(sta);
  if (clk_edge) {
    if (ignoreClkLatency(path, path_delay, sta))
      offset = -delayAsFloat(src_clk_arrival);
    else
      // Arrival includes src clock edge time that is not counted in the
      // path delay.
      offset = -clk_edge->time();
  }
  return offset;
}

bool
PathEnd::ignoreClkLatency(const Path *path,
                          PathDelay *path_delay,
                          const StaState *sta)
{
  return path_delay->ignoreClkLatency() && !path->isClock(sta);
}

const ClockEdge *
PathEndPathDelay::targetClkEdge(const StaState *sta) const
{
  if (clk_path_)
    return clk_path_->clkEdge(sta);
  else if (output_delay_)
    return output_delay_->clkEdge();
  else
    return nullptr;
}

float 
PathEndPathDelay::targetClkTime(const StaState *sta) const
{
  const ClockEdge *tgt_clk_edge = targetClkEdge(sta);
  if (tgt_clk_edge)
    return tgt_clk_edge->time();
  else
    return 0.0;
}

Arrival
PathEndPathDelay::targetClkArrivalNoCrpr(const StaState *sta) const
{
  const ClockEdge *tgt_clk_edge = targetClkEdge(sta);
  if (tgt_clk_edge)
    return targetClkDelay(sta)
      + targetClkUncertainty(sta);
  else if (clk_path_)
    return clk_path_->arrival();
  else
    return 0.0;
}

float 
PathEndPathDelay::targetClkOffset(const StaState *) const
{
  return 0.0;
}

Required
PathEndPathDelay::requiredTime(const StaState *sta) const
{
  float delay = path_delay_->delay();
  if (path_delay_->ignoreClkLatency()) {
    Required src_offset = path_->isClock(sta)
      ? path_->clkEdge(sta)->time()
      : src_clk_arrival_;
    return src_offset + delay
      + ((minMax(sta) == MinMax::max()) ? -margin(sta) : margin(sta));
  }
  else {
    Arrival tgt_clk_arrival = targetClkArrival(sta);
    float src_clk_offset = sourceClkOffset(sta);
    // Path delay includes target clk latency and timing check setup/hold 
    // margin or external departure at target.
    return delay - src_clk_offset + tgt_clk_arrival
      + ((minMax(sta) == MinMax::max()) ? -margin(sta) : margin(sta));
  }
}

bool
PathEndPathDelay::ignoreClkLatency(const StaState *sta) const
{
  return PathEnd::ignoreClkLatency(path_, path_delay_, sta);
}

int
PathEndPathDelay::exceptPathCmp(const PathEnd *path_end,
				const StaState *sta) const
{
  int cmp = PathEndClkConstrained::exceptPathCmp(path_end, sta);
  if (cmp == 0) {
    const PathEndPathDelay *path_end2 =
      dynamic_cast<const PathEndPathDelay*>(path_end);
    PathDelay *path_delay2 = path_end2->path_delay_;
    if (path_delay_ == path_delay2) {
      const TimingArc *check_arc2 = path_end2->check_arc_;
      if (check_arc_ == check_arc2)
	return 0;
      else if (check_arc_ < check_arc2)
	return -1;
      else
	return 1;
    }
    else if (path_delay_ < path_delay2)
      return -1;
    else
      return 1;
  }
  else
    return cmp;
}

////////////////////////////////////////////////////////////////

PathEndLess::PathEndLess(const StaState *sta) :
  sta_(sta)
{
}

bool
PathEndLess::operator()(const PathEnd *path_end1,
			const PathEnd *path_end2) const
{
  return PathEnd::less(path_end1, path_end2, sta_);
}

bool
PathEnd::less(const PathEnd *path_end1,
	      const PathEnd *path_end2,
	      const StaState *sta)
{
  return cmp(path_end1, path_end2, sta) < 0;
}

int
PathEnd::cmp(const PathEnd *path_end1,
	     const PathEnd *path_end2,
	     const StaState *sta)
{
  int cmp = path_end1->isUnconstrained()
    ? -cmpArrival(path_end1, path_end2, sta)
    : cmpSlack(path_end1, path_end2, sta);
  if (cmp == 0) {
    const Path *path1 = path_end1->path();
    const Path *path2 = path_end2->path();
    cmp = Path::cmpPinTrClk(path1, path2, sta);
    if (cmp == 0) {
      const Path *clk_path1 = path_end1->targetClkPath();
      const Path *clk_path2 = path_end2->targetClkPath();
      cmp = Path::cmpPinTrClk(clk_path1, clk_path2, sta);
      if (cmp == 0) {
	cmp = Path::cmpAll(path1, path2, sta);
	if (cmp == 0)
	  cmp = Path::cmpAll(clk_path1, clk_path2, sta);
      }
    }
  }
  return cmp;
}

int
PathEnd::cmpSlack(const PathEnd *path_end1,
		  const PathEnd *path_end2,
		  const StaState *sta)
{
  Slack slack1 = path_end1->slack(sta);
  Slack slack2 = path_end2->slack(sta);
  if (delayZero(slack1)
      && delayZero(slack2)
      && path_end1->isLatchCheck()
      && path_end2->isLatchCheck()) {
    Arrival borrow1 = path_end1->borrow(sta);
    Arrival borrow2 = path_end2->borrow(sta);
    // Latch slack is zero if there is borrowing so break ties
    // based on borrow time.
    if (delayEqual(borrow1, borrow2))
      return 0;
    else if (delayGreater(borrow1, borrow2, sta))
      return -1;
    else
      return 1;
  }
  else if (delayEqual(slack1, slack2))
    return 0;
  else if (delayLess(slack1, slack2, sta))
    return -1;
  else
    return 1;
}

int
PathEnd::cmpArrival(const PathEnd *path_end1,
		    const PathEnd *path_end2,
		    const StaState *sta)
{
  Arrival arrival1 = path_end1->dataArrivalTime(sta);
  Arrival arrival2 = path_end2->dataArrivalTime(sta);
  const MinMax *min_max = path_end1->minMax(sta);
  if (delayEqual(arrival1, arrival2))
    return 0;
  else if (delayLess(arrival1, arrival2, min_max, sta))
    return -1;
  else
    return 1;
}

int
PathEnd::cmpNoCrpr(const PathEnd *path_end1,
		   const PathEnd *path_end2,
		   const StaState *sta)
{
  int cmp = path_end1->exceptPathCmp(path_end2, sta);
  if (cmp == 0) {
    const Path *path1 = path_end1->path();
    const Path *path2 = path_end2->path();
    return Path::cmpNoCrpr(path1, path2, sta);
  }
  else
    return cmp;
}

////////////////////////////////////////////////////////////////

PathEndSlackLess::PathEndSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
PathEndSlackLess::operator()(const PathEnd *path_end1,
			     const PathEnd *path_end2) const
{
  int cmp = path_end1->isUnconstrained()
    ? -PathEnd::cmpArrival(path_end1, path_end2, sta_)
    : PathEnd::cmpSlack(path_end1, path_end2, sta_);
  return cmp < 0;
}

////////////////////////////////////////////////////////////////

PathEndNoCrprLess::PathEndNoCrprLess(const StaState *sta) :
  sta_(sta)
{
}

bool
PathEndNoCrprLess::operator()(const PathEnd *path_end1,
			      const PathEnd *path_end2) const
{
  int cmp = path_end1->exceptPathCmp(path_end2, sta_);
  if (cmp == 0) {
    const Path *path1 = path_end1->path();
    const Path *path2 = path_end2->path();
    return Path::cmpNoCrpr(path1, path2, sta_) < 0;
  }
  else
    return cmp < 0;
}

} // namespace
