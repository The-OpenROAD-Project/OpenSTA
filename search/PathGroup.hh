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

#pragma once

#include "DisallowCopyAssign.hh"
#include "Map.hh"
#include "Vector.hh"
#include "StaState.hh"
#include "SearchClass.hh"

namespace sta {

class MinMax;
class PathEndVisitor;

typedef PathEndSeq::Iterator PathGroupIterator;
typedef Map<const Clock*, PathGroup*> PathGroupClkMap;
typedef Map<const char*, PathGroup*, CharPtrLess> PathGroupNamedMap;

// A collection of PathEnds grouped and sorted for reporting.
class PathGroup
{
public:
  virtual ~PathGroup();
  // Path group that compares compare slacks.
  static PathGroup *makePathGroupArrival(const char *name,
					 int group_count,
					 int endpoint_count,
					 bool unique_pins,
					 const MinMax *min_max,
					 const StaState *sta);
  // Path group that compares arrival time, sorted by min_max.
  static PathGroup *makePathGroupSlack(const char *name,
				       int group_count,
				       int endpoint_count,
				       bool unique_pins,
				       float min_slack,
				       float max_slack,
				       const StaState *sta);
  const char *name() const { return name_; }
  const MinMax *minMax() const { return min_max_;}
  const PathEndSeq &pathEnds() const { return path_ends_; }
  void insert(PathEnd *path_end);
  // Push group_count into path_ends.
  void pushEnds(PathEndSeq *path_ends);
  // Predicates to determine if a PathEnd is worth saving.
  virtual bool savable(PathEnd *path_end);
  int maxPaths() const { return group_count_; }
  PathGroupIterator *iterator();
  // This does NOT delete the path ends.
  void clear();
  static int group_count_max;
  
protected:
  PathGroup(const char *name,
	    int group_count,
	    int endpoint_count,
	    bool unique_pins,
	    float min_slack,
	    float max_slack,
	    bool cmp_slack,
	    const MinMax *min_max,
	    const StaState *sta);
  void ensureSortedMaxPaths();
  void prune();
  void sort();

  const char *name_;
  int group_count_;
  int endpoint_count_;
  bool unique_pins_;
  float slack_min_;
  float slack_max_;
  PathEndSeq path_ends_;
  const MinMax *min_max_;
  bool compare_slack_;
  float threshold_;
  std::mutex lock_;
  const StaState *sta_;

private:
  DISALLOW_COPY_AND_ASSIGN(PathGroup);
};

class PathGroups : public StaState
{
public:
  PathGroups(int group_count,
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
	     const StaState *sta);
  ~PathGroups();
  // Use corner nullptr to make PathEnds for all corners.
  // Returned PathEndSeq is owned by the caller.
  // The PathEnds in the vector are owned by the PathGroups.
  PathEndSeq *makePathEnds(ExceptionTo *to,
			   bool unconstrained_paths,
			   const Corner *corner,
			   const MinMaxAll *min_max,
			   bool sort_by_slack);
  PathGroup *findPathGroup(const char *name,
			   const MinMax *min_max) const;
  PathGroup *findPathGroup(const Clock *clock,
			   const MinMax *min_max) const;
  PathGroup *pathGroup(const PathEnd *path_end) const;
  static bool isGroupPathName(const char *group_name);
  static const char *asyncPathGroupName() { return async_group_name_; }

protected:
  void makeGroupPathEnds(ExceptionTo *to,
			 int group_count,
			 int endpoint_count,
			 bool unique_pins,
			 const Corner *corner,
			 const MinMaxAll *min_max);
  void makeGroupPathEnds(ExceptionTo *to,
			 const Corner *corner,
			 const MinMaxAll *min_max,
			 PathEndVisitor *visitor);
  void makeGroupPathEnds(VertexSet *endpoints,
			 const Corner *corner,
			 const MinMaxAll *min_max,
			 PathEndVisitor *visitor);
  void enumPathEnds(PathGroup *group,
		    int group_count,
		    int endpoint_count,
		    bool unique_pins,
		    bool cmp_slack);

  void pushGroupPathEnds(PathEndSeq *path_ends);
  void pushUnconstrainedPathEnds(PathEndSeq *path_ends,
				 const MinMaxAll *min_max);

  void makeGroups(int group_count,
		  int endpoint_count,
		  bool unique_pins,
		  float slack_min,
		  float slack_max,
		  PathGroupNameSet *group_names,
		  bool setup_hold,
		  bool async,
		  bool gated_clk,
		  bool unconstrained,
		  const MinMax *min_max);
  bool reportGroup(const char *group_name,
		   PathGroupNameSet *group_names) const;
  GroupPath *groupPathTo(const PathEnd *path_end) const;

  int group_count_;
  int endpoint_count_;
  bool unique_pins_;
  float slack_min_;
  float slack_max_;

  // Paths grouped by SDC group_path command.
  // name -> PathGroup
  PathGroupNamedMap named_map_[MinMax::index_count];
  // clock -> PathGroup
  PathGroupClkMap clk_map_[MinMax::index_count];
  // Min/max path delays.
  PathGroup *path_delay_[MinMax::index_count];
  // Gated clock checks.
  PathGroup *gated_clk_[MinMax::index_count];
  // Asynchronous (recovery/removal) checks.
  PathGroup *async_[MinMax::index_count];
  // Unconstrained paths.
  PathGroup *unconstrained_[MinMax::index_count];

  static const char *path_delay_group_name_;
  static const char *gated_clk_group_name_;
  static const char *async_group_name_;
  static const char *unconstrained_group_name_;

private:
  DISALLOW_COPY_AND_ASSIGN(PathGroups);
};

} // namespace
