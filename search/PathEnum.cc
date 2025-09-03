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

#include "PathEnum.hh"

#include "Debug.hh"
#include "Error.hh"
#include "Fuzzy.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "PathAnalysisPt.hh"
#include "Tag.hh"
#include "Search.hh"
#include "Latches.hh"
#include "PathEnd.hh"
#include "Path.hh"

namespace sta {

// A diversion is an alternate path formed by changing the previous 
// path/arc of before_div to after_div/div_arc in path.
//
//             div_arc
// after_div<--------+ 
//                   |
//      <--...--before_div<--...--path<---path_end
class Diversion
{
public:
  Diversion(PathEnd *path_end,
	    Path *after_div);
  PathEnd *pathEnd() const { return path_end_; }
  Path *divPath() const { return after_div_; }

private:
  PathEnd *path_end_;
  Path *after_div_;
};

Diversion::Diversion(PathEnd *path_end,
		     Path *after_div) :
  path_end_(path_end),
  after_div_(after_div)
{
}

////////////////////////////////////////////////////////////////

// Default constructor required for DiversionQueue template.
DiversionGreater::DiversionGreater() :
  sta_(nullptr)
{
}

DiversionGreater::DiversionGreater(const StaState *sta) :
  sta_(sta)
{
}

// It is important to break all ties in this comparison so that no two
// diversions are equal.  Otherwise only one of a set of paths with
// the same delay is kept in the queue.
bool
DiversionGreater::operator()(Diversion *div1,
			     Diversion *div2) const
{
  PathEnd *path_end1 = div1->pathEnd();
  PathEnd *path_end2 = div2->pathEnd();
  return PathEnd::cmp(path_end1, path_end2, sta_) > 0;
}

static void
deleteDiversionPathEnd(Diversion *div)
{
  delete div->pathEnd();
  delete div;
}

////////////////////////////////////////////////////////////////

PathEnum::PathEnum(size_t group_path_count,
		   size_t endpoint_path_count,
		   bool unique_pins,
		   bool cmp_slack,
		   const StaState *sta) :
  StaState(sta),
  cmp_slack_(cmp_slack),
  group_path_count_(group_path_count),
  endpoint_path_count_(endpoint_path_count),
  unique_pins_(unique_pins),
  div_queue_(DiversionGreater(sta)),
  div_count_(0),
  inserts_pruned_(false),
  next_(nullptr)
{
}

void
PathEnum::insert(PathEnd *path_end)
{
  debugPrint(debug_, "path_enum", 1, "insert %s",
             path_end->path()->to_string(this).c_str());
  debugPrint(debug_, "path_enum", 2, "diversion %s %s %s",
             path_end->path()->to_string(this).c_str(),
             cmp_slack_ ? "slack" : "delay",
             delayAsString(cmp_slack_ ? path_end->slack(this) :
                           path_end->dataArrivalTime(this), this));
  Diversion *div = new Diversion(path_end, path_end->path());
  div_queue_.push(div);
  div_count_++;
}

PathEnum::~PathEnum()
{
  while (!div_queue_.empty()) {
    Diversion *div = div_queue_.top();
    deleteDiversionPathEnd(div);
    div_queue_.pop();
  }
  // PathEnd on deck may not have been consumed.
  delete next_;
}

bool
PathEnum::hasNext()
{
  if (unique_pins_
      && !inserts_pruned_) {
    pruneDiversionQueue();
    inserts_pruned_ = true;
  }
  if (next_ == nullptr
      && !div_queue_.empty())
    findNext();
  return next_ != nullptr;
}

PathEnd *
PathEnum::next()
{
  PathEnd *next = next_;
  findNext();
  return next;
}

void
PathEnum::findNext()
{
  next_ = nullptr;
  // Pop the next slowest path off the queue.
  while (!div_queue_.empty()) {
    Diversion *div = div_queue_.top();
    div_queue_.pop();
    PathEnd *path_end = div->pathEnd();
    Vertex *vertex = path_end->vertex(this);
    path_counts_[vertex]++;
    if (debug_->check("path_enum", 2)) {
      Path *path = path_end->path();
      report_->reportLine("path_enum: next path %zu %s delay %s slack %s",
                          path_counts_[vertex],
                          path->to_string(this).c_str(),
                          delayAsString(path_end->dataArrivalTime(this), this),
                          delayAsString(path_end->slack(this), this));
      reportDiversionPath(div);
    }

    if (path_counts_[vertex] <= endpoint_path_count_) {
      // Add diversions for all arcs converging on the path up to the
      // diversion.
      makeDiversions(path_end, div->divPath());
      // Caller owns the path end now, so don't delete it.
      next_ = path_end;
      delete div;
      break;
    }
    else {
      // We have endpoint_path_count paths for this endpoint,
      // so we are done with it.
      debugPrint(debug_, "path_enum", 1,
		 "endpoint_path_count reached for %s",
                 vertex->to_string(this).c_str());
      deleteDiversionPathEnd(div);
    }
  }
}

void
PathEnum::reportDiversionPath(Diversion *div)
{
  PathEnd *path_end = div->pathEnd();
  Path *path = path_end->path();
  Path *p = path_end->path();
  Path *after_div = div->divPath();
  while (p) {
    report_->reportLine("path_enum:  %s %s%s",
                        p->to_string(this).c_str(),
                        delayAsString(p->arrival(), this),
                        Path::equal(p, after_div, this) ? " <-after diversion" : "");
    if (p != path
        && network_->isLatchData(p->pin(this)))
      break;
    p = p->prevPath();
  }
}

////////////////////////////////////////////////////////////////

typedef std::set<std::pair<const Vertex*, const TimingArc*>> VisitedFanins;

class PathEnumFaninVisitor : public PathVisitor
{
public:
  PathEnumFaninVisitor(PathEnd *path_end,
		       Path *before_div,
		       bool unique_pins,
		       PathEnum *path_enum);
  virtual VertexVisitor *copy() const override;
  void visitFaninPathsThru(Path *before_div,
			   Vertex *prev_vertex,
			   TimingArc *prev_arc);
  virtual bool visitFromToPath(const Pin *from_pin,
			       Vertex *from_vertex,
			       const RiseFall *from_rf,
			       Tag *from_tag,
			       Path *from_path,
                               const Arrival &from_arrival,
			       Edge *edge,
			       TimingArc *arc,
			       ArcDelay arc_delay,
			       Vertex *to_vertex,
			       const RiseFall *to_rf,
			       Tag *to_tag,
			       Arrival &to_arrival,
			       const MinMax *min_max,
			       const PathAnalysisPt *path_ap) override;

private:
  void makeDivertedPathEnd(Path *after_div,
                           Edge *div_edge,
			   TimingArc *div_arc,
			   // Return values.
			   PathEnd *&div_end,
			   Path *&after_div_copy);
  bool visitEdge(const Pin *from_pin,
                 Vertex *from_vertex,
                 Edge *edge,
                 const Pin *to_pin,
                 Vertex *to_vertex) override;
  virtual void visit(Vertex *) override {}  // Not used.
  void reportDiversion(const Edge *edge,
                       const TimingArc *div_arc,
		       Path *after_div);

