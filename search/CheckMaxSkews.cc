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
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "PathVertex.hh"
#include "PathAnalysisPt.hh"
#include "Search.hh"
#include "CheckMaxSkews.hh"

namespace sta {

// Abstract base class.
class MaxSkewCheckVisitor
{
public:
  MaxSkewCheckVisitor() {}
  virtual ~MaxSkewCheckVisitor() {}
  virtual void visit(MaxSkewCheck &check,
		     const StaState *sta) = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(MaxSkewCheckVisitor);
};

CheckMaxSkews::CheckMaxSkews(StaState *sta) :
  sta_(sta)
{
}

CheckMaxSkews::~CheckMaxSkews()
{
  checks_.deleteContents();
}

void
CheckMaxSkews::clear()
{
  checks_.deleteContentsClear();
}

class MaxSkewChecksVisitor : public MaxSkewCheckVisitor
{
public:
  explicit MaxSkewChecksVisitor(MaxSkewCheckSeq &checks);
  virtual void visit(MaxSkewCheck &check,
		     const StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(MaxSkewChecksVisitor);

  MaxSkewCheckSeq &checks_;
};

MaxSkewChecksVisitor::MaxSkewChecksVisitor(MaxSkewCheckSeq &checks) :
  MaxSkewCheckVisitor(),
  checks_(checks)
{
}

void
MaxSkewChecksVisitor::visit(MaxSkewCheck &check,
			    const StaState *)
{
  checks_.push_back(new MaxSkewCheck(check));
}

class MaxSkewViolatorsVisititor : public MaxSkewCheckVisitor
{
public:
  explicit MaxSkewViolatorsVisititor(MaxSkewCheckSeq &checks);
  virtual void visit(MaxSkewCheck &check,
		     const StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(MaxSkewViolatorsVisititor);
  MaxSkewCheckSeq &checks_;
};

MaxSkewViolatorsVisititor::
MaxSkewViolatorsVisititor(MaxSkewCheckSeq &checks) :
  MaxSkewCheckVisitor(),
  checks_(checks)
{
}

void
MaxSkewViolatorsVisititor::visit(MaxSkewCheck &check,
				 const StaState *sta)
{
  if (fuzzyLess(check.slack(sta), 0.0))
    checks_.push_back(new MaxSkewCheck(check));
}

MaxSkewCheckSeq &
CheckMaxSkews::violations()
{
  clear();
  MaxSkewViolatorsVisititor visitor(checks_);
  visitMaxSkewChecks(&visitor);
  sort(checks_, MaxSkewSlackLess(sta_));
  return checks_;
}

class MaxSkewSlackVisitor : public MaxSkewCheckVisitor
{
public:
  MaxSkewSlackVisitor();
  virtual void visit(MaxSkewCheck &check,
		     const StaState *sta);
  MaxSkewCheck *minSlackCheck();

private:
  DISALLOW_COPY_AND_ASSIGN(MaxSkewSlackVisitor);

  MaxSkewCheck *min_slack_check_;
};

MaxSkewSlackVisitor::MaxSkewSlackVisitor() :
  MaxSkewCheckVisitor(),
  min_slack_check_(nullptr)
{
}

void
MaxSkewSlackVisitor::visit(MaxSkewCheck &check,
			   const StaState *sta)
{
  MaxSkewSlackLess slack_less(sta);
  if (min_slack_check_ == nullptr
      || slack_less(&check, min_slack_check_)) {
    delete min_slack_check_;
    min_slack_check_ = new MaxSkewCheck(check);
  }
}

MaxSkewCheck *
MaxSkewSlackVisitor::minSlackCheck()
{
  return min_slack_check_;
}

MaxSkewCheck *
CheckMaxSkews::minSlackCheck()
{
  clear();
  MaxSkewSlackVisitor visitor;
  visitMaxSkewChecks(&visitor);
  MaxSkewCheck *check = visitor.minSlackCheck();
  // Save check for cleanup.
  checks_.push_back(check);
  return check;
}

void
CheckMaxSkews::visitMaxSkewChecks(MaxSkewCheckVisitor *visitor)
{
  Graph *graph = sta_->graph();
  VertexIterator vertex_iter(graph);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    visitMaxSkewChecks(vertex, visitor);
  }
}

void
CheckMaxSkews:: visitMaxSkewChecks(Vertex *vertex,
				   MaxSkewCheckVisitor *visitor)
{
  Graph *graph = sta_->graph();
  Search *search = sta_->search();
  const MinMax *clk_min_max = MinMax::max();
  VertexInEdgeIterator edge_iter(vertex, graph);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->role() == TimingRole::skew()) {
      Vertex *ref_vertex = edge->from(graph);
      TimingArcSet *arc_set = edge->timingArcSet();
      TimingArcSetArcIterator arc_iter(arc_set);
      while (arc_iter.hasNext()) {
	TimingArc *arc = arc_iter.next();
	RiseFall *clk_rf = arc->fromTrans()->asRiseFall();
	RiseFall *ref_rf = arc->toTrans()->asRiseFall();
	VertexPathIterator clk_path_iter(vertex, clk_rf, clk_min_max, search);
	while (clk_path_iter.hasNext()) {
	  PathVertex *clk_path = clk_path_iter.next();
	  if (clk_path->isClock(search)) {
	    const PathAnalysisPt *clk_ap = clk_path->pathAnalysisPt(sta_);
	    PathAnalysisPt *ref_ap = clk_ap->tgtClkAnalysisPt();
	    VertexPathIterator ref_path_iter(ref_vertex, ref_rf, ref_ap, sta_);
	    while (ref_path_iter.hasNext()) {
	      PathVertex *ref_path = ref_path_iter.next();
	      if (ref_path->isClock(search)) {
		MaxSkewCheck check(clk_path, ref_path, arc, edge);
		visitor->visit(check, sta_);
	      }
	    }
	  }
	}
      }
    }
  }
}

////////////////////////////////////////////////////////////////

MaxSkewCheck::MaxSkewCheck(PathVertex *clk_path,
			   PathVertex *ref_path,
			   TimingArc *check_arc,
			   Edge *check_edge) :
  clk_path_(clk_path),
  ref_path_(ref_path),
  check_arc_(check_arc),
  check_edge_(check_edge)
{
}

Pin *
MaxSkewCheck::clkPin(const StaState *sta) const
{
  return clk_path_.pin(sta);
}

Pin *
MaxSkewCheck::refPin(const StaState *sta) const
{
  return ref_path_.pin(sta);
}

ArcDelay
MaxSkewCheck::maxSkew(const StaState *sta) const
{
  Search *search = sta->search();
  return search->deratedDelay(ref_path_.vertex(sta),
			      check_arc_, check_edge_, false,
			      clk_path_.pathAnalysisPt(sta));
}

Delay
MaxSkewCheck::skew(const StaState *sta) const
{
  return Delay(clk_path_.arrival(sta) - ref_path_.arrival(sta));
}

Slack
MaxSkewCheck::slack(const StaState *sta) const
{
  return maxSkew(sta) - skew(sta);
}

////////////////////////////////////////////////////////////////

MaxSkewSlackLess::MaxSkewSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
MaxSkewSlackLess::operator()(const MaxSkewCheck *check1,
			     const MaxSkewCheck *check2) const
{
  Slack slack1 = check1->slack(sta_);
  Slack slack2 = check2->slack(sta_);
  return slack1 < slack2
    || (fuzzyEqual(slack1, slack2)
	// Break ties based on constrained pin names.
	&& sta_->network()->pinLess(check1->clkPin(sta_),check2->clkPin(sta_)));
}

} // namespace
