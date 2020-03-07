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

#include <algorithm>
#include <limits>
#include "Machine.hh"
#include "Stats.hh"
#include "Debug.hh"
#include "Mutex.hh"
#include "Fuzzy.hh"
#include "MinMax.hh"
#include "ExceptionPath.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "PathEnd.hh"
#include "PathAnalysisPt.hh"
#include "Tag.hh"
#include "Corner.hh"
#include "Search.hh"
#include "VisitPathEnds.hh"
#include "PathEnum.hh"
#include "DispatchQueue.hh"
#include "PathGroup.hh"

namespace sta {

int PathGroup::group_count_max = std::numeric_limits<int>::max();

PathGroup *
PathGroup::makePathGroupSlack(const char *name,
			      int group_count,
			      int endpoint_count,
			      bool unique_pins,
			      float slack_min,
			      float slack_max,
			      const StaState *sta)
{
  return new PathGroup(name, group_count, endpoint_count, unique_pins,
		       slack_min, slack_max, true, MinMax::min(), sta);
}

PathGroup *
PathGroup::makePathGroupArrival(const char *name,
				int group_count,
				int endpoint_count,
				bool unique_pins,
				const MinMax *min_max,
				const StaState *sta)
{
  return new PathGroup(name, group_count, endpoint_count, unique_pins,
		       0.0, 0.0, false, min_max, sta);
}

PathGroup::PathGroup(const char *name,
		     int group_count,
		     int endpoint_count,
		     bool unique_pins,
		     float slack_min,
		     float slack_max,
		     bool cmp_slack,
		     const MinMax *min_max,
		     const StaState *sta) :
  name_(name),
  group_count_(group_count),
  endpoint_count_(endpoint_count),
  unique_pins_(unique_pins),
  slack_min_(slack_min),
  slack_max_(slack_max),
  min_max_(min_max),
  compare_slack_(cmp_slack),
  threshold_(min_max->initValue()),
  sta_(sta)
{
}

PathGroup::~PathGroup()
{
  path_ends_.deleteContents();
}

bool
PathGroup::savable(PathEnd *path_end)
{
  bool savable = false;
  if (compare_slack_) {
    // Crpr increases the slack, so check the slack
    // without crpr first because it is expensive to find.
    Slack slack = path_end->slackNoCrpr(sta_);
    if (!delayIsInitValue(slack, min_max_)
 	&& fuzzyLessEqual(slack, threshold_)
 	&& fuzzyLessEqual(slack, slack_max_)) {
      // Now check with crpr.
      slack = path_end->slack(sta_);
      savable = fuzzyLessEqual(slack, threshold_)
 	&& fuzzyLessEqual(slack, slack_max_)
 	&& fuzzyGreaterEqual(slack, slack_min_);
    }
  }
  else {
    const Arrival &arrival = path_end->dataArrivalTime(sta_);
    savable = !delayIsInitValue(arrival, min_max_)
      && fuzzyGreaterEqual(arrival, threshold_, min_max_);
  }
  return savable;
}

void
PathGroup::insert(PathEnd *path_end)
{
  UniqueLock lock(lock_);
  path_ends_.push_back(path_end);
  if (group_count_ != group_count_max
      && static_cast<int>(path_ends_.size()) > group_count_ * 2)
    prune();
}

void
PathGroup::prune()
{
  sort();
  VertexPathCountMap path_counts;
  int end_count = 0;
  for (unsigned i = 0; i < path_ends_.size(); i++) {
    PathEnd *path_end = path_ends_[i];
    Vertex *vertex = path_end->vertex(sta_);
    // Squish up to endpoint_count path ends per vertex up to the front of path_ends_.
    if (end_count < group_count_
	&& path_counts[vertex] < endpoint_count_) {
      path_ends_[end_count++] = path_end;
      path_counts[vertex]++;
    }
    else
      delete path_end;
  }
  path_ends_.resize(end_count);

  // Set a threshold to the bottom of the sorted list that future
  // inserts need to beat.
  PathEnd *last_end = path_ends_[end_count - 1];
  if (compare_slack_)
    threshold_ = delayAsFloat(last_end->slack(sta_));
  else
    threshold_ = delayAsFloat(last_end->dataArrivalTime(sta_));
}

void
PathGroup::pushEnds(PathEndSeq *path_ends)
{
  ensureSortedMaxPaths();
  for (PathEnd *path_end : path_ends_)
    path_ends->push_back(path_end);
}

PathGroupIterator *
PathGroup::iterator()
{
  ensureSortedMaxPaths();
  return new PathGroupIterator(path_ends_);
}

void
PathGroup::ensureSortedMaxPaths()
{
  if (static_cast<int>(path_ends_.size()) > group_count_)
    prune();
  else
    sort();
}

void
PathGroup::sort()
{
  sta::sort(path_ends_, PathEndLess(sta_));
}

void
PathGroup::clear()
{
  UniqueLock lock(lock_);
  threshold_ = min_max_->initValue();
  path_ends_.clear();
}

////////////////////////////////////////////////////////////////

const char *PathGroups::path_delay_group_name_ = "**default**";
const char *PathGroups::gated_clk_group_name_ = "**clock_gating_default**";
const char *PathGroups::async_group_name_ = "**async_default**";
const char *PathGroups::unconstrained_group_name_ = "(none)";

bool
PathGroups::isGroupPathName(const char *group_name)
{
  return stringEq(group_name, path_delay_group_name_)
    || stringEq(group_name, gated_clk_group_name_)
    || stringEq(group_name, async_group_name_)
    || stringEq(group_name, unconstrained_group_name_);
}

PathGroups::PathGroups(int group_count,
		       int endpoint_count,
		       bool unique_pins,
		       float slack_min,
		       float slack_max,
		       PathGroupNameSet *group_names,
		       bool setup,
		       bool hold,
		       bool recovery,
		       bool removal,
		       bool clk_gating_setup,
		       bool clk_gating_hold,
		       bool unconstrained,
		       const StaState *sta) :
  StaState(sta),
  group_count_(group_count),
  endpoint_count_(endpoint_count),
  unique_pins_(unique_pins),
  slack_min_(slack_min),
  slack_max_(slack_max)
{
  makeGroups(group_count, endpoint_count, unique_pins, slack_min, slack_max, group_names,
	     setup, recovery, clk_gating_setup, unconstrained,
	     MinMax::max());
  makeGroups(group_count, endpoint_count, unique_pins, slack_min, slack_max, group_names,
	     hold, removal, clk_gating_hold, unconstrained,
	     MinMax::min());
}

void
PathGroups::makeGroups(int group_count,
		       int endpoint_count,
		       bool unique_pins,
		       float slack_min,
		       float slack_max,
		       PathGroupNameSet *group_names,
		       bool setup_hold,
		       bool async,
		       bool gated_clk,
		       bool unconstrained,
		       const MinMax *min_max)
{
  int mm_index = min_max->index();
  if (setup_hold) {
    GroupPathIterator group_path_iter(sdc_);
    while (group_path_iter.hasNext()) {
      const char *name;
      GroupPathSet *groups;
      group_path_iter.next(name, groups);
      if (reportGroup(name, group_names)) {
	PathGroup *group = PathGroup::makePathGroupSlack(name, group_count,
							 endpoint_count, unique_pins,
							 slack_min, slack_max,
							 this);
	named_map_[mm_index][name] = group;
      }
    }

    for (auto clk : sdc_->clks()) {
      const char *clk_name = clk->name();
      if (reportGroup(clk_name, group_names)) {
	PathGroup *group = PathGroup::makePathGroupSlack(clk_name, group_count,
							 endpoint_count, unique_pins,
							 slack_min, slack_max,
							 this);
	clk_map_[mm_index][clk] = group;
      }
    }
  }

  if (setup_hold
      && reportGroup(path_delay_group_name_, group_names))
    path_delay_[mm_index] = PathGroup::makePathGroupSlack(path_delay_group_name_,
							  group_count, endpoint_count,
							  unique_pins,
							  slack_min, slack_max,
							  this);
  else
    path_delay_[mm_index] = nullptr;

  if (gated_clk
      && reportGroup(gated_clk_group_name_, group_names))
    gated_clk_[mm_index] = PathGroup::makePathGroupSlack(gated_clk_group_name_,
							 group_count, endpoint_count,
							 unique_pins,
							 slack_min, slack_max,
							 this);
  else
    gated_clk_[mm_index] = nullptr;

  if (async
      && reportGroup(async_group_name_, group_names))
    async_[mm_index] = PathGroup::makePathGroupSlack(async_group_name_,
						     group_count, endpoint_count,
						     unique_pins,
						     slack_min, slack_max,
						     this);
  else
    async_[mm_index] = nullptr;

  if (unconstrained
      && reportGroup(unconstrained_group_name_, group_names))
    unconstrained_[mm_index] =
      PathGroup::makePathGroupArrival(unconstrained_group_name_,
				      group_count, endpoint_count, unique_pins,
				      min_max, this);
  else
    unconstrained_[mm_index] = nullptr;
}

PathGroups::~PathGroups()
{
  for (auto mm_index : MinMax::rangeIndex()) {
    named_map_[mm_index].deleteContents();
    clk_map_[mm_index].deleteContents();
    delete path_delay_[mm_index];
    delete gated_clk_[mm_index];
    delete async_[mm_index];
    delete unconstrained_[mm_index];
  }
}

PathGroup *
PathGroups::findPathGroup(const char *name,
			  const MinMax *min_max) const
{
  return named_map_[min_max->index()].findKey(name);
}

PathGroup *
PathGroups::findPathGroup(const Clock *clock,
			  const MinMax *min_max) const
{
  return clk_map_[min_max->index()].findKey(clock);
}

bool
PathGroups::reportGroup(const char *group_name,
			PathGroupNameSet *group_names) const
{
  return group_names == nullptr
    || group_names->empty()
    || group_names->hasKey(group_name);
}

PathGroup *
PathGroups::pathGroup(const PathEnd *path_end) const
{
  const MinMax *min_max = path_end->minMax(this);
  int mm_index =  min_max->index();
  // GroupPaths have precedence.
  GroupPath *group_path = groupPathTo(path_end);
 if (group_path) {
   if (group_path->isDefault())
     return path_delay_[mm_index];
   else {
     const char *group_name = group_path->name();
     return findPathGroup(group_name, min_max);
   }
  }
  else if (path_end->isCheck() || path_end->isLatchCheck()) {
    const TimingRole *check_role = path_end->checkRole(this);
    const Clock *tgt_clk = path_end->targetClk(this);
    if (check_role == TimingRole::removal()
	|| check_role == TimingRole::recovery())
      return async_[mm_index];
    else
      return findPathGroup(tgt_clk, min_max);
  }
  else if (path_end->isOutputDelay()
	   || path_end->isDataCheck())
    return findPathGroup(path_end->targetClk(this), min_max);
  else if (path_end->isGatedClock())
    return gated_clk_[mm_index];
  else if (path_end->isPathDelay()) {
    // Path delays that end at timing checks are part of the target clk group
    // unless -ignore_clock_latency is true.
    PathDelay *path_delay = path_end->pathDelay();
    Clock *tgt_clk = path_end->targetClk(this);
    if (tgt_clk
	&& !path_delay->ignoreClkLatency())
      return findPathGroup(tgt_clk, min_max);
    else
      return path_delay_[mm_index];
  }
  else if (path_end->isUnconstrained())
    return unconstrained_[mm_index];
  else {
    internalError("unknown path end type");
    return nullptr;
  }
}

GroupPath *
PathGroups::groupPathTo(const PathEnd *path_end) const
{
  const Path *path = path_end->path();
  const Pin *pin = path->pin(this);
  ExceptionPath *exception = 
    search_->exceptionTo(ExceptionPathType::group_path, path,
			 pin, path->transition(this),
			 path_end->targetClkEdge(this),
			 path->minMax(this), false, false);
  return dynamic_cast<GroupPath*>(exception);
}

void
PathGroups::pushGroupPathEnds(PathEndSeq *path_ends)
{
  for (auto min_max : MinMax::range()) {
    int mm_index =  min_max->index();
    GroupPathIterator group_path_iter(sdc_);
    while (group_path_iter.hasNext()) {
      const char *name;
      GroupPathSet *groups;
      group_path_iter.next(name, groups);
      PathGroup *path_group = findPathGroup(name, min_max);
      if (path_group)
	path_group->pushEnds(path_ends);
    }

    if (async_[mm_index])
      async_[mm_index]->pushEnds(path_ends);

    if (gated_clk_[mm_index])
      gated_clk_[mm_index]->pushEnds(path_ends);

    if (path_delay_[mm_index])
      path_delay_[mm_index]->pushEnds(path_ends);

    ClockSeq clks;
    sdc_->sortedClocks(clks);
    ClockSeq::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
      PathGroup *path_group = findPathGroup(clk, min_max);
      if (path_group)
	path_group->pushEnds(path_ends);
    }
  }
}

