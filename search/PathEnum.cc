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
#include "PathEnd.hh"
#include "PathRef.hh"
#include "PathEnumed.hh"

namespace sta {

////////////////////////////////////////////////////////////////

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

PathEnum::PathEnum(int group_count,
		   int endpoint_count,
		   bool unique_pins,
		   bool cmp_slack,
		   const StaState *sta) :
  StaState(sta),
  cmp_slack_(cmp_slack),
  group_count_(group_count),
  endpoint_count_(endpoint_count),
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
             path_end->path()->name(this));
  debugPrint(debug_, "path_enum", 2, "diversion %s %s %s",
             path_end->path()->name(this),
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
    if (debug_->check("path_enum", 2)) {
      Path *path = path_end->path();
      report_->reportLine("path_enum: next path %s delay %s slack %s",
                          path->name(this),
                          delayAsString(path_end->dataArrivalTime(this), this),
                          delayAsString(path_end->slack(this), this));
      reportDiversionPath(div);
    }

    path_counts_[vertex]++;
    if (path_counts_[vertex] <= endpoint_count_) {
      // Add diversions for all arcs converging on the path up to the
      // diversion.
      makeDiversions(path_end, div->divPath());
      // Caller owns the path end now, so don't delete it.
      next_ = path_end;
      delete div;
      break;
    }
    else {
      // We have endpoint_count paths for this endpoint, so we are done with it.
      debugPrint(debug_, "path_enum", 1, "endpoint_count reached for %s",
                 vertex->name(sdc_network_));
      deleteDiversionPathEnd(div);
    }
  }
}

void
PathEnum::reportDiversionPath(Diversion *div)
{
  PathEnd *path_end = div->pathEnd();
  Path *path = path_end->path();
  PathRef p;
  path->prevPath(this, p);
  Path *after_div = div->divPath();
  while (!p.isNull()) {
    report_->reportLine("path_enum:  %s %s%s",
                        p.name(this),
                        delayAsString(p.arrival(this), this),
                        Path::equal(&p, after_div, this) ? " <-diversion" : "");
    if (network_->isLatchData(p.pin(this)))
      break;
    p.prevPath(this, p);
  }
}

////////////////////////////////////////////////////////////////

class PathEnumFaninVisitor : public PathVisitor
{
public:
  PathEnumFaninVisitor(PathEnd *path_end,
		       PathRef &before_div,
		       bool unique_pins,
		       PathEnum *path_enum);
  virtual VertexVisitor *copy() const;
  virtual void visit(Vertex *) {}  // Not used.
  void visitFaninPathsThru(Vertex *vertex,
			   Vertex *prev_vertex,
			   TimingArc *prev_arc);
  virtual bool visitFromToPath(const Pin *from_pin,
			       Vertex *from_vertex,
			       const RiseFall *from_rf,
			       Tag *from_tag,
			       PathVertex *from_path,
                               const Arrival &from_arrival,
			       Edge *edge,
			       TimingArc *arc,
			       ArcDelay arc_delay,
			       Vertex *to_vertex,
			       const RiseFall *to_rf,
			       Tag *to_tag,
			       Arrival &to_arrival,
			       const MinMax *min_max,
			       const PathAnalysisPt *path_ap);

private:
  void makeDivertedPathEnd(Path *after_div,
			   TimingArc *div_arc,
			   // Return values.
			   PathEnd *&div_end,
			   PathEnumed *&after_div_copy);
  void reportDiversion(TimingArc *div_arc,
		       Path *after_div);

  PathEnd *path_end_;
  Slack path_end_slack_;
  PathRef &before_div_;
  bool unique_pins_;
  int before_div_rf_index_;
  Tag *before_div_tag_;
  PathAPIndex before_div_ap_index_;
  Arrival before_div_arrival_;
  TimingArc *prev_arc_;
  Vertex *prev_vertex_;
  PathEnum *path_enum_;
  bool crpr_active_;
};

PathEnumFaninVisitor::PathEnumFaninVisitor(PathEnd *path_end,
					   PathRef &before_div,
					   bool unique_pins,
					   PathEnum *path_enum) :
  PathVisitor(path_enum),
  path_end_(path_end),
  path_end_slack_(path_end->slack(this)),
  before_div_(before_div),
  unique_pins_(unique_pins),
  before_div_rf_index_(before_div_.rfIndex(this)),
  before_div_tag_(before_div_.tag(this)),
  before_div_ap_index_(before_div_.pathAnalysisPtIndex(this)),
  before_div_arrival_(before_div_.arrival(this)),
  path_enum_(path_enum),
  crpr_active_(sdc_->crprActive())
{
}

