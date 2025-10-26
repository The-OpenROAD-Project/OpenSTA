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

#pragma once

#include <mutex>

#include "Map.hh"
#include "Vector.hh"
#include "SdcClass.hh"
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
  ~PathGroup();
  // Path group that compares compare slacks.
  static PathGroup *makePathGroupArrival(const char *name,
					 int group_path_count,
					 int endpoint_path_count,
					 bool unique_pins,
					 const MinMax *min_max,
					 const StaState *sta);
  // Path group that compares arrival time, sorted by min_max.
  static PathGroup *makePathGroupSlack(const char *name,
				       int group_path_count,
				       int endpoint_path_count,
				       bool unique_pins,
				       float min_slack,
				       float max_slack,
				       const StaState *sta);
  const char *name() const { return name_; }
  const MinMax *minMax() const { return min_max_;}
  const PathEndSeq &pathEnds() const { return path_ends_; }
  void insert(PathEnd *path_end);
  // Push group_path_count into path_ends.
  void pushEnds(PathEndSeq &path_ends);
  // Predicate to determine if a PathEnd is worth saving.
  bool saveable(PathEnd *path_end);
  bool enumMinSlackUnderMin(PathEnd *path_end);
  int maxPaths() const { return group_path_count_; }
  PathGroupIterator *iterator();
  // This does NOT delete the path ends.
  void clear();
  static size_t group_path_count_max;
  
protected:
  PathGroup(const char *name,
	    size_t group_path_count,
	    size_t endpoint_path_count,
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
  size_t group_path_count_;
  size_t endpoint_path_count_;
  bool unique_pins_;
  float slack_min_;
  float slack_max_;
  PathEndSeq path_ends_;
  const MinMax *min_max_;
  bool compare_slack_;
  float threshold_;
  std::mutex lock_;
  const StaState *sta_;
};

class PathGroups : public StaState
{
public:
  PathGroups(int group_path_count,
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
	     const StaState *sta);
  ~PathGroups();
  // Use corner nullptr to make PathEnds for all corners.
  // The PathEnds in the vector are owned by the PathGroups.
  PathEndSeq makePathEnds(ExceptionTo *to,
                          bool unconstrained_paths,
                          const Corner *corner,
                          const MinMaxAll *min_max,
                          bool sort_by_slack);
  PathGroup *findPathGroup(const char *name,
			   const MinMax *min_max) const;
  PathGroup *findPathGroup(const Clock *clock,
			   const MinMax *min_max) const;
  PathGroup *pathGroup(const PathEnd *path_end) const;
  static std::string pathGroupName(const PathEnd *path_end,
				   const StaState *sta);
  static const char *asyncPathGroupName() { return async_group_name_; }
  static const char *pathDelayGroupName() { return  path_delay_group_name_; }
  static const char *gatedClkGroupName() { return gated_clk_group_name_; }
  static const char *unconstrainedGroupName() { return unconstrained_group_name_; }

protected:
  void makeGroupPathEnds(ExceptionTo *to,
			 int group_path_count,
			 int endpoint_path_count,
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
		    int group_path_count,
		    int endpoint_path_count,
		    bool unique_pins,
		    bool cmp_slack);

  void pushGroupPathEnds(PathEndSeq &path_ends);
  void pushUnconstrainedPathEnds(PathEndSeq &path_ends,
				 const MinMaxAll *min_max);

  void makeGroups(int group_path_count,
		  int endpoint_path_count,
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
  static GroupPath *groupPathTo(const PathEnd *path_end,
				const StaState *sta);

  int group_path_count_;
  int endpoint_path_count_;
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
};

} // namespace
