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

#include <cmath>
#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "Fuzzy.hh"
#include "Graph.hh"
#include "ExceptionPath.hh"
#include "Sdc.hh"
#include "Corner.hh"
#include "GraphDelayCalc.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "PathAnalysisPt.hh"
#include "PathRef.hh"
#include "PathVertexRep.hh"
#include "Search.hh"
#include "PathVertex.hh"

namespace sta {

PathVertex::PathVertex() :
  vertex_(nullptr),
  tag_(nullptr),
  arrival_index_(0)
{
}

PathVertex::PathVertex(const PathVertex &path) :
  vertex_(path.vertex_),
  tag_(path.tag_),
  arrival_index_(path.arrival_index_)
{
}

PathVertex::PathVertex(const PathVertex *path) :
  vertex_(nullptr),
  tag_(nullptr),
  arrival_index_(0)
{
  if (path) {
    vertex_ = path->vertex_;
    tag_ = path->tag_;
    arrival_index_ = path->arrival_index_;
  }
}

PathVertex::PathVertex(Vertex *vertex,
		       Tag *tag,
		       const StaState *sta)
{
  init(vertex, tag, sta);
}

PathVertex::PathVertex(Vertex *vertex,
		       Tag *tag,
		       int arrival_index) :
  vertex_(vertex),
  tag_(tag),
  arrival_index_(arrival_index)
{
}

PathVertex::PathVertex(const PathVertexRep *path,
		       const StaState *sta)
{
  if (path)
    init(path->vertex(sta), path->tag(sta), sta);
}

PathVertex::PathVertex(const PathVertexRep &path,
		       const StaState *sta)
{
  if (path.isNull())
    init();
  else
    init(path.vertex(sta), path.tag(sta), sta);
}

void
PathVertex::init()
{
  vertex_ = nullptr;
  tag_ = nullptr;
  arrival_index_ = 0;
}

void
PathVertex::init(Vertex *vertex,
		 Tag *tag,
		 const StaState *sta)
{
  vertex_ = nullptr;
  tag_ = nullptr;
  arrival_index_ = 0;
  const Search *search = sta->search();
  TagGroup *tag_group = search->tagGroup(vertex);
  if (tag_group) {
    bool arrival_exists;
    tag_group->arrivalIndex(tag, arrival_index_, arrival_exists);
    if (arrival_exists) {
      vertex_ = vertex;
      tag_ = tag;
    }
  }
}

void
PathVertex::init(Vertex *vertex,
		 Tag *tag,
		 int arrival_index)
{
  vertex_ = vertex;
  tag_ = tag;
  arrival_index_ = arrival_index;
}

void
PathVertex::init(const PathVertexRep *path,
		 const StaState *sta)
{
  if (path)
    init(path->vertex(sta), path->tag(sta), sta);
  else
    init();
}

void
PathVertex::init(const PathVertexRep &path,
		 const StaState *sta)
{
  if (!path.isNull())
    init(path.vertex(sta), path.tag(sta), sta);
  else
    init();
}

void
PathVertex::operator=(const PathVertex &path)
{
  vertex_ = path.vertex_;
  tag_ = path.tag_;
  arrival_index_ = path.arrival_index_;
}

bool
PathVertex::isNull() const
{
  return tag_ == nullptr;
}

void
PathVertex::setRef(PathRef *ref) const
{
  ref->init(vertex_, tag_, arrival_index_);
}

VertexId
PathVertex::vertexId(const StaState *sta) const
{
  const Graph *graph = sta->graph();
  return graph->id(vertex_);
}

TagIndex
PathVertex::tagIndex(const StaState *) const
{
  return tag_->index();
}

const RiseFall *
PathVertex::transition(const StaState *) const
{
  return tag_->transition();
}

int
PathVertex::rfIndex(const StaState *) const
{
  return tag_->trIndex();
}

PathAnalysisPt *
PathVertex::pathAnalysisPt(const StaState *sta) const
{
  return tag_->pathAnalysisPt(sta);
}

PathAPIndex
PathVertex::pathAnalysisPtIndex(const StaState *) const
{
  return tag_->pathAPIndex();
}

void
PathVertex::arrivalIndex(int &arrival_index,
			 bool &arrival_exists) const
{
  if (tag_) {
    arrival_index = arrival_index_;
    arrival_exists = true;
  }
  else
    arrival_exists = false;
}

void
PathVertex::setArrivalIndex(int arrival_index)
{
  arrival_index_ = arrival_index;
}

Arrival
PathVertex::arrival(const StaState *sta) const
{
  Arrival *arrivals = sta->graph()->arrivals(vertex_);
  return arrivals[arrival_index_];
}

void
PathVertex::setArrival(Arrival arrival,
		       const StaState *sta)
{
  if (tag_) {
    Arrival *arrivals = sta->graph()->arrivals(vertex_);
    arrivals[arrival_index_] = arrival;
  }
}

const Required &
PathVertex::required(const StaState *sta) const
{
  if (tag_ && vertex_->hasRequireds()) {
    const Search *search = sta->search();
    TagGroup *tag_group = search->tagGroup(vertex_);
    int req_index = tag_group->requiredIndex(arrival_index_);
    Arrival *arrivals = sta->graph()->arrivals(vertex_);
    return arrivals[req_index];
  }
  else
    return delayInitValue(minMax(sta)->opposite());
}

void
PathVertex::setRequired(const Required &required,
			const StaState *sta)
{
  Graph *graph = sta->graph();
  const Search *search = sta->search();
  TagGroup *tag_group = search->tagGroup(vertex_);
  Arrival *arrivals = graph->arrivals(vertex_);
  int arrival_count = tag_group->arrivalCount();
  if (!vertex_->hasRequireds()) {
    Arrival *new_arrivals = graph->makeArrivals(vertex_, arrival_count * 2);
    memcpy(new_arrivals, arrivals, arrival_count * sizeof(Arrival));
    vertex_->setHasRequireds(true);
    arrivals = new_arrivals;
  }
  int req_index = arrival_index_ + arrival_count;
  arrivals[req_index] = required;
}

void
PathVertex::deleteRequireds(Vertex *vertex,
			    const StaState *)
{
  vertex->setHasRequireds(false);
  // Don't bother reclaiming requieds from arrival table.
}

bool
PathVertex::equal(const PathVertex *path1,
		  const PathVertex *path2)
{
  return path1->vertex_ == path2->vertex_
    && path1->tag_ == path2->tag_;
}

////////////////////////////////////////////////////////////////

// EvalPred but search to clk source pin.
class PrevPred2 : public SearchPred0
{
public:
  explicit PrevPred2(const StaState *sta);
  virtual bool searchThru(Edge *edge);

private:
  DISALLOW_COPY_AND_ASSIGN(PrevPred2);
};

PrevPred2::PrevPred2(const StaState *sta) :
  SearchPred0(const_cast<StaState*>(sta))
{
}

bool
PrevPred2::searchThru(Edge *edge)
{
  const Sdc *sdc = sta_->sdc();
  TimingRole *role = edge->role();
  return SearchPred0::searchThru(edge)
    && (sdc->dynamicLoopBreaking()
	|| !edge->isDisabledLoop())
    && !role->isTimingCheck();
}

class PrevPathVisitor : public PathVisitor
{
public:
  PrevPathVisitor(const Path *path,
		  SearchPred *pred,
		  const StaState *sta);
  virtual VertexVisitor *copy();
  virtual void visit(Vertex *) {}
  virtual bool visitFromToPath(const Pin *from_pin,
			       Vertex *from_vertex,
			       const RiseFall *from_rf,
			       Tag *from_tag,
			       PathVertex *from_path,
			       Edge *edge,
			       TimingArc *arc,
			       ArcDelay arc_delay,
			       Vertex *to_vertex,
			       const RiseFall *to_rf,
			       Tag *to_tag,
			       Arrival &to_arrival,
			       const MinMax *min_max,
			       const PathAnalysisPt *path_ap);
  PathVertex &prevPath() { return prev_path_; }
  TimingArc *prevArc() const { return prev_arc_; }

protected:
  Tag *unfilteredTag(const Tag *tag) const;

