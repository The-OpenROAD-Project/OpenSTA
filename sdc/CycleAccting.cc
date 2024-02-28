// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#include "CycleAccting.hh"

#include <cmath> // ceil
#include <algorithm> // max

#include "Debug.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "TimingRole.hh"
#include "Clock.hh"
#include "Sdc.hh"

namespace sta {

CycleAcctings::CycleAcctings(Sdc *sdc) :
  sdc_(sdc)
{
}

CycleAcctings::~CycleAcctings()
{
  clear();
}

void
CycleAcctings::clear()
{
  cycle_acctings_.deleteContentsClear();
}

// Determine cycle accounting "on demand".
CycleAccting *
CycleAcctings::cycleAccting(const ClockEdge *src,
                            const ClockEdge *tgt)
{
  if (src == nullptr)
    src = tgt;
  CycleAccting probe(src, tgt);
  CycleAccting *acct = cycle_acctings_.findKey(&probe);
  if (acct == nullptr) {
    acct = new CycleAccting(src, tgt);
    if (src == sdc_->defaultArrivalClockEdge())
      acct->findDefaultArrivalSrcDelays();
    else
      acct->findDelays(sdc_);
    cycle_acctings_.insert(acct);
  }
  return acct;
}

void
CycleAcctings::reportClkToClkMaxCycleWarnings(Report *report)
{
  // Find cycle acctings that exceed max cycle count.  Eliminate
  // duplicate warnings between different src/tgt clk edges.
  ClockPairSet clk_warnings;
  for (Clock *src_clk : *sdc_->clocks()) {
    for (RiseFall *src_rf : RiseFall::range()) {
      ClockEdge *src = src_clk->edge(src_rf);
      for (Clock *tgt_clk : *sdc_->clocks()) {
        for (RiseFall *tgt_rf : RiseFall::range()) {
          ClockEdge *tgt = tgt_clk->edge(tgt_rf);
          CycleAccting probe(src, tgt);
          CycleAccting *acct = cycle_acctings_.findKey(&probe);
          if (acct && acct->maxCyclesExceeded()) {
            // Canonicalize the warning wrt src/tgt.
            ClockPair clk_pair1(src_clk, tgt_clk);
            ClockPair clk_pair2(tgt_clk, src_clk);
            if (!clk_warnings.hasKey(clk_pair1)
                && !clk_warnings.hasKey(clk_pair2)) {
              report->warn(1010, "No common period was found between clocks %s and %s.",
                           src_clk->name(),
                           tgt_clk->name());
              clk_warnings.insert(clk_pair1);
            }
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

CycleAccting::CycleAccting(const ClockEdge *src,
			   const ClockEdge *tgt) :
  src_(src),
  tgt_(tgt),
  max_cycles_exceeded_(false)
{
  for (int i = 0; i <= TimingRole::index_max; i++) {
    delay_[i] = MinMax::min()->initValue();
    required_[i] = 0;
    src_cycle_[i] = 0;
    tgt_cycle_[i] = 0;
  }
}

void
CycleAccting::findDelays(StaState *sta)
{
  Debug *debug = sta->debug();
  const Unit *time_unit = sta->units()->timeUnit();
  debugPrint(debug, "cycle_acct", 1, "%s -> %s",
             src_->name(),
             tgt_->name());
  const int setup_index = TimingRole::setup()->index();
  const int latch_setup_index = TimingRole::latchSetup()->index();
  const int data_check_setup_index = TimingRole::dataCheckSetup()->index();
  const int hold_index = TimingRole::hold()->index();
  const int gclk_hold_index = TimingRole::gatedClockHold()->index();
  Clock *src_clk = src_->clock();
  Clock *tgt_clk = tgt_->clock();
  double tgt_opp_time1 = tgt_->opposite()->time();
  double tgt_period = tgt_clk->period();
  double src_period = src_clk->period();
  if (tgt_period > 0.0 && src_period > 0.0) {
    // If the clocks are related (ie, generated clock and its source) allow
    // allow enough cycles to match up the common period.
    int tgt_max_cycle;
    if (tgt_period > src_period)
      tgt_max_cycle = 100;
    else {
      int ratio = std::ceil(src_period / tgt_period);
      tgt_max_cycle = std::max(ratio, 1000);
    }
    bool tgt_past_src = false;
    bool src_past_tgt = false;
    int tgt_cycle = firstCycle(tgt_);
    int src_cycle = 0;
    while (tgt_cycle <= tgt_max_cycle) {
      double tgt_cycle_start = tgt_cycle * tgt_period;
      double tgt_time = tgt_cycle_start + tgt_->time();
      double tgt_opp_time = tgt_cycle_start + tgt_opp_time1;
      for (src_cycle = firstCycle(src_);
	   ;
	   src_cycle++) {
	double src_cycle_start = src_cycle * src_period;
	double src_time = src_cycle_start + src_->time();

	// Make sure both setup and hold required are determined.
	if (tgt_past_src && src_past_tgt
	    // Synchronicity achieved.
	    && fuzzyEqual(src_cycle_start, tgt_cycle_start)) {
	  debugPrint(debug, "cycle_acct", 1, " setup = %s, required = %s",
                     time_unit->asString(delay_[setup_index]),
                     time_unit->asString(required_[setup_index]));
	  debugPrint(debug, "cycle_acct", 1, " hold = %s, required = %s",
                     time_unit->asString(delay_[hold_index]),
                     time_unit->asString(required_[hold_index]));
	  debugPrint(debug, "cycle_acct", 1,
                     " converged at src cycles = %d tgt cycles = %d",
                     src_cycle, tgt_cycle);
	  return;
	}

	if (fuzzyGreater(src_cycle_start, tgt_cycle_start + tgt_period)
	    && src_past_tgt)
	  break;
	debugPrint(debug, "cycle_acct", 2, " %s src cycle %d %s + %s = %s",
                   src_->name(),
                   src_cycle,
                   time_unit->asString(src_cycle_start),
                   time_unit->asString(src_->time()),
                   time_unit->asString(src_time));
	debugPrint(debug, "cycle_acct", 2, " %s tgt cycle %d %s + %s = %s",
                   tgt_->name(),
                   tgt_cycle,
                   time_unit->asString(tgt_cycle_start),
                   time_unit->asString(tgt_->time()),
                   time_unit->asString(tgt_time));

	// For setup checks, target has to be AFTER source.
	if (fuzzyGreater(tgt_time, src_time)) {
	  tgt_past_src = true;
	  double delay = tgt_time - src_time;
	  if (fuzzyLess(delay, delay_[setup_index])) {
	    double required = tgt_time - src_cycle_start;
	    setSetupAccting(src_cycle, tgt_cycle, delay, required);
	    debugPrint(debug, "cycle_acct", 2,
                       " setup min delay = %s, required = %s",
                       time_unit->asString(delay_[setup_index]),
                       time_unit->asString(required_[setup_index]));
	  }
	}

	// Data check setup checks are zero cycle.
	if (fuzzyLessEqual(tgt_time, src_time)) {
	  double setup_delay = src_time - tgt_time;
	  if (fuzzyLess(setup_delay, delay_[data_check_setup_index])) {
	    double setup_required = tgt_time - src_cycle_start;
	    setAccting(TimingRole::dataCheckSetup(), src_cycle, tgt_cycle,
		       setup_delay, setup_required);
	    double hold_required = tgt_time - (src_cycle_start + src_period);
	    double hold_delay = (src_period + src_time) - tgt_time;
	    setAccting(TimingRole::dataCheckHold(),
		       src_cycle + 1, tgt_cycle, hold_delay, hold_required);
	  }
	}

	// Latch setup cycle accting for the enable is the data clk edge
	// closest to the disable (opposite) edge.
	if (fuzzyGreater(tgt_opp_time, src_time)) {
	  double delay = tgt_opp_time - src_time;
	  if (fuzzyLess(delay, delay_[latch_setup_index])) {
	    double latch_tgt_time = tgt_time;
	    int latch_tgt_cycle = tgt_cycle;
	    // Enable time is the edge before the disable.
	    if (tgt_time > tgt_opp_time) {
	      latch_tgt_time -= tgt_period;
	      latch_tgt_cycle--;
	    }
	    double required = latch_tgt_time - src_cycle_start;
	    setAccting(TimingRole::latchSetup(),
		       src_cycle, latch_tgt_cycle, delay, required);
	    debugPrint(debug, "cycle_acct", 2,
                       " latch setup min delay = %s, required = %s",
                       time_unit->asString(delay_[latch_setup_index]),
                       time_unit->asString(required_[latch_setup_index]));
	  }
	}

	// For hold checks, target has to be BEFORE source.
	if (fuzzyLessEqual(tgt_time, src_time)) {
	  double delay = src_time - tgt_time;
	  src_past_tgt = true;
	  if (fuzzyLess(delay, delay_[hold_index])) {
	    double required = tgt_time - src_cycle_start;
	    setHoldAccting(src_cycle, tgt_cycle, delay, required);
	    debugPrint(debug, "cycle_acct", 2,
                       " hold min delay = %s, required = %s",
                       time_unit->asString(delay_[hold_index]),
                       time_unit->asString(required_[hold_index]));
	  }
	}

	// Gated clock hold checks are in the same cycle as the
	// setup check.
	if (fuzzyLessEqual(tgt_opp_time, src_time)) {
	  double delay = src_time - tgt_time;
	  if (fuzzyLess(delay, delay_[gclk_hold_index])) {
	    double required = tgt_time - src_cycle_start;
	    setAccting(TimingRole::gatedClockHold(),
		       src_cycle, tgt_cycle, delay, required);
	    debugPrint(debug, "cycle_acct", 2,
                       " gated clk hold min delay = %s, required = %s",
                       time_unit->asString(delay_[gclk_hold_index]),
                       time_unit->asString(required_[gclk_hold_index]));
	  }
	}
      }
      tgt_cycle++;
    }
    max_cycles_exceeded_ = true;
    debugPrint(debug, "cycle_acct", 1,
               " max cycles exceeded after %d src cycles, %d tgt_cycles",
               src_cycle, tgt_cycle);
  }
  else if (tgt_period > 0.0)
    findDefaultArrivalSrcDelays();
}

int
CycleAccting::firstCycle(const ClockEdge *clk_edge) const
{
  if (clk_edge->time() < 0)
    return 1;
  else if (clk_edge->time() < clk_edge->clock()->period())
    return 0;
  else
    return -1;
}

void
CycleAccting::setSetupAccting(int src_cycle,
			      int tgt_cycle,
			      float delay,
			      float req)
{
  setAccting(TimingRole::setup(), src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::outputSetup(), src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::gatedClockSetup(), src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::recovery(), src_cycle, tgt_cycle, delay, req);
}

void
CycleAccting::setHoldAccting(int src_cycle,
			     int tgt_cycle,
			     float delay,
			     float req)
{
  setAccting(TimingRole::hold(), src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::outputHold(), src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::removal(), src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::latchHold(), src_cycle, tgt_cycle, delay, req);
}

void
CycleAccting::setAccting(TimingRole *role,
			 int src_cycle,
			 int tgt_cycle,
			 float delay,
			 float req)
{
  int index = role->index();
  src_cycle_[index] = src_cycle;
  tgt_cycle_[index] = tgt_cycle;
  delay_[index] = delay;
  required_[index] = req;
}

void
CycleAccting::findDefaultArrivalSrcDelays()
{
  const Clock *tgt_clk = tgt_->clock();
  float tgt_time = tgt_->time();
  float tgt_period = tgt_clk->period();
  // Unclocked arrival setup check is in cycle zero.
  if (tgt_time > tgt_period)
    setDefaultSetupAccting(0, 0, tgt_time - tgt_period, tgt_time - tgt_period);
  else if (tgt_time > 0.0)
    setDefaultSetupAccting(0, 0, tgt_time, tgt_time);
  else
    setDefaultSetupAccting(0, 1, tgt_period, tgt_period);
  setDefaultHoldAccting(0, 0, 0.0, tgt_time);
}

void
CycleAccting::setDefaultSetupAccting(int src_cycle, 
				     int tgt_cycle,
				     float delay,
				     float req)
{
  setSetupAccting(src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::latchSetup(), src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::dataCheckSetup(), src_cycle, tgt_cycle, delay, req);
}

void
CycleAccting::setDefaultHoldAccting(int src_cycle, 
				    int tgt_cycle,
				    float delay,
				    float req)
{
  setHoldAccting(src_cycle, tgt_cycle, delay, req);
  setAccting(TimingRole::dataCheckHold(), src_cycle, tgt_cycle, delay, req);
}

float
CycleAccting::requiredTime(const TimingRole *check_role)
{
  return required_[check_role->index()];
}

float
CycleAccting::sourceTimeOffset(const TimingRole *check_role)
{
  return sourceCycle(check_role) * src_->clock()->period();
}

int
CycleAccting::sourceCycle(const TimingRole *check_role)
{
  return src_cycle_[check_role->index()];
}

int
CycleAccting::targetCycle(const TimingRole *check_role)
{
  return tgt_cycle_[check_role->index()];
}

float
CycleAccting::targetTimeOffset(const TimingRole *check_role)
{
  return targetCycle(check_role) * tgt_->clock()->period();
}

////////////////////////////////////////////////////////////////

bool
CycleAcctingLess::operator()(const CycleAccting *acct1,
			     const CycleAccting *acct2) const

{
  int src_index1 = acct1->src()->index();
  int src_index2 = acct2->src()->index();
  return src_index1 < src_index2
    || (src_index1 == src_index2
	&& acct1->target()->index() < acct2->target()->index());
}

size_t 
CycleAcctingHash::operator()(const CycleAccting *acct) const
{
  return hashSum(acct->src()->index(), acct->target()->index());
}

bool
CycleAcctingEqual::operator()(const CycleAccting *acct1,
			      const CycleAccting *acct2) const
{
  return acct1->src() == acct2->src()
    && acct1->target() == acct2->target();
}

} // namespace