void
PathEnumFaninVisitor::visitFaninPathsThru(Vertex *vertex,
					  Vertex *prev_vertex,
					  TimingArc *prev_arc)
{
  before_div_rf_index_ = before_div_.rfIndex(this);
  before_div_tag_ = before_div_.tag(this);
  before_div_ap_index_ = before_div_.pathAnalysisPtIndex(this);
  before_div_arrival_ = before_div_.arrival(this);
  prev_arc_ = prev_arc;
  prev_vertex_ = prev_vertex;
  visitFaninPaths(vertex);
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
				      PathVertex *from_path,
                                      const Arrival &,
				      Edge *edge,
				      TimingArc *arc,
				      ArcDelay,
				      Vertex *to_vertex,
				      const RiseFall *to_rf,
				      Tag *to_tag,
				      Arrival &to_arrival,
				      const MinMax *min_max,
				      const PathAnalysisPt *path_ap)
{
  debugPrint(debug_, "path_enum", 3, "visit fanin %s -> %s %s %s",
             from_path->name(this),
             to_vertex->name(network_),
             to_rf->asString(),
             delayAsString(search_->deratedDelay(from_vertex, arc, edge,
                                                 false,path_ap), this));
  // These paths fanin to before_div_ so we know to_vertex matches.
  if (to_rf->index() == before_div_rf_index_
      && path_ap->index() == before_div_ap_index_
      && arc != prev_arc_
      && (!unique_pins_ || from_vertex != prev_vertex_)
      && tagMatchNoCrpr(to_tag, before_div_tag_)) {
    if (crpr_active_) {
      PathEnd *div_end;
      PathEnumed *after_div_copy;
      // Make the diverted path end to check slack with from_path crpr.
      makeDivertedPathEnd(from_path, arc, div_end, after_div_copy);
      // Only enumerate paths with greater slack.
      if (delayGreaterEqual(div_end->slack(this), path_end_slack_, this)) {
	reportDiversion(arc, from_path);
	path_enum_->makeDiversion(div_end, after_div_copy);
      }
      else
	delete div_end;
    }
    // Only enumerate slower/faster paths.
    else if (delayLessEqual(to_arrival, before_div_arrival_, min_max, this)) {
      PathEnd *div_end;
      PathEnumed *after_div_copy;
      makeDivertedPathEnd(from_path, arc, div_end, after_div_copy);
      reportDiversion(arc, from_path);
      path_enum_->makeDiversion(div_end, after_div_copy);
    }
  }
  return true;
}

void
PathEnumFaninVisitor::makeDivertedPathEnd(Path *after_div,
					  TimingArc *div_arc,
					  // Return values.
					  PathEnd *&div_end,
					  PathEnumed *&after_div_copy)
{
  PathEnumed *div_path;
  path_enum_->makeDivertedPath(path_end_->path(), &before_div_, after_div,
			       div_arc, div_path, after_div_copy);
  div_end = path_end_->copy();
  div_end->setPath(div_path, this);
}

void
PathEnumFaninVisitor::reportDiversion(TimingArc *div_arc,
				      Path *after_div)
{			
  if (debug_->check("path_enum", 3)) {
    Path *path = path_end_->path();
    const PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
    Arrival path_delay = path_enum_->cmp_slack_
      ? path_end_->slack(this)
      : path_end_->dataArrivalTime(this);
    Arrival div_delay = path_delay - path_enum_->divSlack(&before_div_,
							  after_div,
							  div_arc, path_ap);
    PathRef div_prev;
    before_div_.prevPath(this, div_prev);
    report_->reportLine("path_enum: diversion %s %s %s -> %s",
                        path->name(this),
                        path_enum_->cmp_slack_ ? "slack" : "delay",
                        delayAsString(path_delay, this),
                        delayAsString(div_delay, this));
    report_->reportLine("path_enum:  from %s -> %s",
                        div_prev.name(this),
                        before_div_.name(this));
    report_->reportLine("path_enum:    to %s",
                        after_div->name(this));
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
			PathEnumed *after_div_copy)
{
  Diversion *div = new Diversion(div_end, after_div_copy);
  div_queue_.push(div);
  div_count_++;

  if (static_cast<int>(div_queue_.size()) > group_count_ * 2)
    // We have more potenial paths than we will need.
    pruneDiversionQueue();
}

void
PathEnum::pruneDiversionQueue()
{
  debugPrint(debug_, "path_enum", 2, "prune queue");
  VertexPathCountMap path_counts;
  int end_count = 0;
  // Collect endpoint_count diversions per vertex.
  DiversionSeq divs;
  while (!div_queue_.empty()) {
    Diversion *div = div_queue_.top();
    Vertex *vertex = div->pathEnd()->vertex(this);
    if (end_count < group_count_
	&& ((unique_pins_ && path_counts[vertex] == 0)
	    || (!unique_pins_ && path_counts[vertex] < endpoint_count_))) {
      divs.push_back(div);
      path_counts[vertex]++;
      end_count++;
    }
    else
      deleteDiversionPathEnd(div);
    div_queue_.pop();
  }

  // Add the top diversions back.
  DiversionSeq::Iterator div_iter(divs);
  while (div_iter.hasNext()) {
    Diversion *div = div_iter.next();
    div_queue_.push(div);
  }
}

Arrival
PathEnum::divSlack(Path *before_div,
		   Path *after_div,
		   TimingArc *div_arc,
		   const PathAnalysisPt *path_ap)
{
  Arrival arc_arrival = before_div->arrival(this);
  Edge *div_edge = divEdge(before_div, div_arc);
  if (div_edge) {
    ArcDelay div_delay = search_->deratedDelay(div_edge->from(graph_),
                                               div_arc, div_edge,
                                               false, path_ap);
    Arrival div_arrival = search_->clkPathArrival(after_div) + div_delay;
    return div_arrival - arc_arrival;
  }
  else {
    report()->error(1370, "path diversion missing edge.");
    return 0.0;
  }
}

Edge *
PathEnum::divEdge(Path *before_div,
		  TimingArc *div_arc)
{
  TimingArcSet *arc_set = div_arc->set();
  VertexInEdgeIterator edge_iter(before_div->vertex(this), graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->timingArcSet() == arc_set)
      return edge;
  }
  return nullptr;
}