  const Path *path_;
  Arrival path_arrival_;
  Tag *path_tag_;
  int path_rf_index_;
  PathAPIndex path_ap_index_;
  PathVertex prev_path_;
  TimingArc *prev_arc_;
  float dcalc_tol_;

private:
  DISALLOW_COPY_AND_ASSIGN(PrevPathVisitor);
};

PrevPathVisitor::PrevPathVisitor(const Path *path,
				 SearchPred *pred,
				 const StaState *sta) :
  PathVisitor(pred, sta),
  path_(path),
  path_arrival_(path->arrival(sta)),
  path_tag_(path->tag(sta)),
  path_rf_index_(path->rfIndex(sta)),
  path_ap_index_(path->pathAnalysisPtIndex(sta)),
  prev_path_(),
  prev_arc_(nullptr),
  dcalc_tol_(sta->graphDelayCalc()->incrementalDelayTolerance())
{
}

VertexVisitor *
PrevPathVisitor::copy()
{
  return new PrevPathVisitor(path_, pred_, sta_);
}

bool
PrevPathVisitor::visitFromToPath(const Pin *,
				 Vertex *,
				 const RiseFall *,
				 Tag *from_tag,
				 PathVertex *from_path,
				 Edge *,
				 TimingArc *arc,
				 ArcDelay,
				 Vertex *,
				 const RiseFall *to_rf,
				 Tag *to_tag,
				 Arrival &to_arrival,
				 const MinMax *,
				 const PathAnalysisPt *path_ap)
{
  PathAPIndex path_ap_index = path_ap->index();
  if (to_rf->index() == path_rf_index_
      && path_ap_index == path_ap_index_
      && (dcalc_tol_ > 0.0 
	  ? std::abs(delayAsFloat(to_arrival - path_arrival_)) < dcalc_tol_
	  : fuzzyEqual(to_arrival, path_arrival_))
      && (tagMatch(to_tag, path_tag_, sta_)
	  // If the filter exception became active searching from
	  // from_path to to_path the tag includes the filter, but
	  // to_vertex still has paths from previous searches that do
	  // not have the filter.
	  || (!from_tag->isFilter()
	      && to_tag->isFilter()
	      && tagMatch(unfilteredTag(to_tag), path_tag_, sta_)))) {
    int arrival_index;
    bool arrival_exists;
    from_path->arrivalIndex(arrival_index, arrival_exists);
    if (arrival_exists) {
      prev_path_ = from_path;
      prev_arc_ = arc;
      // Stop looking for the previous path/arc.
      return false;
    }
  }
  return true;
}

Tag *
PrevPathVisitor::unfilteredTag(const Tag *tag) const
{
  Search *search = sta_->search();
  const Corners *corners = sta_->corners();
  ExceptionStateSet *unfiltered_states = nullptr;
  const ExceptionStateSet *states = tag->states();
  ExceptionStateSet::ConstIterator state_iter(states);
  while (state_iter.hasNext()) {
    ExceptionState *state = state_iter.next();
    ExceptionPath *except = state->exception();
    if (!except->isFilter()) {
      if (unfiltered_states == nullptr)
	unfiltered_states = new ExceptionStateSet;
      unfiltered_states->insert(state);
    }
  }
  return search->findTag(tag->transition(),
			 corners->findPathAnalysisPt(tag->pathAPIndex()),
			 tag->clkInfo(),
			 tag->isClock(),
			 tag->inputDelay(),
			 tag->isSegmentStart(),
			 unfiltered_states, true);
}

////////////////////////////////////////////////////////////////

void
PathVertex::prevPath(const StaState *sta,
		     // Return values.
		     PathVertex &prev_path,
		     TimingArc *&prev_arc) const
{
  PrevPred2 pred(sta);
  PrevPathVisitor visitor(this, &pred, sta);
  visitor.visitFaninPaths(vertex(sta));
  prev_path = visitor.prevPath();
  prev_arc = visitor.prevArc();
}

void
PathVertex::prevPath(const StaState *sta,
		     // Return values.
		     PathVertex &prev_path) const
{
  PrevPred2 pred(sta);
  PrevPathVisitor visitor(this, &pred, sta);
  visitor.visitFaninPaths(vertex(sta));
  prev_path = visitor.prevPath();
}

void
PathVertex::prevPath(const StaState *sta,
		     // Return values.
		     PathRef &prev_path,
		     TimingArc *&prev_arc) const
{
  PathVertex prev;
  prevPath(sta, prev, prev_arc);
  prev.setRef(prev_path);
}

////////////////////////////////////////////////////////////////

VertexPathIterator::VertexPathIterator(Vertex *vertex,
				       const StaState *sta) :
  search_(sta->search()),
  vertex_(vertex),
  rf_(nullptr),
  path_ap_(nullptr),
  min_max_(nullptr)
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group) {
    arrival_iter_.init(tag_group->arrivalMap());
    findNext();
  }
}

