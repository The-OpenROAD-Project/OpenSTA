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

#include "PathGroup.hh"

#include <algorithm>
#include <limits>

#include "Stats.hh"
#include "Debug.hh"
#include "Mutex.hh"
#include "Fuzzy.hh"
#include "MinMax.hh"
#include "DispatchQueue.hh"
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

namespace sta {

size_t PathGroup::group_path_count_max = std::numeric_limits<size_t>::max();

PathGroup *
PathGroup::makePathGroupSlack(const char *name,
			      int group_path_count,
			      int endpoint_path_count,
			      bool unique_pins,
			      float slack_min,
			      float slack_max,
			      const StaState *sta)
{
  return new PathGroup(name, group_path_count, endpoint_path_count, unique_pins,
		       slack_min, slack_max, true, MinMax::min(), sta);
}

PathGroup *
PathGroup::makePathGroupArrival(const char *name,
				int group_path_count,
				int endpoint_path_count,
				bool unique_pins,
				const MinMax *min_max,
				const StaState *sta)
{
  return new PathGroup(name, group_path_count, endpoint_path_count, unique_pins,
		       0.0, 0.0, false, min_max, sta);
}

PathGroup::PathGroup(const char *name,
		     size_t group_path_count,
		     size_t endpoint_path_count,
		     bool unique_pins,
		     float slack_min,
		     float slack_max,
		     bool cmp_slack,
		     const MinMax *min_max,
		     const StaState *sta) :
  name_(name),
  group_path_count_(group_path_count),
  endpoint_path_count_(endpoint_path_count),
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
PathGroup::saveable(PathEnd *path_end)
{
  float threshold;
  {
    LockGuard lock(lock_);
    threshold = threshold_;
  }
  if (compare_slack_) {
    // Crpr increases the slack, so check the slack
    // without crpr first because it is expensive to find.
    Slack slack = path_end->slackNoCrpr(sta_);
    if (!delayIsInitValue(slack, min_max_)
 	&& delayLessEqual(slack, threshold, sta_)
 	&& delayLessEqual(slack, slack_max_, sta_)) {
      // Now check with crpr.
      slack = path_end->slack(sta_);
      return delayLessEqual(slack, threshold, sta_)
 	&& delayLessEqual(slack, slack_max_, sta_)
 	&& delayGreaterEqual(slack, slack_min_, sta_);
    }
  }
  else {
    const Arrival &arrival = path_end->dataArrivalTime(sta_);
    return !delayIsInitValue(arrival, min_max_)
      && delayGreaterEqual(arrival, threshold, min_max_, sta_);
  }
  return false;
}

// endpoint_path_count > 1 with slack_min requires
// saving endpoints with slack > slack_min so that
// path enumeration can find them. Use the path end
// with the min(max) delay to prune ends that cannot
// onion peel down to slack_min.
bool
PathGroup::enumMinSlackUnderMin(PathEnd *path_end)
{
  if (compare_slack_
      && endpoint_path_count_ > 1
      && slack_min_ > -INF) {
    const Path *path = path_end->path();
    PathAnalysisPt *other_ap = path->pathAnalysisPt(sta_)->tgtClkAnalysisPt();
    const Tag *tag = path->tag(sta_);
    VertexPathIterator other_iter(path->vertex(sta_),
                                  path->transition(sta_),
                                  other_ap, sta_);
    while (other_iter.hasNext()) {
      Path *other = other_iter.next();
      if (Tag::matchCrpr(other->tag(sta_), tag)) {
        PathEnd *end_min = path_end->copy();
        end_min->setPath(other);
        float slack = delayAsFloat(end_min->slackNoCrpr(sta_));
        bool slack_under = fuzzyGreater(slack, slack_min_);
        delete end_min;
        if (slack_under)
          return true;
      }
    }
  }
  return false;
}

void
PathGroup::insert(PathEnd *path_end)
{
  LockGuard lock(lock_);
  path_ends_.push_back(path_end);
  if (group_path_count_ != group_path_count_max
      && path_ends_.size() > group_path_count_ * 2)
    prune();
}

void
PathGroup::prune()
{
  sort();
  VertexPathCountMap path_counts;
  size_t end_count = 0;
  for (unsigned i = 0; i < path_ends_.size(); i++) {
    PathEnd *path_end = path_ends_[i];
    Vertex *vertex = path_end->vertex(sta_);
    // Squish up to endpoint_path_count path ends per vertex
    // up to the front of path_ends_.
    if (end_count < group_path_count_
	&& path_counts[vertex] < endpoint_path_count_) {
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
PathGroup::pushEnds(PathEndSeq &path_ends)
{
  ensureSortedMaxPaths();
  for (PathEnd *path_end : path_ends_)
    path_ends.push_back(path_end);
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
  if (path_ends_.size() > group_path_count_)
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
  LockGuard lock(lock_);
  threshold_ = min_max_->initValue();
  path_ends_.clear();
}

////////////////////////////////////////////////////////////////

const char *PathGroups::path_delay_group_name_ = "path delay";
const char *PathGroups::gated_clk_group_name_ = "gated clock";
const char *PathGroups::async_group_name_ = "asynchronous";
const char *PathGroups::unconstrained_group_name_ = "unconstrained";

PathGroups::PathGroups(int group_path_count,
		       int endpoint_path_count,
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
  group_path_count_(group_path_count),
  endpoint_path_count_(endpoint_path_count),
  unique_pins_(unique_pins),
  slack_min_(slack_min),
  slack_max_(slack_max)
{
  makeGroups(group_path_count, endpoint_path_count, unique_pins,
	     slack_min, slack_max, group_names,
	     setup, recovery, clk_gating_setup, unconstrained,
	     MinMax::max());
  makeGroups(group_path_count, endpoint_path_count, unique_pins,
	     slack_min, slack_max, group_names,
	     hold, removal, clk_gating_hold, unconstrained,
	     MinMax::min());
}

void
PathGroups::makeGroups(int group_path_count,
		       int endpoint_path_count,
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
    for (const auto [name, group] : sdc_->groupPaths()) {
      if (reportGroup(name, group_names)) {
	PathGroup *group = PathGroup::makePathGroupSlack(name,
							 group_path_count,
							 endpoint_path_count,
							 unique_pins,
							 slack_min, slack_max,
							 this);
	named_map_[mm_index][name] = group;
      }
    }

    for (auto clk : sdc_->clks()) {
      const char *clk_name = clk->name();
      if (reportGroup(clk_name, group_names)) {
	PathGroup *group = PathGroup::makePathGroupSlack(clk_name,
							 group_path_count,
							 endpoint_path_count,
							 unique_pins,
							 slack_min, slack_max,
							 this);
	clk_map_[mm_index][clk] = group;
      }
    }
  }

  if (setup_hold
      && reportGroup(path_delay_group_name_, group_names))
    path_delay_[mm_index] = PathGroup::makePathGroupSlack(path_delay_group_name_,
							  group_path_count,
							  endpoint_path_count,
							  unique_pins,
							  slack_min, slack_max,
							  this);
  else
    path_delay_[mm_index] = nullptr;

  if (gated_clk
      && reportGroup(gated_clk_group_name_, group_names))
    gated_clk_[mm_index] = PathGroup::makePathGroupSlack(gated_clk_group_name_,
							 group_path_count,
							 endpoint_path_count,
							 unique_pins,
							 slack_min, slack_max,
							 this);
  else
    gated_clk_[mm_index] = nullptr;

  if (async
      && reportGroup(async_group_name_, group_names))
    async_[mm_index] = PathGroup::makePathGroupSlack(async_group_name_,
						     group_path_count,
						     endpoint_path_count,
						     unique_pins,
						     slack_min, slack_max,
						     this);
  else
    async_[mm_index] = nullptr;

  if (unconstrained
      && reportGroup(unconstrained_group_name_, group_names))
    unconstrained_[mm_index] =
      PathGroup::makePathGroupArrival(unconstrained_group_name_,
				      group_path_count, endpoint_path_count,
				      unique_pins, min_max, this);
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
  GroupPath *group_path = groupPathTo(path_end, this);
  if (path_end->isUnconstrained())
    return unconstrained_[mm_index];
  // GroupPaths have precedence.
  else if (group_path) {
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
    const Clock *tgt_clk = path_end->targetClk(this);
    if (tgt_clk
	&& !path_delay->ignoreClkLatency())
      return findPathGroup(tgt_clk, min_max);
    else
      return path_delay_[mm_index];
  }
  else {
    report_->critical(1390, "unknown path end type");
    return nullptr;
  }
}

// Mirrors PathGroups::pathGroup.
std::string
PathGroups::pathGroupName(const PathEnd *path_end,
			  const StaState *sta)
{
  GroupPath *group_path = groupPathTo(path_end, sta);
  if (path_end->isUnconstrained())
    return unconstrained_group_name_;
  // GroupPaths have precedence.
  else if (group_path) {
   if (group_path->isDefault())
     return path_delay_group_name_;
   else
     return group_path->name();
  }
  else if (path_end->isCheck() || path_end->isLatchCheck()) {
    const TimingRole *check_role = path_end->checkRole(sta);
    const Clock *tgt_clk = path_end->targetClk(sta);
    if (check_role == TimingRole::removal()
	|| check_role == TimingRole::recovery())
      return async_group_name_;
    else
      return tgt_clk->name();
  }
  else if (path_end->isOutputDelay()
	   || path_end->isDataCheck())
    return path_end->targetClk(sta)->name();
  else if (path_end->isGatedClock())
    return gated_clk_group_name_;
  else if (path_end->isPathDelay()) {
    // Path delays that end at timing checks are part of the target clk group
    // unless -ignore_clock_latency is true.
    PathDelay *path_delay = path_end->pathDelay();
    const Clock *tgt_clk = path_end->targetClk(sta);
    if (tgt_clk
	&& !path_delay->ignoreClkLatency())
      return tgt_clk->name();
    else
      return path_delay_group_name_;
  }
  else {
    sta->report()->critical(1391, "unknown path end type");
    return nullptr;
  }
}

GroupPath *
PathGroups::groupPathTo(const PathEnd *path_end,
			const StaState *sta)
{
  const Path *path = path_end->path();
  const Pin *pin = path->pin(sta);
  ExceptionPath *exception = 
    sta->search()->exceptionTo(ExceptionPathType::group_path, path,
			       pin, path->transition(sta),
			       path_end->targetClkEdge(sta),
			       path->minMax(sta), false, false);
  return dynamic_cast<GroupPath*>(exception);
}

void
PathGroups::pushGroupPathEnds(PathEndSeq &path_ends)
{
  for (auto min_max : MinMax::range()) {
    int mm_index =  min_max->index();
    for (auto name_group : sdc_->groupPaths()) {
      const char *name = name_group.first;
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
PathGroups::pushUnconstrainedPathEnds(PathEndSeq &path_ends,
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

PathEndSeq
PathGroups::makePathEnds(ExceptionTo *to,
			 bool unconstrained_paths,
			 const Corner *corner,
			 const MinMaxAll *min_max,
			 bool sort_by_slack)
{
  Stats stats(debug_, report_);
  makeGroupPathEnds(to, group_path_count_, endpoint_path_count_, unique_pins_,
		    corner, min_max);

  PathEndSeq path_ends;
  pushGroupPathEnds(path_ends);
  if (sort_by_slack) {
    sort(path_ends, PathEndLess(this));
  }

  if (unconstrained_paths
      && path_ends.empty())
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
  MakePathEnds1(PathGroups *path_groups);
  MakePathEnds1(const MakePathEnds1&) = default;
  virtual PathEndVisitor *copy() const;
  virtual void visit(PathEnd *path_end);
  virtual void vertexEnd(Vertex *vertex);

private:
  void visitPathEnd(PathEnd *path_end,
		    PathGroup *group);

  PathGroups *path_groups_;
  PathGroupEndMap ends_;
  PathEndLess cmp_;
};

MakePathEnds1::MakePathEnds1(PathGroups *path_groups) :
  path_groups_(path_groups),
  cmp_(path_groups)
{
}

PathEndVisitor *
MakePathEnds1::copy() const
{
  return new MakePathEnds1(*this);
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
  if (group->saveable(path_end)) {
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
    // visitPathEnd already confirmed slack is saveable.
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
  MakePathEndsAll(int endpoint_path_count,
                  PathGroups *path_groups);
  MakePathEndsAll(const MakePathEndsAll&) = default;
  virtual ~MakePathEndsAll();
  virtual PathEndVisitor *copy() const;
  virtual void visit(PathEnd *path_end);
  virtual void vertexEnd(Vertex *vertex);

private:
  void visitPathEnd(PathEnd *path_end,
		    PathGroup *group);

  int endpoint_path_count_;
  PathGroups *path_groups_;
  const StaState *sta_;
  PathGroupEndsMap ends_;
  PathEndSlackLess slack_cmp_;
  PathEndNoCrprLess path_no_crpr_cmp_;
};

MakePathEndsAll::MakePathEndsAll(int endpoint_path_count,
				 PathGroups *path_groups) :
  endpoint_path_count_(endpoint_path_count),
  path_groups_(path_groups),
  sta_(path_groups),
  slack_cmp_(path_groups),
  path_no_crpr_cmp_(path_groups)
{
}


PathEndVisitor *
MakePathEndsAll::copy() const
{
  return new MakePathEndsAll(*this);
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
	     && n < endpoint_path_count_) {
	PathEnd *path_end = end_iter.next();
	// Only save the worst path end for each crpr tag.
	// PathEnum will peel the others.
	if (!unique_ends.hasKey(path_end)) {
	  debugPrint(debug, "path_group", 2, "insert %s %s %s %d",
                     path_end->vertex(sta_)->to_string(sta_).c_str(),
                     path_end->typeName(),
                     path_end->transition(sta_)->to_string().c_str(),
                     path_end->path()->tag(sta_)->index());
	  // Give the group a copy of the path end because
	  // it may delete it during pruning.
	  if (group->saveable(path_end)
              || group->enumMinSlackUnderMin(path_end)) {
	    group->insert(path_end->copy());
	    unique_ends.insert(path_end);
	    n++;
	  }
	}
	else
	  debugPrint(debug, "path_group", 3, "prune %s %s %s %d",
                     path_end->vertex(sta_)->to_string(sta_).c_str(),
                     path_end->typeName(),
                     path_end->transition(sta_)->to_string().c_str(),
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
			      int group_path_count,
			      int endpoint_path_count,
			      bool unique_pins,
			      const Corner *corner,
			      const MinMaxAll *min_max)
{
  if (endpoint_path_count == 1) {
    MakePathEnds1 make_path_ends(this);
    makeGroupPathEnds(to, corner, min_max, &make_path_ends);
  }
  else {
    MakePathEndsAll make_path_ends(endpoint_path_count, this);
    makeGroupPathEnds(to, corner, min_max, &make_path_ends);

    for (auto path_min_max : MinMax::range()) {
      int mm_index =  path_min_max->index();
      for (auto name_group : sdc_->groupPaths()) {
        const char *name = name_group.first;
        PathGroup *group = findPathGroup(name, path_min_max);
        if (group)
          enumPathEnds(group, group_path_count, endpoint_path_count, unique_pins, true);
      }

      for (auto clk : sdc_->clks()) {
	PathGroup *group = findPathGroup(clk, path_min_max);
	if (group)
	  enumPathEnds(group, group_path_count, endpoint_path_count, unique_pins, true);
      }

      PathGroup *group = unconstrained_[mm_index];
      if (group)
	enumPathEnds(group, group_path_count, endpoint_path_count, unique_pins, false);
      group = path_delay_[mm_index];
      if (group)
	enumPathEnds(group, group_path_count, endpoint_path_count, unique_pins, true);
      group = gated_clk_[mm_index];
      if (group)
	enumPathEnds(group, group_path_count, endpoint_path_count, unique_pins, true);
      group = async_[mm_index];
      if (group)
	enumPathEnds(group, group_path_count, endpoint_path_count, unique_pins, true);
    }
  }
}

void
PathGroups::enumPathEnds(PathGroup *group,
			 int group_path_count,
			 int endpoint_path_count,
			 bool unique_pins,
			 bool cmp_slack)
{
  // Insert the worst max_path path ends in the group into a path
  // enumerator.
  PathEnum path_enum(group_path_count, endpoint_path_count,
		     unique_pins, cmp_slack, this);
  PathGroupIterator *end_iter = group->iterator();
  while (end_iter->hasNext()) {
    PathEnd *end = end_iter->next();
    if (group->saveable(end)
        || group->enumMinSlackUnderMin(end))
      path_enum.insert(end);
  }
  delete end_iter;
  group->clear();

  // Parallel path enumeratation to find the endpoint_path_count/max path ends.
  for (int n = 0; path_enum.hasNext() && n < group_path_count; n++) {
    PathEnd *end = path_enum.next();
    if (group->saveable(end))
      group->insert(end);
    else
      delete end;
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
    VertexSet endpoints(graph_);
    PinSet pins = to->allPins(network);
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
  MakeEndpointPathEnds(const MakeEndpointPathEnds &make_path_ends);
  ~MakeEndpointPathEnds();
  virtual VertexVisitor *copy() const;
  virtual void visit(Vertex *vertex);

private:
  VisitPathEnds visit_path_ends_;
  PathEndVisitor *path_end_visitor_;
  const Corner *corner_;
  const MinMaxAll *min_max_;
  const StaState *sta_;
};

MakeEndpointPathEnds::MakeEndpointPathEnds(PathEndVisitor *path_end_visitor,
					   const Corner *corner,
					   const MinMaxAll *min_max,
					   const StaState *sta) :
  visit_path_ends_(sta),
  path_end_visitor_(path_end_visitor->copy()),
  corner_(corner),
  min_max_(min_max),
  sta_(sta)
{
}

MakeEndpointPathEnds::MakeEndpointPathEnds(const MakeEndpointPathEnds &make_path_ends) :
  visit_path_ends_(make_path_ends.sta_),
  path_end_visitor_(make_path_ends.path_end_visitor_->copy()),
  corner_(make_path_ends.corner_),
  min_max_(make_path_ends.min_max_),
  sta_(make_path_ends.sta_)
{
}

MakeEndpointPathEnds::~MakeEndpointPathEnds()
{
  delete path_end_visitor_;
}

VertexVisitor *
MakeEndpointPathEnds::copy() const
{
  return new MakeEndpointPathEnds(path_end_visitor_, corner_, min_max_, sta_);
}

void
MakeEndpointPathEnds::visit(Vertex *vertex)
{
  visit_path_ends_.visitPathEnds(vertex, corner_, min_max_, true, path_end_visitor_);
}

////////////////////////////////////////////////////////////////

void
PathGroups::makeGroupPathEnds(VertexSet *endpoints,
			      const Corner *corner,
			      const MinMaxAll *min_max,
			      PathEndVisitor *visitor)
{
  if (thread_count_ == 1) {
    MakeEndpointPathEnds end_visitor(visitor, corner, min_max, this);
    for (auto endpoint : *endpoints)
      end_visitor.visit(endpoint);
  }
  else {
    Vector<MakeEndpointPathEnds> visitors(thread_count_,
                                          MakeEndpointPathEnds(visitor, corner,
                                                               min_max, this));
    for (const auto endpoint : *endpoints) {
      dispatch_queue_->dispatch( [endpoint, &visitors](int i)
      { visitors[i].visit(endpoint); } );
    }
    dispatch_queue_->finishTasks();
  }
}

} // namespace