  PathEnd *path_end_;
  Path *before_div_;
  bool unique_pins_;
  PathEnum *path_enum_;

  Slack path_end_slack_;
  Tag *before_div_tag_;
  int before_div_rf_index_;
  PathAPIndex before_div_ap_index_;
  Arrival before_div_arrival_;
  TimingArc *prev_arc_;
  Vertex *prev_vertex_;
  bool crpr_active_;
  VisitedFanins visited_fanins_;
};

PathEnumFaninVisitor::PathEnumFaninVisitor(PathEnd *path_end,
					   Path *before_div,
					   bool unique_pins,
					   PathEnum *path_enum) :
  PathVisitor(path_enum),
  path_end_(path_end),
  before_div_(before_div),
  unique_pins_(unique_pins),
  path_enum_(path_enum),

  path_end_slack_(path_end->slack(this)),
  before_div_tag_(before_div_->tag(this)),
  before_div_rf_index_(before_div_tag_->rfIndex()),
  before_div_ap_index_(before_div_tag_->pathAPIndex()),
  before_div_arrival_(before_div_->arrival()),
  crpr_active_(crprActive())
{
}

void
PathEnumFaninVisitor::visitFaninPathsThru(Path *before_div,
					  Vertex *prev_vertex,
					  TimingArc *prev_arc)
{
  before_div_ = before_div;
  before_div_tag_ = before_div_->tag(this);
  before_div_arrival_ = before_div_->arrival();
  before_div_rf_index_ = before_div_tag_->rfIndex();
  before_div_ap_index_ = before_div_tag_->pathAPIndex();

  prev_arc_ = prev_arc;
  prev_vertex_ = prev_vertex;
  visited_fanins_.clear();
  visitFaninPaths(before_div_->vertex(this));
}

// Specialize PathVisitor::visitEdge to filter paths/arcs to
// reduce tag mutations.
bool
PathEnumFaninVisitor::visitEdge(const Pin *from_pin,
                                Vertex *from_vertex,
                                Edge *edge,
                                const Pin *to_pin,
                                Vertex *to_vertex)
{
  TagGroup *from_tag_group = search_->tagGroup(from_vertex);
  if (from_tag_group) {
    TimingArcSet *arc_set = edge->timingArcSet();
    VertexPathIterator from_iter(from_vertex, search_);
    while (from_iter.hasNext()) {
      Path *from_path = from_iter.next();
      PathAnalysisPt *path_ap = from_path->pathAnalysisPt(this);
      if (path_ap->index() == before_div_ap_index_) {
        const MinMax *min_max = path_ap->pathMinMax();
        const RiseFall *from_rf = from_path->transition(this);
        TimingArc *arc1, *arc2;
        arc_set->arcsFrom(from_rf, arc1, arc2);
        if (arc1 && arc1->toEdge()->asRiseFall()->index() == before_div_rf_index_) {
          if (!visitArc(from_pin, from_vertex, from_rf, from_path,
                        edge, arc1, to_pin, to_vertex,
                        min_max, path_ap))
            return false;
        }
        if (arc2 && arc2->toEdge()->asRiseFall()->index() == before_div_rf_index_) {
          if (!visitArc(from_pin, from_vertex, from_rf, from_path,
                        edge, arc2, to_pin, to_vertex,
                        min_max, path_ap))
            return false;
        }
      }
    }
  }
  return true;
}

VertexVisitor *
PathEnumFaninVisitor::copy() const
{
  return new PathEnumFaninVisitor(path_end_, before_div_, unique_pins_,
				  path_enum_);
}

bool
PathEnumFaninVisitor::visitFromToPath(const Pin *,
				      Vertex *from_vertex,
				      const RiseFall *,
				      Tag *,
				      Path *from_path,
                                      const Arrival &,
				      Edge *edge,
				      TimingArc *arc,
				      ArcDelay,
				      Vertex *to_vertex,
				      const RiseFall *to_rf,
				      Tag *to_tag,
				      Arrival & /* to_arrival */,
				      const MinMax * /* min_max */,
				      const PathAnalysisPt *path_ap)
{
  // These paths fanin to before_div_ so we know to_vertex matches.
  if ((!unique_pins_ || from_vertex != prev_vertex_)
      && arc != prev_arc_
      && tagMatchNoCrpr(to_tag, before_div_tag_)) {
    debugPrint(debug_, "path_enum", 3, "visit fanin %s -> %s %s %s",
               from_path->to_string(this).c_str(),
               to_vertex->to_string(this).c_str(),
               to_rf->to_string().c_str(),
               delayAsString(search_->deratedDelay(from_vertex, arc, edge,
                                                   false,path_ap), this));
    if (crpr_active_) {
      // Ingore paths that only differ by crpr from same vertex/edge.
      if (visited_fanins_.find({from_vertex, arc}) == visited_fanins_.end()) {
        PathEnd *div_end;
        Path *after_div_copy;
        // Make the diverted path end to check slack with from_path crpr.
        makeDivertedPathEnd(from_path, edge, arc, div_end, after_div_copy);
        if (div_end) {
          reportDiversion(edge, arc, from_path);
          path_enum_->makeDiversion(div_end, after_div_copy);
          visited_fanins_.emplace(from_vertex, arc);
        }
      }
      else
        debugPrint(debug_, "path_enum", 3, "      pruned %s %s",
                   edge->to_string(this).c_str(),
                   arc->to_string().c_str());
    }
    else {
      PathEnd *div_end;
      Path *after_div_copy;
      makeDivertedPathEnd(from_path, edge, arc, div_end, after_div_copy);
      reportDiversion(edge, arc, from_path);
      path_enum_->makeDiversion(div_end, after_div_copy);
    }
  }
  return true;
}

void
PathEnumFaninVisitor::makeDivertedPathEnd(Path *after_div,
                                          Edge *div_edge,
					  TimingArc *div_arc,
					  // Return values.
					  PathEnd *&div_end,
					  Path *&after_div_copy)
{
  Path *div_path;
  path_enum_->makeDivertedPath(path_end_->path(), before_div_, after_div,
			       div_edge, div_arc, div_path, after_div_copy);
  if (after_div_copy) {
    div_end = path_end_->copy();
    div_end->setPath(div_path);
  }
  else
    div_end = nullptr;
}

void
PathEnumFaninVisitor::reportDiversion(const Edge *div_edge,
                                      const TimingArc *div_arc,
				      Path *after_div)
{			
  if (debug_->check("path_enum", 3)) {
    Path *path = path_end_->path();
    const PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
    Arrival path_delay = path_enum_->cmp_slack_
      ? path_end_->slack(this)
      : path_end_->dataArrivalTime(this);
    Arrival div_delay = path_delay - path_enum_->divSlack(before_div_,
							  after_div, div_edge,
                                                          div_arc, path_ap);
    Path *div_prev = before_div_->prevPath();
    report_->reportLine("path_enum: diversion %s %s %s -> %s",
                        path->to_string(this).c_str(),
                        path_enum_->cmp_slack_ ? "slack" : "delay",
                        delayAsString(path_delay, this),
                        delayAsString(div_delay, this));
    report_->reportLine("path_enum:  from %s -> %s",
                        div_prev->to_string(this).c_str(),
                        before_div_->to_string(this).c_str());
    report_->reportLine("path_enum:    to %s",
                        after_div->to_string(this).c_str());
  }
}

// A diversion is an alternate path formed by changing the previous 
// path/arc of before_div to after_div/div_arc in path.
//
//             div_arc
// after_div<--------+ 
//                   |
//      <--...--before_div<--...--path<---path_end
void
PathEnum::makeDiversion(PathEnd *div_end,
			Path *after_div_copy)
{
  Diversion *div = new Diversion(div_end, after_div_copy);
  div_queue_.push(div);
  div_count_++;

  if (div_queue_.size() > group_path_count_ * 2)
    // We have more potenial paths than we will need.
    pruneDiversionQueue();
}

void
PathEnum::pruneDiversionQueue()
{
  debugPrint(debug_, "path_enum", 2, "prune queue");
  VertexPathCountMap path_counts;
  size_t end_count = 0;
  // Collect endpoint_path_count diversions per vertex.
  DiversionSeq divs;
  while (!div_queue_.empty()) {
    Diversion *div = div_queue_.top();
    Vertex *vertex = div->pathEnd()->vertex(this);
    if (end_count < group_path_count_
	&& ((unique_pins_ && path_counts[vertex] == 0)
	    || (!unique_pins_ && path_counts[vertex] < endpoint_path_count_))) {
      divs.push_back(div);
      path_counts[vertex]++;
      end_count++;
    }
    else
      deleteDiversionPathEnd(div);
    div_queue_.pop();
  }

  // Add the top diversions back.
  for (Diversion *div : divs)
    div_queue_.push(div);
}

Arrival
PathEnum::divSlack(Path *before_div,
		   Path *after_div,
                   const Edge *div_edge,
                   const TimingArc *div_arc,
		   const PathAnalysisPt *path_ap)
{
  Arrival before_div_arrival = before_div->arrival();
  if (div_edge) {
    if (div_edge->role()->isLatchDtoQ()) {
      Arrival div_arrival;
      ArcDelay div_delay;
      Tag *q_tag;
      latches_->latchOutArrival(after_div, div_arc, div_edge, path_ap,
				q_tag, div_delay, div_arrival);
      return div_arrival - before_div_arrival;
    }
    else {
      ArcDelay div_delay = search_->deratedDelay(div_edge->from(graph_),
						 div_arc, div_edge,
						 false, path_ap);
      Arrival div_arrival = search_->clkPathArrival(after_div) + div_delay;
      return div_arrival - before_div_arrival;
    }
  }
  else {
    report()->error(1370, "path diversion missing edge.");
    return 0.0;
  }
}

// Make diversions for all arcs that merge into path for paths
// starting at "before" to the beginning of the path.
void
PathEnum::makeDiversions(PathEnd *path_end,
			 Path *before)
{
  Path *path = before;
  Path *prev_path = path->prevPath();
  TimingArc *prev_arc = path->prevArc(this);
  PathEnumFaninVisitor fanin_visitor(path_end, path, unique_pins_, this);
  while (prev_path) {
    // Fanin visitor does all the work.
    // While visiting the fanins the fanin_visitor finds the
    // previous path and arc as well as diversions.
    fanin_visitor.visitFaninPathsThru(path, prev_path->vertex(this), prev_arc);
    // Do not enumerate beyond latch D to Q edges.
    // This breaks latch loop paths.
    const TimingRole *prev_role = prev_arc->role();
    if (prev_role == TimingRole::latchDtoQ()
        || prev_role == TimingRole::regClkToQ())
      break;
    path = prev_path;
    prev_path = path->prevPath();
    prev_arc = path->prevArc(this);
  }
}

void
PathEnum::makeDivertedPath(Path *path,
			   Path *before_div,
			   Path *after_div,
                           Edge *div_edge,
			   TimingArc *div_arc,
			   // Returned values.
			   Path *&div_path,
			   Path *&after_div_copy)
{
  div_path = nullptr;
  after_div_copy = nullptr;
  // Copy the diversion path.
  bool found_div = false;
  PathSeq copies;
  Path *p = path;
  bool first = true;
  Path *prev_copy = nullptr;
  while (p) {
    // prev_path made in next pass.
    Path *copy = new Path(p->vertex(this),
                          p->tag(this),
                          p->arrival(),
                          // Replaced on next pass.
                          p->prevPath(),
                          p->prevEdge(this),
                          p->prevArc(this),
                          true, this);
    if (prev_copy)
      prev_copy->setPrevPath(copy);
    copies.push_back(copy);

    if (p == after_div)
      after_div_copy = copy;
    if (first)
      div_path = copy;
    else if (network_->isLatchData(p->pin(this)))
      break;
    if (p == before_div) {
      // Replaced on next pass.
      copy->setPrevPath(after_div);
      copy->setPrevEdgeArc(div_edge, div_arc, this);
      // Update the delays forward from before_div to the end of the path.
      updatePathHeadDelays(copies, after_div);
      p = after_div;
      found_div = true;
    }
    else
      p = p->prevPath();

    prev_copy = copy;
    first = false;
  }
  if (!found_div)
    criticalError(280, "diversion path not found");
}

void
PathEnum::updatePathHeadDelays(PathSeq &paths,
			       Path *after_div)
{
  Tag *prev_tag = after_div->tag(this);
  ClkInfo *prev_clk_info = prev_tag->clkInfo();
  Arrival prev_arrival = search_->clkPathArrival(after_div);
  int path_idx_max = paths.size() - 1;
  // paths[0] is the path endpoint
  for (int i = path_idx_max; i >= 0; i--) {
    Path *path = paths[i];
    TimingArc *arc = path->prevArc(this);
    Edge *edge = path->prevEdge(this);
    if (edge) {
      Arrival arrival;
      PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
      const MinMax *min_max = path->minMax(this);
      if (i == path_idx_max
	  && edge->role()->isLatchDtoQ()
	  && min_max == MinMax::max()) {
	ArcDelay arc_delay;
	Tag *q_tag;
	latches_->latchOutArrival(after_div, arc, edge, path_ap,
				  q_tag, arc_delay, arrival);
	path->setArrival(arrival);
	path->setTag(q_tag);
	prev_clk_info = q_tag->clkInfo();
      }
      else {
	ArcDelay arc_delay = search_->deratedDelay(edge->from(graph_),
						   arc, edge, false, path_ap);
	arrival = prev_arrival + arc_delay;
	path->setArrival(arrival);
	const Tag *tag = path->tag(this);
	const ClkInfo *clk_info = tag->clkInfo();
	if (crprActive()
	    && clk_info != prev_clk_info
	    // D->Q paths use the EN->Q clk info so no need to update.
	    && arc->role() != TimingRole::latchDtoQ()) {
	  // When crpr is enabled the diverion may be from another crpr clk pin,
	  // so update the tags to use the corresponding ClkInfo.
	  Tag *updated_tag = search_->findTag(path->transition(this),
					      path_ap,
					      prev_clk_info,
					      tag->isClock(),
					      tag->inputDelay(),
					      tag->isSegmentStart(),
					      tag->states(), false);
	  path->setTag(updated_tag);
	}
	debugPrint(debug_, "path_enum", 5, "update arrival %s %s %s -> %s",
		   path->vertex(this)->to_string(this).c_str(),
		   path->tag(this)->to_string(this).c_str(),
		   delayAsString(path->arrival(), this),
		   delayAsString(arrival, this));
      }
      prev_arrival = arrival;
    }
  }
}

} // namespace
