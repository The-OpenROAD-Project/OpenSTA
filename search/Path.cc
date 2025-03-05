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

#include "Path.hh"

#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "DcalcAnalysisPt.hh"
#include "Corner.hh"
#include "PathAnalysisPt.hh"
#include "Tag.hh"
#include "TagGroup.hh"
#include "Search.hh"

namespace sta {

Path::Path() :
  prev_path_(nullptr),
  arrival_(0.0),
  required_(0.0),
  vertex_id_(vertex_id_null),
  tag_index_(tag_index_null),
  is_enum_(false),
  prev_arc_idx_(0)
{
}

Path::Path(Vertex *vertex,
           Tag *tag,
           const StaState *sta) :
  prev_path_(nullptr),
  arrival_(0.0),
  required_(0.0),
  tag_index_(tag->index()),
  is_enum_(false),
  prev_arc_idx_(0)
{
  const Graph *graph = sta->graph();
  vertex_id_ = graph->id(vertex);
}

Path::Path(Vertex *vertex,
           Tag *tag,
           Arrival arrival,
           Path *prev_path,
           Edge *prev_edge,
           TimingArc *prev_arc,
           const StaState *sta) :
  prev_path_(prev_path),
  arrival_(arrival),
  required_(0.0),
  tag_index_(tag->index()),
  is_enum_(false)
{
  const Graph *graph = sta->graph();
  if (prev_path) {
    prev_edge_id_ = graph->id(prev_edge);
    prev_arc_idx_ = prev_arc->index();
  }
  else {
    vertex_id_ = graph->id(vertex);
    prev_arc_idx_ = 0;
  }
}

Path::Path(Vertex *vertex,
           Tag *tag,
           Arrival arrival,
           Path *prev_path,
           Edge *prev_edge,
           TimingArc *prev_arc,
           bool is_enum,
           const StaState *sta) :
  prev_path_(prev_path),
  arrival_(arrival),
  required_(0.0),
  tag_index_(tag->index()),
  is_enum_(is_enum)
{
  const Graph *graph = sta->graph();
  if (prev_path) {
    prev_edge_id_ = graph->id(prev_edge);
    prev_arc_idx_ = prev_arc->index();
  }
  else {
    vertex_id_ = graph->id(vertex);
    prev_arc_idx_ = 0;
  }
}

void
Path::init(Vertex *vertex,
           Arrival arrival,
           const StaState *sta)
{
  const Graph *graph = sta->graph();
  vertex_id_ = graph->id(vertex);
  tag_index_ = tag_index_null,
  prev_path_ = nullptr;
  prev_arc_idx_ = 0;
  arrival_ = arrival;
  required_ = 0.0;
  is_enum_ = false;
}

void
Path::init(Vertex *vertex,
           Tag *tag,
           const StaState *sta)
{
  const Graph *graph = sta->graph();
  vertex_id_ = graph->id(vertex);
  tag_index_ = tag->index(),
  prev_path_ = nullptr;
  prev_arc_idx_ = 0;
  arrival_ = 0.0;
  required_ = 0.0;
  is_enum_ = false;
}

void
Path::init(Vertex *vertex,
           Tag *tag,
           Arrival arrival,
           Path *prev_path,
           Edge *prev_edge,
           TimingArc *prev_arc,
           const StaState *sta)
{
  const Graph *graph = sta->graph();
  tag_index_ = tag->index(),
  prev_path_ = prev_path;
  if (prev_path) {
    prev_edge_id_ = graph->id(prev_edge);
    prev_arc_idx_ = prev_arc->index();
  }
  else {
    vertex_id_ = graph->id(vertex);
    prev_arc_idx_ = 0;
  }
  arrival_ = arrival;
  required_ = 0.0;
  is_enum_ = false;
}

const char *
Path::name(const StaState *sta) const
{
  const Network *network = sta->network();
  Vertex *vertex = this->vertex(sta);
  if (vertex) {
    const char *vertex_name = vertex->name(network);
    const char *tr_str = transition(sta)->asString();
    const PathAnalysisPt *path_ap = pathAnalysisPt(sta);
    int ap_index = path_ap->index();
    const char *min_max = path_ap->pathMinMax()->asString();
    TagIndex tag_index = tagIndex(sta);
    return stringPrintTmp("%s %s %s/%d %d",
			  vertex_name, tr_str, min_max,
			  ap_index, tag_index);
  }
  else
    return "NULL";
}

bool
Path::isNull() const
{
  return vertex_id_ == vertex_id_null;
}

Vertex *
Path::vertex(const StaState *sta) const
{
  const Graph *graph = sta->graph();
  if (prev_path_) {
    const Edge *edge = graph->edge(prev_edge_id_);
    return edge->to(graph);
  }
  else 
    return graph->vertex(vertex_id_);
}

VertexId
Path::vertexId(const StaState *sta) const
{
  const Graph *graph = sta->graph();
  if (prev_path_) {
    const Edge *edge = graph->edge(prev_edge_id_);
    return edge->to();
  }
  else 
    return vertex_id_;
}

Pin *
Path::pin(const StaState *sta) const
{
  return vertex(sta)->pin();
}

Tag *
Path::tag(const StaState *sta) const
{
  const Search *search = sta->search();
  return search->tag(tag_index_);
}

void
Path::setTag(Tag *tag)
{
  tag_index_ = tag->index();
}

TagIndex
Path::tagIndex(const StaState *) const
{
  return tag_index_;
}

size_t
Path::pathIndex(const StaState *sta) const
{
  Vertex *vertex = this->vertex(sta);
  Path *paths = vertex->paths();
  return this - paths;
}

ClkInfo *
Path::clkInfo(const StaState *sta) const
{
  return tag(sta)->clkInfo();
}

const ClockEdge *
Path::clkEdge(const StaState *sta) const
{
  return tag(sta)->clkEdge();
}

const Clock *
Path::clock(const StaState *sta) const
{
  return tag(sta)->clock();
}

bool
Path::isClock(const StaState *sta) const
{
  return tag(sta)->isClock();
}

const MinMax *
Path::minMax(const StaState *sta) const
{
  return pathAnalysisPt(sta)->pathMinMax();
}

PathAPIndex
Path::pathAnalysisPtIndex(const StaState *sta) const
{
  return pathAnalysisPt(sta)->index();
}

DcalcAnalysisPt *
Path::dcalcAnalysisPt(const StaState *sta) const
{
  return pathAnalysisPt(sta)->dcalcAnalysisPt();
}

Slew
Path::slew(const StaState *sta) const
{
  return sta->graph()->slew(vertex(sta), transition(sta),
			    dcalcAnalysisPt(sta)->index());
}

const RiseFall *
Path::transition(const StaState *sta) const
{
  return tag(sta)->transition();
}

int
Path::rfIndex(const StaState *sta) const
{
  return transition(sta)->index();
}

PathAnalysisPt *
Path::pathAnalysisPt(const StaState *sta) const
{
  return tag(sta)->pathAnalysisPt(sta);
}

void
Path::initArrival(const StaState *sta)
{
  setArrival(delayInitValue(minMax(sta)));
}

void
Path::setArrival(Arrival arrival)
{
  arrival_ = arrival;
}

bool
Path::arrivalIsInitValue(const StaState *sta) const
{
  return delayIsInitValue(arrival_, minMax(sta));
}

void
Path::setRequired(const Required &required)
{
  required_ = required;
}

void
Path::initRequired(const StaState *sta)
{
  required_ = delayInitValue(minMax(sta)->opposite());
}

bool
Path::requiredIsInitValue(const StaState *sta) const
{
  return delayIsInitValue(required_, minMax(sta)->opposite());
}

Slack
Path::slack(const StaState *sta) const
{
  if (minMax(sta) == MinMax::max())
    return required_ - arrival_;
  else
    return arrival_ - required_;
}

void
Path::setPrevPath(Path *prev_path)
{
  prev_path_ = prev_path;
}

TimingArc *
Path::prevArc(const StaState *sta) const
{
  if (prev_path_) {
    const Graph *graph = sta->graph();
    const Edge *edge = graph->edge(prev_edge_id_);
    TimingArcSet *arc_set = edge->timingArcSet();
    return arc_set->findTimingArc(prev_arc_idx_);
  }
  else 
    return nullptr;
}

Edge *
Path::prevEdge(const StaState *sta) const
{
  if (prev_path_) {
    const Graph *graph = sta->graph();
    return graph->edge(prev_edge_id_);
  }
  else 
    return nullptr;
}

void
Path::setPrevEdgeArc(Edge *prev_edge,
                     TimingArc *prev_arc,
                     const StaState *sta)
{
  if (prev_edge) {
    const Graph *graph = sta->graph();
    prev_edge_id_ = graph->id(prev_edge);
    prev_arc_idx_ = prev_arc->index();
  }
  else
    prev_arc_idx_ = 0;
}

////////////////////////////////////////////////////////////////

int
Path::cmpPinTrClk(const Path *path1,
		  const Path *path2,
		  const StaState *sta)
{
  if (path1 && path2) {
    const Pin *pin1 = path1->pin(sta);
    const Pin *pin2 = path2->pin(sta);
    const Network *network = sta->network();
    if (pin1 == pin2) {
      int tr_index1 = path1->rfIndex(sta);
      int tr_index2 = path2->rfIndex(sta);
      if (tr_index1 == tr_index2)
	return cmpClk(path1, path2, sta);
      else if (tr_index1 < tr_index2)
	return -1;
      else
	return 1;
    }
    else if (network->pathNameLess(pin1, pin2))
      return -1;
    else
      return 1;
  }
  else if (path1 == nullptr && path2 == nullptr)
    return 0;
  else if (path1 == nullptr)
    return -1;
  else
    return 1;
}

int
Path::cmpClk(const Path *path1,
	     const Path *path2,
	     const StaState *sta)
{
  const ClockEdge *clk_edge1 = path1->clkEdge(sta);
  const ClockEdge *clk_edge2 = path2->clkEdge(sta);
  if (clk_edge1 && clk_edge2) {
    int index1 = clk_edge1->index();
    int index2 = clk_edge2->index();
    if (index1 == index2)
      return 0;
    else if (index1 < index2)
      return -1;
    else
      return 1;
  }
  else if (clk_edge1 == nullptr
	   && clk_edge2 == nullptr)
    return 0;
  else if (clk_edge2)
    return -1;
  else
    return 1;
}

bool
Path::equal(const Path *path1,
	    const Path *path2,
	    const StaState *sta)
{
  return (path1 == nullptr && path2 == nullptr)
    || (path1
	&& path2
	&& path1->vertexId(sta) == path2->vertexId(sta)
	// Tag equal implies transition and path ap equal.
	&& path1->tagIndex(sta) == path2->tagIndex(sta));
}

void
Path::operator=(const Path *path)
{
  *this = *path;
}

////////////////////////////////////////////////////////////////

PathLess::PathLess(const StaState *sta) :
  sta_(sta)
{
}

bool
PathLess::operator()(const Path *path1,
		     const Path *path2) const
{
  return Path::less(path1, path2, sta_);
}

bool
Path::less(const Path *path1,
	   const Path *path2,
	   const StaState *sta)
{
  return cmp(path1, path2, sta) < 0;
}

int
Path::cmp(const Path *path1,
	  const Path *path2,
	  const StaState *sta)
{
  if (path1 && path2) {
    VertexId vertex_id1 = path1->vertexId(sta);
    VertexId vertex_id2 = path2->vertexId(sta);
    if (vertex_id1 == vertex_id2) {
      TagIndex tag_index1 = path1->tagIndex(sta);
      TagIndex tag_index2 = path2->tagIndex(sta);
      if (tag_index1 == tag_index2)
	return 0;
      else if (tag_index1 < tag_index2)
	return -1;
      else
	return 1;
    }
    else if (vertex_id1 < vertex_id2)
      return -1;
    else
      return 1;
  }
  else if (path1 == nullptr
	   && path2 == nullptr)
    return 0;
  else if (path1 == nullptr)
    return -1;
  else
    return 1;
}

int
Path::cmpNoCrpr(const Path *path1,
		const Path *path2,
		const StaState *sta)
{
  VertexId vertex_id1 = path1->vertexId(sta);
  VertexId vertex_id2 = path2->vertexId(sta);
  if (vertex_id1 == vertex_id2)
    return tagMatchCmp(path1->tag(sta), path2->tag(sta), false, sta);
  else if (vertex_id1 < vertex_id2)
    return -1;
  else
    return 1;
}

int
Path::cmpAll(const Path *path1,
	     const Path *path2,
	     const StaState *sta)
{
  const Path *p1 = path1;
  const Path *p2 = path2;
  while (p1 && p2) {
    int cmp = Path::cmp(p1, p2, sta);
    if (cmp != 0)
      return cmp;

    TimingArc *prev_arc1 = p1->prevArc(sta);
    TimingArc *prev_arc2 = p2->prevArc(sta);
    p1 = p1->prevPath();
    p2 = p2->prevPath();
    if (equal(p1, path1, sta))
      // Equivalent latch loops.
      return 0;
    if ((prev_arc1 && prev_arc1->role()->isLatchDtoQ())
        || (prev_arc2 && prev_arc2->role()->isLatchDtoQ()))
      break;
  }
  if (p1 == nullptr && p2 == nullptr)
    return 0;
  else if (p1 == nullptr && p2 != nullptr)
    return -1;
  else
    return 1;
}

bool
Path::lessAll(const Path *path1,
	      const Path *path2,
	      const StaState *sta)
{
  return cmpAll(path1, path2, sta) < 0;
}

////////////////////////////////////////////////////////////////

VertexPathIterator::VertexPathIterator(Vertex *vertex,
				       const StaState *sta) :
  search_(sta->search()),
  filtered_(false),
  rf_(nullptr),
  path_ap_(nullptr),
  min_max_(nullptr),
  paths_(vertex->paths()),
  path_count_(0),
  path_index_(0),
  next_(nullptr)
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group) {
    path_count_ = tag_group->pathCount();
    path_iter_.init(tag_group->pathIndexMap());
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
  filtered_(true),
  rf_(rf),
  path_ap_(path_ap),
  min_max_(nullptr),
  paths_(vertex->paths()),
  path_count_(0),
  path_index_(0),
  next_(nullptr)
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group) {
    path_count_ = tag_group->pathCount();
    path_iter_.init(tag_group->pathIndexMap());
    findNext();
  }
}