void
PathGroups::pushUnconstrainedPathEnds(PathEndSeq *path_ends,
				      const MinMaxAll *min_max)
{
  Set<PathGroup *> groups;
  for (auto path_ap : corners_->pathAnalysisPts()) {
    const MinMax *path_min_max = path_ap->pathMinMax();
    if (min_max->matches(path_min_max)) {
      int mm_index =  path_min_max->index();
      PathGroup *group = unconstrained_[mm_index];
      if (group
	  // For multiple corner path APs use the same group.
	  // Only report it once.
	  && !groups.findKey(group)) {
	group->pushEnds(path_ends);
        groups.insert(group);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

typedef Map<PathGroup*, PathEnd*> PathGroupEndMap;
typedef Map<PathGroup*, PathEndSeq*> PathGroupEndsMap;
typedef Set<PathEnd*, PathEndNoCrprLess> PathEndNoCrprSet;

static bool
exceptionToEmpty(ExceptionTo *to);

PathEndSeq *
PathGroups::makePathEnds(ExceptionTo *to,
			 bool unconstrained_paths,
			 const Corner *corner,
			 const MinMaxAll *min_max,
			 bool sort_by_slack)
{
  Stats stats(this->debug());
  makeGroupPathEnds(to, group_count_, endpoint_count_, unique_pins_,
		    corner, min_max);

  PathEndSeq *path_ends = new PathEndSeq;
  pushGroupPathEnds(path_ends);
  if (sort_by_slack) {
    sort(path_ends, PathEndLess(this));
    if (static_cast<int>(path_ends->size()) > group_count_)
      path_ends->resize(group_count_);
  }

  if (unconstrained_paths
      && path_ends->empty())
    // No constrained paths, so report unconstrained paths.
    pushUnconstrainedPathEnds(path_ends, min_max);

  stats.report("Make path ends");
  return path_ends;
}

////////////////////////////////////////////////////////////////

// Visit each path end for a vertex and add the worst one in each
// path group to the group.
class MakePathEnds1 : public PathEndVisitor
{
public:
  explicit MakePathEnds1(PathGroups *path_groups);
  virtual PathEndVisitor *copy();
  virtual void visit(PathEnd *path_end);
  virtual void vertexEnd(Vertex *vertex);

private:
  DISALLOW_COPY_AND_ASSIGN(MakePathEnds1);
  void visitPathEnd(PathEnd *path_end,
		    PathGroup *group);

  PathGroups *path_groups_;
  PathGroupEndMap ends_;
  PathEndLess cmp_;
};

MakePathEnds1::MakePathEnds1(PathGroups *path_groups) :
  path_groups_(path_groups),
  cmp_(path_groups){

}

PathEndVisitor *
MakePathEnds1::copy()
{
  return new MakePathEnds1(path_groups_);
}

void
MakePathEnds1::visit(PathEnd *path_end)
{
  PathGroup *group = path_groups_->pathGroup(path_end);
  if (group)
    visitPathEnd(path_end, group);
}

void
MakePathEnds1::visitPathEnd(PathEnd *path_end,
			    PathGroup *group)
{
  if (group->savable(path_end)) {
    // Only keep the path end with the smallest slack/latest arrival.
    PathEnd *worst_end = ends_.findKey(group);
    if (worst_end) {
      if (cmp_(path_end, worst_end)) {
	ends_[group] = path_end->copy();
	delete worst_end;
      }
    }
    else
      ends_[group] = path_end->copy();
  }
}

// Save the worst end for each path group.
void
MakePathEnds1::vertexEnd(Vertex *)
{
  PathGroupEndMap::Iterator group_iter(ends_);
  while (group_iter.hasNext()) {
    PathGroup *group;
    PathEnd *end;
    group_iter.next(group, end);
    // visitPathEnd already confirmed slack is savable.
    if (end) {
      group->insert(end);
      // Clear ends_ for next vertex.
      ends_[group] = nullptr;
    }
  }
}

////////////////////////////////////////////////////////////////

// Visit each path end and add it to the corresponding path group.
// After collecting the ends do parallel path enumeration to find the
// path ends for the group.
class MakePathEndsAll : public PathEndVisitor
{
public:
  explicit MakePathEndsAll(int endpoint_count,
			   PathGroups *path_groups);
  virtual ~MakePathEndsAll();
  virtual PathEndVisitor *copy();
  virtual void visit(PathEnd *path_end);
  virtual void vertexEnd(Vertex *vertex);

private:
  DISALLOW_COPY_AND_ASSIGN(MakePathEndsAll);
  void visitPathEnd(PathEnd *path_end,
		    PathGroup *group);

  int endpoint_count_;
  PathGroups *path_groups_;
  const StaState *sta_;
  PathGroupEndsMap ends_;
  PathEndSlackLess slack_cmp_;
  PathEndNoCrprLess path_no_crpr_cmp_;
};

MakePathEndsAll::MakePathEndsAll(int endpoint_count,
				 PathGroups *path_groups) :
  endpoint_count_(endpoint_count),
  path_groups_(path_groups),
  sta_(path_groups),
  slack_cmp_(path_groups),
  path_no_crpr_cmp_(path_groups)
{
}


PathEndVisitor *
MakePathEndsAll::copy()
{
  return new MakePathEndsAll(endpoint_count_, path_groups_);
}

MakePathEndsAll::~MakePathEndsAll()
{
  PathGroupEndsMap::Iterator group_iter(ends_);
  while (group_iter.hasNext()) {
    PathGroup *group;
    PathEndSeq *ends;
    group_iter.next(group, ends);
    delete ends;
  }
}

void
MakePathEndsAll::visit(PathEnd *path_end)
{
  PathGroup *group = path_groups_->pathGroup(path_end);
  if (group)
    visitPathEnd(path_end, group);
}

void
MakePathEndsAll::visitPathEnd(PathEnd *path_end,
			      PathGroup *group)
{
  PathEndSeq *ends = ends_.findKey(group);
  if (ends == nullptr) {
    ends = new PathEndSeq;
    ends_[group] = ends;
  }
  ends->push_back(path_end->copy());
}

void
MakePathEndsAll::vertexEnd(Vertex *)
{
  Debug *debug = sta_->debug();
  Network *network = sta_->network();
  PathGroupEndsMap::Iterator group_iter(ends_);
  while (group_iter.hasNext()) {
    PathGroup *group;
    PathEndSeq *ends;
    group_iter.next(group, ends);
    if (ends) {
      sort(ends, slack_cmp_);
      PathEndNoCrprSet unique_ends(path_no_crpr_cmp_);
      PathEndSeq::Iterator end_iter(ends);
      int n = 0;
      while (end_iter.hasNext()
	     && n < endpoint_count_) {
	PathEnd *path_end = end_iter.next();
	// Only save the worst path end for each crpr tag.
	// PathEnum will peel the others.
	if (!unique_ends.hasKey(path_end)) {
	  debugPrint4(debug, "path_enum", 5, "insert %s %s %s %d\n",
		      path_end->vertex(sta_)->name(network),
		      path_end->typeName(),
		      path_end->transition(sta_)->asString(),
		      path_end->path()->tag(sta_)->index());
	  // Give the group a copy of the path end because
	  // it may delete it during pruning.
	  if (group->savable(path_end)) {
	    group->insert(path_end->copy());
	    unique_ends.insert(path_end);
	    n++;
	  }
	}
	else
	  debugPrint4(debug, "path_enum", 5, "prune %s %s %s %d\n",
		      path_end->vertex(sta_)->name(network),
		      path_end->typeName(),
		      path_end->transition(sta_)->asString(),
		      path_end->path()->tag(sta_)->index());
      }
      // Clear ends for next vertex.
      PathEndSeq::Iterator end_iter2(ends);
      while (end_iter2.hasNext()) {
	PathEnd *path_end = end_iter2.next();
	delete path_end;
      }
      ends->clear();
    }
  }
}

////////////////////////////////////////////////////////////////

void
PathGroups::makeGroupPathEnds(ExceptionTo *to,
			      int group_count,
			      int endpoint_count,
			      bool unique_pins,
			      const Corner *corner,
			      const MinMaxAll *min_max)
{
  if (endpoint_count == 1) {
    MakePathEnds1 make_path_ends(this);
    makeGroupPathEnds(to, corner, min_max, &make_path_ends);
  }
  else {
    MakePathEndsAll make_path_ends(endpoint_count, this);
    makeGroupPathEnds(to, corner, min_max, &make_path_ends);

    for (auto path_min_max : MinMax::range()) {
      int mm_index =  path_min_max->index();
      GroupPathIterator group_path_iter(sdc_);
      while (group_path_iter.hasNext()) {
	const char *name;
	GroupPathSet *groups;
	group_path_iter.next(name, groups);
	PathGroup *group = findPathGroup(name, path_min_max);
	if (group)
	  enumPathEnds(group, group_count, endpoint_count, unique_pins, true);
      }

      for (auto clk : sdc_->clks()) {
	PathGroup *group = findPathGroup(clk, path_min_max);
	if (group)
	  enumPathEnds(group, group_count, endpoint_count, unique_pins, true);
      }

      PathGroup *group = unconstrained_[mm_index];
      if (group)
	enumPathEnds(group, group_count, endpoint_count, unique_pins, false);
      group = path_delay_[mm_index];
      if (group)
	enumPathEnds(group, group_count, endpoint_count, unique_pins, true);
      group = gated_clk_[mm_index];
      if (group)
	enumPathEnds(group, group_count, endpoint_count, unique_pins, true);
      group = async_[mm_index];
      if (group)
	enumPathEnds(group, group_count, endpoint_count, unique_pins, true);
    }
  }
}

void
PathGroups::enumPathEnds(PathGroup *group,
			 int group_count,
			 int endpoint_count,
			 bool unique_pins,
			 bool cmp_slack)
{
  // Insert the worst max_path path ends in the group into a path
  // enumerator.
  PathEnum path_enum(group_count, endpoint_count, unique_pins, cmp_slack, this);
  PathGroupIterator *end_iter = group->iterator();
  while (end_iter->hasNext()) {
    PathEnd *end = end_iter->next();
    if (group->savable(end))
      path_enum.insert(end);
  }
  delete end_iter;
  group->clear();

  // Parallel path enumeratation to find the endpoint_count/max path ends.
  for (int n = 0; path_enum.hasNext() && n < group_count; n++) {
    PathEnd *end = path_enum.next();
    group->insert(end);
  }
}

void
PathGroups::makeGroupPathEnds(ExceptionTo *to,
			      const Corner *corner,
			      const MinMaxAll *min_max,
			      PathEndVisitor *visitor)
{
  Network *network = this->network();
  Graph *graph = this->graph();
  Search *search = this->search();
  if (exceptionToEmpty(to))
    makeGroupPathEnds(search->endpoints(), corner, min_max, visitor);
  else {
    // Only visit -to filter pins.
    VertexSet endpoints;
    PinSet pins;
    to->allPins(network, &pins);
    PinSet::Iterator pin_iter(pins);
    while (pin_iter.hasNext()) {
      const Pin *pin = pin_iter.next();
      Vertex *vertex, *bidirect_drvr_vertex;
      graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
      if (vertex
	  && search->isEndpoint(vertex))
	endpoints.insert(vertex);
      if (bidirect_drvr_vertex
	  && search->isEndpoint(bidirect_drvr_vertex))
	endpoints.insert(bidirect_drvr_vertex);
    }
    makeGroupPathEnds(&endpoints, corner, min_max, visitor);
  }
}

static bool
exceptionToEmpty(ExceptionTo *to)
{
  return to == nullptr
    || (to->pins() == nullptr
	&& to->instances() == nullptr);
}

////////////////////////////////////////////////////////////////

class MakeEndpointPathEnds : public VertexVisitor
{
public:
  MakeEndpointPathEnds(PathEndVisitor *path_end_visitor,
		       const Corner *corner,
		       const MinMaxAll *min_max,
		       const StaState *sta);
  ~MakeEndpointPathEnds();
  virtual VertexVisitor *copy();
  virtual void visit(Vertex *vertex);

private:
  DISALLOW_COPY_AND_ASSIGN(MakeEndpointPathEnds);

  VisitPathEnds *visit_path_ends_;
  PathEndVisitor *path_end_visitor_;
  const Corner *corner_;
  const MinMaxAll *min_max_;
  const StaState *sta_;
};

MakeEndpointPathEnds::MakeEndpointPathEnds(PathEndVisitor *path_end_visitor,
					   const Corner *corner,
					   const MinMaxAll *min_max,
					   const StaState *sta) :
  visit_path_ends_(new VisitPathEnds(sta)),
  path_end_visitor_(path_end_visitor->copy()),
  corner_(corner),
  min_max_(min_max),
  sta_(sta)
{
}

MakeEndpointPathEnds::~MakeEndpointPathEnds()
{
  delete visit_path_ends_;
  delete path_end_visitor_;
}

VertexVisitor *
MakeEndpointPathEnds::copy()
{
  return new MakeEndpointPathEnds(path_end_visitor_, corner_, min_max_, sta_);
}

void
MakeEndpointPathEnds::visit(Vertex *vertex)
{
  visit_path_ends_->visitPathEnds(vertex, corner_, min_max_, true,
				  path_end_visitor_);
}

////////////////////////////////////////////////////////////////

void
PathGroups::makeGroupPathEnds(VertexSet *endpoints,
			      const Corner *corner,
			      const MinMaxAll *min_max,
			      PathEndVisitor *visitor)
{
  Vector<MakeEndpointPathEnds*> visitors;
  for (int i = 0; i < thread_count_; i++)
    visitors.push_back(new MakeEndpointPathEnds(visitor, corner, min_max, this));
  for (auto endpoint : *endpoints) {
    dispatch_queue_->dispatch( [endpoint, &visitors](int i)
			       { visitors[i]->visit(endpoint); } );
  }
  dispatch_queue_->finishTasks();
  visitors.deleteContents();
}

} // namespace