// Iterate over vertex paths with the same transition and
// analysis pt but different but different tags.
VertexPathIterator::VertexPathIterator(Vertex *vertex,
				       const RiseFall *rf,
				       const PathAnalysisPt *path_ap,
				       const StaState *sta) :
  search_(sta->search()),
  vertex_(vertex),
  rf_(rf),
  path_ap_(path_ap),
  min_max_(nullptr)
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group) {
    arrival_iter_.init(tag_group->arrivalMap());
    findNext();
  }
}

VertexPathIterator::VertexPathIterator(Vertex *vertex,
				       const RiseFall *rf,
				       const MinMax *min_max,
				       const StaState *sta) :
  search_(sta->search()),
  vertex_(vertex),
  rf_(rf),
  path_ap_(nullptr),
  min_max_(min_max)
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group) {
    arrival_iter_.init(tag_group->arrivalMap());
    findNext();
  }
}

VertexPathIterator::~VertexPathIterator()
{
}

bool
VertexPathIterator::hasNext()
{
  return !next_.isNull();
}

void
VertexPathIterator::findNext()
{
  while (arrival_iter_.hasNext()) {
    Tag *tag;
    int arrival_index;
    arrival_iter_.next(tag, arrival_index);
    if ((rf_ == nullptr
	 || tag->trIndex() == rf_->index())
	&& (path_ap_ == nullptr
	    || tag->pathAPIndex() == path_ap_->index())
	&& (min_max_ == nullptr
	    || tag->pathAnalysisPt(search_)->pathMinMax() == min_max_)) {
      next_.init(vertex_, tag, arrival_index);
      return;
    }
  }
  next_.init();
}

PathVertex *
VertexPathIterator::next()
{
  path_ = next_;
  findNext();
  return &path_;
}

} // namespace