// Make diversions for all arcs that merge into path for paths
// starting at "before" to the beginning of the path.
void
PathEnum::makeDiversions(PathEnd *path_end,
			 Path *before)
{
  PathRef path(before);
  PathRef prev_path;
  TimingArc *prev_arc;
  path.prevPath(this, prev_path, prev_arc);
  PathEnumFaninVisitor fanin_visitor(path_end, path, unique_pins_, this);
  while (prev_arc
         // Do not enumerate paths in the clk network.
         && !path.isClock(this)) {
    // Fanin visitor does all the work.
    // While visiting the fanins the fanin_visitor finds the
    // previous path and arc as well as diversions.
    fanin_visitor.visitFaninPathsThru(path.vertex(this),
                                      prev_path.vertex(this), prev_arc);
    // Do not enumerate beyond latch D to Q edges.
    // This breaks latch loop paths.
    if (prev_arc->role() == TimingRole::latchDtoQ())
      break;
    path.init(prev_path);
    path.prevPath(this, prev_path, prev_arc);
  }
}

void
PathEnum::makeDivertedPath(Path *path,
			   Path *before_div,
			   Path *after_div,
			   TimingArc *div_arc,
			   // Returned values.
			   PathEnumed *&div_path,
			   PathEnumed *&after_div_copy)
{
  // Copy the diversion path.
  bool found_div = false;
  PathEnumedSeq copies;
  PathRef p(path);
  bool first = true;
  PathEnumed *prev_copy = nullptr;
  VertexSet visited(graph_);
  while (!p.isNull()
         // Break latch loops.
         && !visited.hasKey(p.vertex(this))) {
    visited.insert(p.vertex(this));
    PathRef prev;
    TimingArc *prev_arc;
    p.prevPath(this, prev, prev_arc);
    PathEnumed *copy = new PathEnumed(p.vertexId(this),
				      p.tagIndex(this),
				      p.arrival(this),
				      nullptr,  // prev_path made in next pass.
				      prev_arc);
    if (prev_copy)
      prev_copy->setPrevPath(copy);
    copies.push_back(copy);

    if (first)
      div_path = copy;
    if (Path::equal(&p, after_div, this))
      after_div_copy = copy;
    if (Path::equal(&p, before_div, this)) {
      copy->setPrevArc(div_arc);
      // Update the delays forward from before_div to the end of the path.
      updatePathHeadDelays(copies, after_div);
      p.init(after_div);
      found_div = true;
    }
    else
      p.init(prev);
    prev_copy = copy;
    first = false;
  }
  if (!found_div)
    criticalError(280, "diversion path not found");
}

void
PathEnum::updatePathHeadDelays(PathEnumedSeq &paths,
			       Path *after_div)
{
  Tag *prev_tag = after_div->tag(this);
  ClkInfo *prev_clk_info = prev_tag->clkInfo();
  Arrival prev_arrival = search_->clkPathArrival(after_div);
  for (int i = paths.size() - 1; i >= 0; i--) {
    PathEnumed *path = paths[i];
    TimingArc *arc = path->prevArc(this);
    Edge *edge = path->prevEdge(arc, this);
    if (edge) {
      PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
      ArcDelay arc_delay = search_->deratedDelay(edge->from(graph_),
                                                 arc, edge, false, path_ap);
      Arrival arrival = prev_arrival + arc_delay;
      debugPrint(debug_, "path_enum", 3, "update arrival %s %s %s -> %s",
                 path->vertex(this)->name(network_),
                 path->tag(this)->asString(this),
                 delayAsString(path->arrival(this), this),
                 delayAsString(arrival, this));
      path->setArrival(arrival, this);
      prev_arrival = arrival;
      if (sdc_->crprActive()
          // D->Q paths use the EN->Q clk info so no need to update.
          && arc->role() != TimingRole::latchDtoQ()) {
        // When crpr is enabled the diverion may be from another crpr clk pin,
        // so update the tags to use the corresponding ClkInfo.
        Tag *tag = path->tag(this);
        Tag *updated_tag = search_->findTag(path->transition(this),
                                            path_ap,
                                            prev_clk_info,
                                            tag->isClock(),
                                            tag->inputDelay(),
                                            tag->isSegmentStart(),
                                            tag->states(), false);
        path->setTag(updated_tag);
      }
    }
  }
}

} // namespace
