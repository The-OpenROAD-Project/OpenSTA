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
#include "Debug.hh"
#include "DisallowCopyAssign.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "Sdc.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "PathVertex.hh"
#include "PathRef.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "PathAnalysisPt.hh"
#include "SearchPred.hh"
#include "PathEnd.hh"
#include "Search.hh"
#include "CheckMinPulseWidths.hh"

namespace sta {

static void
minPulseWidth(const Path *path,
	      const StaState *sta,
	      // Return values.
	      float &min_width,
	      bool &exists);

// Abstract base class.
class MinPulseWidthCheckVisitor
{
public:
  MinPulseWidthCheckVisitor() {}
  virtual ~MinPulseWidthCheckVisitor() {}
  virtual void visit(MinPulseWidthCheck &check,
		     const StaState *sta) = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(MinPulseWidthCheckVisitor);
};

CheckMinPulseWidths::CheckMinPulseWidths(StaState *sta) :
  sta_(sta)
{
}

CheckMinPulseWidths::~CheckMinPulseWidths()
{
  checks_.deleteContents();
}

void
CheckMinPulseWidths::clear()
{
  checks_.deleteContentsClear();
}

////////////////////////////////////////////////////////////////

class MinPulseWidthChecksVisitor : public MinPulseWidthCheckVisitor
{
public:
  explicit MinPulseWidthChecksVisitor(const Corner *corner,
				      MinPulseWidthCheckSeq &checks);
  virtual void visit(MinPulseWidthCheck &check,
		     const StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(MinPulseWidthChecksVisitor);

  const Corner *corner_;
  MinPulseWidthCheckSeq &checks_;
};

MinPulseWidthChecksVisitor::
MinPulseWidthChecksVisitor(const Corner *corner,
			   MinPulseWidthCheckSeq &checks) :
  corner_(corner),
  checks_(checks)
{
}

void
MinPulseWidthChecksVisitor::visit(MinPulseWidthCheck &check,
				  const StaState *sta)
{
  if (corner_ == nullptr
      || check.corner(sta) == corner_) {
    MinPulseWidthCheck *copy = new MinPulseWidthCheck(check.openPath());
    checks_.push_back(copy);
  }
}

MinPulseWidthCheckSeq &
CheckMinPulseWidths::check(const Corner *corner)
{
  clear();
  MinPulseWidthChecksVisitor visitor(corner, checks_);
  visitMinPulseWidthChecks(&visitor);
  sort(checks_, MinPulseWidthSlackLess(sta_));
  return checks_;
}

MinPulseWidthCheckSeq &
CheckMinPulseWidths::check(PinSeq *pins,
			   const Corner *corner)
{
  clear();
  Graph *graph = sta_->graph();
  MinPulseWidthChecksVisitor visitor(corner, checks_);
  PinSeq::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    Vertex *vertex = graph->pinLoadVertex(pin);
    visitMinPulseWidthChecks(vertex, &visitor);
  }
  sort(checks_, MinPulseWidthSlackLess(sta_));
  return checks_;
}

////////////////////////////////////////////////////////////////

class MinPulseWidthViolatorsVisitor : public MinPulseWidthCheckVisitor
{
public:
  explicit MinPulseWidthViolatorsVisitor(const Corner *corner,
					 MinPulseWidthCheckSeq &checks);
  virtual void visit(MinPulseWidthCheck &check,
		     const StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(MinPulseWidthViolatorsVisitor);

  const Corner *corner_;
  MinPulseWidthCheckSeq &checks_;
};

MinPulseWidthViolatorsVisitor::
MinPulseWidthViolatorsVisitor(const Corner *corner,
			      MinPulseWidthCheckSeq &checks) :
  corner_(corner),
  checks_(checks)
{
}

void
MinPulseWidthViolatorsVisitor::visit(MinPulseWidthCheck &check,
				     const StaState *sta)
{
  if (fuzzyLess(check.slack(sta), 0.0)
      && (corner_ == nullptr
	  || check.corner(sta) == corner_)) {
    MinPulseWidthCheck *copy = new MinPulseWidthCheck(check.openPath());
    checks_.push_back(copy);
  }
}

MinPulseWidthCheckSeq &
CheckMinPulseWidths::violations(const Corner *corner)
{
  clear();
  MinPulseWidthViolatorsVisitor visitor(corner, checks_);
  visitMinPulseWidthChecks(&visitor);
  sort(checks_, MinPulseWidthSlackLess(sta_));
  return checks_;
}

////////////////////////////////////////////////////////////////

class MinPulseWidthSlackVisitor : public MinPulseWidthCheckVisitor
{
public:
  MinPulseWidthSlackVisitor(const Corner *corner);
  virtual void visit(MinPulseWidthCheck &check,
		     const StaState *sta);
  MinPulseWidthCheck *minSlackCheck();

private:
  DISALLOW_COPY_AND_ASSIGN(MinPulseWidthSlackVisitor);

