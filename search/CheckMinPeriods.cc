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
#include "DisallowCopyAssign.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Clock.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "Search.hh"
#include "CheckMinPeriods.hh"

namespace sta {

// Abstract base class.
class MinPeriodCheckVisitor
{
public:
  MinPeriodCheckVisitor() {}
  virtual ~MinPeriodCheckVisitor() {}
  virtual void visit(MinPeriodCheck &check,
		     StaState *sta) = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(MinPeriodCheckVisitor);
};

CheckMinPeriods::CheckMinPeriods(StaState *sta) :
  sta_(sta)
{
}

CheckMinPeriods::~CheckMinPeriods()
{
  checks_.deleteContents();
}

void
CheckMinPeriods::clear()
{
  checks_.deleteContentsClear();
}

class MinPeriodViolatorsVisitor : public MinPeriodCheckVisitor
{
public:
  MinPeriodViolatorsVisitor(MinPeriodCheckSeq &checks);
  virtual void visit(MinPeriodCheck &check,
		     StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(MinPeriodViolatorsVisitor);

  MinPeriodCheckSeq &checks_;
};

MinPeriodViolatorsVisitor::MinPeriodViolatorsVisitor(MinPeriodCheckSeq &checks):
  checks_(checks)
{
}

void
MinPeriodViolatorsVisitor::visit(MinPeriodCheck &check,
				 StaState *sta)
{
  if (fuzzyLess(check.slack(sta), 0.0))
    checks_.push_back(check.copy());
}

MinPeriodCheckSeq &
CheckMinPeriods::violations()
{
  clear();
  MinPeriodViolatorsVisitor visitor(checks_);
  visitMinPeriodChecks(&visitor);
  sort(checks_, MinPeriodSlackLess(sta_));
  return checks_;
}

void
CheckMinPeriods::visitMinPeriodChecks(MinPeriodCheckVisitor *visitor)
{
  Graph *graph = sta_->graph();
  VertexIterator vertex_iter(graph);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (isClkEnd(vertex, graph))
      visitMinPeriodChecks(vertex, visitor);
  }
}

void
CheckMinPeriods::visitMinPeriodChecks(Vertex *vertex,
				      MinPeriodCheckVisitor *visitor)
{
  Search *search = sta_->search();
  GraphDelayCalc *graph_dcalc = sta_->graphDelayCalc();
  Pin *pin = vertex->pin();
  float min_period;
  bool exists;
  graph_dcalc->minPeriod(pin, min_period, exists);
  if (exists) {
    ClockSet clks;
    search->clocks(vertex, clks);
    ClockSet::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
      MinPeriodCheck check(pin, clk);
      visitor->visit(check, sta_);
    }
  }
}

////////////////////////////////////////////////////////////////

class MinPeriodSlackVisitor : public MinPeriodCheckVisitor
{
public:
  MinPeriodSlackVisitor();
  virtual void visit(MinPeriodCheck &check,
		     StaState *sta);
  MinPeriodCheck *minSlackCheck();

private:
  DISALLOW_COPY_AND_ASSIGN(MinPeriodSlackVisitor);

  MinPeriodCheck *min_slack_check_;
};

MinPeriodSlackVisitor::MinPeriodSlackVisitor() :
  min_slack_check_(nullptr)
{
}

void
MinPeriodSlackVisitor::visit(MinPeriodCheck &check,
			     StaState *sta)
{
  MinPeriodSlackLess slack_less(sta);
  if (min_slack_check_ == nullptr)
    min_slack_check_ = check.copy();
  else if (slack_less(&check, min_slack_check_)) {
    delete min_slack_check_;
    min_slack_check_ = check.copy();
  }
}

MinPeriodCheck *
MinPeriodSlackVisitor::minSlackCheck()
{
  return min_slack_check_;
}

MinPeriodCheck *
CheckMinPeriods::minSlackCheck()
{
  clear();
  MinPeriodSlackVisitor visitor;
  visitMinPeriodChecks(&visitor);
  MinPeriodCheck *check = visitor.minSlackCheck();
  // Save check for cleanup.
  checks_.push_back(check);
  return check;
}

////////////////////////////////////////////////////////////////

MinPeriodCheck::MinPeriodCheck(Pin *pin,
			       Clock *clk) :
  pin_(pin),
  clk_(clk)
{
}

MinPeriodCheck *
MinPeriodCheck::copy()
{
  return new MinPeriodCheck(pin_, clk_);
}

float
MinPeriodCheck::period() const
{
  return clk_->period();
}

float
MinPeriodCheck::minPeriod(const StaState *sta) const
{
  GraphDelayCalc *graph_dcalc = sta->graphDelayCalc();
  float min_period;
  bool exists;
  graph_dcalc->minPeriod(pin_, min_period, exists);
  return min_period;
}

Slack
MinPeriodCheck::slack(const StaState *sta) const
{
  return clk_->period() - minPeriod(sta);
}

////////////////////////////////////////////////////////////////

MinPeriodSlackLess::MinPeriodSlackLess(StaState *sta) :
  sta_(sta)
{
}

bool
MinPeriodSlackLess::operator()(const MinPeriodCheck *check1,
			       const MinPeriodCheck *check2) const
{
  Slack slack1 = check1->slack(sta_);
  Slack slack2 = check2->slack(sta_);
  const Pin *pin1 = check1->pin();
  const Pin *pin2 = check2->pin();
  return fuzzyLess(slack1, slack2)
    // Break ties based on pin and clock names.
    || (fuzzyEqual(slack1, slack2)
	&& (sta_->network()->pinLess(pin1, pin2)
	    || (pin1 == pin2
		&& ClockNameLess()(check1->clk(),
				   check2->clk()))));
}

} // namespace