VertexPathIterator::VertexPathIterator(Vertex *vertex,
				       const RiseFall *rf,
				       const MinMax *min_max,
				       const StaState *sta) :
  search_(sta->search()),
  filtered_(true),
  rf_(rf),
  path_ap_(nullptr),
  min_max_(min_max),
  paths_(vertex->paths()),
  path_count_(0),
  path_index_(0),
  next_(nullptr)
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group) {
    path_count_ = tag_group->pathCount();
    path_iter_.init(tag_group->pathIndexMap());
    findNext();
  }
}

VertexPathIterator::VertexPathIterator(Vertex *vertex,
				       const RiseFall *rf,
				       const PathAnalysisPt *path_ap,
				       const MinMax *min_max,
				       const StaState *sta) :
  search_(sta->search()),
  filtered_(true),
  rf_(rf),
  path_ap_(path_ap),
  min_max_(min_max),
  paths_(vertex->paths()),
  path_count_(0),
  path_index_(0),
  next_(nullptr)
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group) {
    path_count_ = tag_group->pathCount();
    path_iter_.init(tag_group->pathIndexMap());
    findNext();
  }
}

VertexPathIterator::~VertexPathIterator()
{
}

bool
VertexPathIterator::hasNext()
{
  return next_ != nullptr;
}