  const Corner *corner_;
  MinPulseWidthCheck *min_slack_check_;
};

MinPulseWidthSlackVisitor::MinPulseWidthSlackVisitor(const Corner *corner) :
  corner_(corner),
  min_slack_check_(nullptr)
{
}

void
MinPulseWidthSlackVisitor::visit(MinPulseWidthCheck &check,
				 const StaState *sta)
{
  MinPulseWidthSlackLess slack_less(sta);
  if (corner_ == nullptr
      || check.corner(sta) == corner_) {
    if (min_slack_check_ == nullptr)
      min_slack_check_ = check.copy();
    else if (slack_less(&check, min_slack_check_)) {
      delete min_slack_check_;
      min_slack_check_ = check.copy();
    }
  }
}

MinPulseWidthCheck *
MinPulseWidthSlackVisitor::minSlackCheck()
{
  return min_slack_check_;
}

MinPulseWidthCheck *
CheckMinPulseWidths::minSlackCheck(const Corner *corner)
{
  clear();
  MinPulseWidthSlackVisitor visitor(corner);
  visitMinPulseWidthChecks(&visitor);
  MinPulseWidthCheck *check = visitor.minSlackCheck();
  // Save check for cleanup.
  checks_.push_back(check);
  return check;
}

void
CheckMinPulseWidths::
visitMinPulseWidthChecks(MinPulseWidthCheckVisitor *visitor)
{
  Graph *graph = sta_->graph();
  Debug *debug = sta_->debug();
  Network *sdc_network = sta_->network();
  VertexIterator vertex_iter(graph);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (isClkEnd(vertex, graph)) {
      debugPrint1(debug, "mpw", 1, "check mpw %s\n", vertex->name(sdc_network));
      visitMinPulseWidthChecks(vertex, visitor);
    }
  }
}

void
CheckMinPulseWidths::
visitMinPulseWidthChecks(Vertex *vertex,
			 MinPulseWidthCheckVisitor *visitor)
{
  Search *search = sta_->search();
  const MinMax *min_max = MinMax::max();
  VertexPathIterator path_iter(vertex, search);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    if (path->isClock(search)
	&& !path->tag(sta_)->clkInfo()->isGenClkSrcPath()) {
      if (path->minMax(sta_) == min_max) {
	float min_width;
	bool exists;
	minPulseWidth(path, sta_, min_width, exists);
	if (exists) {
	  MinPulseWidthCheck check(path);
	  PathVertex close_path;
	  check.closePath(sta_, close_path);
	  // Don't bother visiting if nobody is home.
	  if (!close_path.isNull())
	    visitor->visit(check, sta_);
	}
      }
    }
  }
}

////////////////////////////////////////////////////////////////

MinPulseWidthCheck::MinPulseWidthCheck() :
  open_path_()
{
}

MinPulseWidthCheck::MinPulseWidthCheck(Path *open_path) :
  open_path_(open_path)
{
}

MinPulseWidthCheck *
MinPulseWidthCheck::copy()
{
  return new MinPulseWidthCheck(&open_path_);
}

Pin *
MinPulseWidthCheck::pin(const StaState *sta) const
{
  return open_path_.pin(sta);
}

const RiseFall *
MinPulseWidthCheck::openTransition(const StaState *sta) const
{
  return open_path_.transition(sta);
}

void
MinPulseWidthCheck::closePath(const StaState *sta,
			      // Return value.
			      PathVertex &close) const
{
  PathAnalysisPt *open_ap = open_path_.pathAnalysisPt(sta);
  PathAnalysisPt *close_ap = open_ap->tgtClkAnalysisPt();
  const RiseFall *open_rf = open_path_.transition(sta);
  const RiseFall *close_rf = open_rf->opposite();
  Tag *open_tag = open_path_.tag(sta);
  ClkInfo *open_clk_info = open_tag->clkInfo();
  ClkInfo close_clk_info(open_clk_info->clkEdge()->opposite(),
			 open_clk_info->clkSrc(),
			 open_clk_info->isPropagated(),
			 open_clk_info->genClkSrc(),
			 open_clk_info->isGenClkSrcPath(),
			 open_clk_info->pulseClkSense(),
			 delay_zero, 0.0, nullptr,
			 open_clk_info->pathAPIndex(),
			 open_clk_info->crprClkPath(),
			 sta);
  Tag close_tag(0,
		close_rf->index(),
		close_ap->index(),
		&close_clk_info,
		open_tag->isClock(),
		open_tag->inputDelay(),
		open_tag->isSegmentStart(),
		open_tag->states(),
		false, sta);
  debugPrint1(sta->debug(), "mpw", 3, " open  %s\n",
	      open_tag->asString(sta));
  debugPrint1(sta->debug(), "mpw", 3, " close %s\n",
	      close_tag.asString(sta));
  VertexPathIterator close_iter(open_path_.vertex(sta), close_rf,
				close_ap, sta);
  while (close_iter.hasNext()) {
    PathVertex *close_path = close_iter.next();
    if (tagMatchNoPathAp(close_path->tag(sta), &close_tag)) {
      debugPrint1(sta->debug(), "mpw", 3, " match %s\n",
		  close_path->tag(sta)->asString(sta));
      close = close_path;
      break;
    }
  }
}

