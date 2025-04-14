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

#include "Vector.hh"
#include "Map.hh"
#include "Iterator.hh"
#include "MinMax.hh"
#include "Transition.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"
#include "Tag.hh"

namespace sta {

class TagGroupBldr;

class TagGroup
{
public:
  TagGroup(TagGroupIndex index,
	   PathIndexMap *path_index_map,
	   bool has_clk_tag,
	   bool has_genclk_src_tag,
	   bool has_filter_tag,
	   bool has_loop_tag);
  // For Search::findTagGroup to probe.
  TagGroup(TagGroupBldr *tag_bldr);
  ~TagGroup();
  TagGroupIndex index() const { return index_; }
  size_t hash() const { return hash_; }
  void report(const StaState *sta) const;
  void reportArrivalMap(const StaState *sta) const;
  bool hasClkTag() const { return has_clk_tag_; }
  bool hasGenClkSrcTag() const { return has_genclk_src_tag_; }
  bool hasFilterTag() const { return has_filter_tag_; }
  bool hasLoopTag() const { return has_loop_tag_; }
  bool ownPathMap() const { return own_path_map_; }
  size_t pathCount() const { return path_index_map_->size(); }
  void pathIndex(Tag *tag,
                 size_t &path_index,
                 bool &exists) const;
  size_t pathIndex(Tag *tag) const;
  PathIndexMap *pathIndexMap() const { return path_index_map_; }
  bool hasTag(Tag *tag) const;

protected:
  static size_t pathIndexMapHash(PathIndexMap *path_index_map);

  // tag -> path index
  PathIndexMap *path_index_map_;
  size_t hash_;
  unsigned int index_:tag_group_index_bits;
  bool has_clk_tag_:1;
  bool has_genclk_src_tag_:1;
  bool has_filter_tag_:1;
  bool has_loop_tag_:1;
  bool own_path_map_:1;
};

class TagGroupHash
{
public:
  size_t operator()(const TagGroup *tag) const;
};

class TagGroupEqual
{
public:
  bool operator()(const TagGroup *group1,
		  const TagGroup *group2) const;
};

// Incremental tag group used to build tag group and associated
// arrivals.
class TagGroupBldr
{
public:
  TagGroupBldr(bool match_crpr_clk_pin,
	       const StaState *sta);
  void init(Vertex *vertex);
  bool empty();
  void reportArrivalEntries() const;
  TagGroup *makeTagGroup(TagGroupIndex index,
			 const StaState *sta);
  size_t pathCount() const { return path_index_map_.size();; }
  bool hasClkTag() const { return has_clk_tag_; }
  bool hasGenClkSrcTag() const { return has_genclk_src_tag_; }
  bool hasFilterTag() const { return has_filter_tag_; }
  bool hasLoopTag() const { return has_loop_tag_; }
  bool hasPropagatedClk() const { return has_propagated_clk_; }
  Path *tagMatchPath(Tag *tag);
  void tagMatchPath(Tag *tag,
                    // Return values.
                    Path *&match,
                    size_t &path_index);
  Arrival arrival(size_t path_index) const;
  // prev_path == hull
  void setArrival(Tag *tag,
		  const Arrival &arrival);
  void setMatchPath(Path *match,
                    size_t path_index,
                    Tag *tag,
                    Arrival arrival,
                    Path *prev_path,
                    Edge *prev_edge,
                    TimingArc *prev_arc);
  void insertPath(Tag *tag,
                  Arrival arrival,
                  Path *prev_path,
                  Edge *prev_edge,
                  TimingArc *prev_arc);
  void insertPath(const Path &path);
  PathIndexMap &pathIndexMap() { return path_index_map_; }
  void copyPaths(TagGroup *tag_group,
                 Path *paths);

protected:
  int tagMatchIndex();
  PathIndexMap *makePathIndexMap(const StaState *sta);

  Vertex *vertex_;
  int default_path_count_;
  PathIndexMap path_index_map_;
  std::vector<Path>  paths_;
  bool has_clk_tag_;
  bool has_genclk_src_tag_;
  bool has_filter_tag_;
  bool has_loop_tag_;
  bool has_propagated_clk_;
  const StaState *sta_;
};

void
pathIndexMapReport(const PathIndexMap *path_index_map,
                   const StaState *sta);

} // namespace