#if 0
void
VertexPathIterator::findNext()
{
  while (path_index_ < path_count_) {
    Path *path = &paths_[path_index_++];
    if (filtered_) {
      const Tag *tag = path->tag(search_);
      if ((rf_ == nullptr
           || tag->rfIndex() == rf_->index())
          && (path_ap_ == nullptr
              || tag->pathAPIndex() == path_ap_->index())
          && (min_max_ == nullptr
              || tag->pathAnalysisPt(search_)->pathMinMax() == min_max_)) {
        next_ = path;
        return;
      }
    }
    else {
      next_ = path;
      return;
    }
  }
  next_ = nullptr;
}
#endif

void
VertexPathIterator::findNext()
{
  while (path_iter_.hasNext()) {
    Tag *tag;
    size_t path_index;
    path_iter_.next(tag, path_index);
    if ((rf_ == nullptr
	 || tag->rfIndex() == rf_->index())
	&& (path_ap_ == nullptr
	    || tag->pathAPIndex() == path_ap_->index())
	&& (min_max_ == nullptr
	    || tag->pathAnalysisPt(search_)->pathMinMax() == min_max_)) {
      next_ = &paths_[path_index];
      return;
    }
  }
  next_ = nullptr;
}

Path *
VertexPathIterator::next()
{
  Path *path = next_;
  findNext();
  return path;
}

} // namespace