Arrival
MinPulseWidthCheck::openArrival(const StaState *sta) const
{
  return open_path_.arrival(sta);
}

Arrival
MinPulseWidthCheck::closeArrival(const StaState *sta) const
{
  PathVertex close;
  closePath(sta, close);
  return close.arrival(sta);
}

Arrival
MinPulseWidthCheck::openDelay(const StaState *sta) const
{
  return openArrival(sta) - openClkEdge(sta)->time();
}

Arrival
MinPulseWidthCheck::closeDelay(const StaState *sta) const
{
  return closeArrival(sta) - closeClkEdge(sta)->time();
}

ClockEdge *
MinPulseWidthCheck::openClkEdge(const StaState *sta) const
{
  return open_path_.clkEdge(sta->search());
}

ClockEdge *
MinPulseWidthCheck::closeClkEdge(const StaState *sta) const
{
  Tag *open_tag = open_path_.tag(sta);
  ClkInfo *open_clk_info = open_tag->clkInfo();
  return open_clk_info->clkEdge()->opposite();
}

float
MinPulseWidthCheck::closeOffset(const StaState *sta) const
{
  ClockEdge *open_clk_edge = openClkEdge(sta);
  ClockEdge *close_clk_edge = closeClkEdge(sta);
  if (open_clk_edge->time() > close_clk_edge->time())
    return open_clk_edge->clock()->period();
  else
    return 0.0;
}

Arrival
MinPulseWidthCheck::width(const StaState *sta) const
{
  return closeArrival(sta) + closeOffset(sta)
    - open_path_.arrival(sta)
    + commonClkPessimism(sta);
}

float
MinPulseWidthCheck::minWidth(const StaState *sta) const
{
  float min_width;
  bool exists;
  minPulseWidth(&open_path_, sta, min_width, exists);
  return min_width;
}

// Precedence:
//  set_min_pulse_width constraint
//  SDF annotation
//  Liberty library
static void
minPulseWidth(const Path *path,
	      const StaState *sta,
	      // Return values.
	      float &min_width,
	      bool &exists)
{
  Pin *pin = path->pin(sta);
  Clock *clk = path->clock(sta);
  const RiseFall *rf = path->transition(sta);
  Sdc *sdc = sta->sdc();
  // set_min_pulse_width command.
  sdc->minPulseWidth(pin, clk, rf, min_width, exists);
  if (!exists) {
    GraphDelayCalc *graph_dcalc = sta->graphDelayCalc();
    const MinMax *min_max = path->minMax(sta);
    const PathAnalysisPt *path_ap = path->pathAnalysisPt(sta);
    const DcalcAnalysisPt *dcalc_ap = path_ap->dcalcAnalysisPt();
    graph_dcalc->minPulseWidth(pin, rf, dcalc_ap->index(), min_max,
			       min_width, exists);
  }
}

Crpr
MinPulseWidthCheck::commonClkPessimism(const StaState *sta) const
{
  CheckCrpr *check_crpr = sta->search()->checkCrpr();
  PathVertex close;
  closePath(sta, close);
  if (!close.isNull())
    return check_crpr->checkCrpr(openPath(), &close);
  else
    return 0.0;
}

Slack
MinPulseWidthCheck::slack(const StaState *sta) const
{
  return width(sta) - minWidth(sta);
}

Corner *
MinPulseWidthCheck::corner(const StaState *sta) const
{
  return open_path_.pathAnalysisPt(sta)->corner();
}

////////////////////////////////////////////////////////////////

MinPulseWidthSlackLess::MinPulseWidthSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
MinPulseWidthSlackLess::operator()(const MinPulseWidthCheck *check1,
				   const MinPulseWidthCheck *check2) const
{
  Slack slack1 = check1->slack(sta_);
  Slack slack2 = check2->slack(sta_);
  const Pin *pin1 = check1->pin(sta_);
  const Pin *pin2 = check2->pin(sta_);
  return slack1 < slack2
    || (fuzzyEqual(slack1, slack2)
	// Break ties for the sake of regression stability.
	&& (sta_->network()->pinLess(pin1, pin2)
	    || (pin1 == pin2
		&& check1->openPath()->rfIndex(sta_)
		< check2->openPath()->rfIndex(sta_))));
}

} // namespace
