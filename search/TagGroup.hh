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

typedef Vector<PathVertexRep> PathVertexRepSeq;

class TagGroup
{
public:
  TagGroup(TagGroupIndex index,
	   ArrivalMap *arrival_map,
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
  bool ownArrivalMap() const { return own_arrival_map_; }
  int arrivalCount() const { return arrival_map_->size(); }
  void arrivalIndex(Tag *tag,
		    int &arrival_index,
		    bool &exists) const;
  void requiredIndex(Tag *tag,
		     int &req_index,
		     bool &exists) const;
  int requiredIndex(int arrival_index) const;
  ArrivalMap *arrivalMap() const { return arrival_map_; }
  bool hasTag(Tag *tag) const;

protected:
  size_t arrivalMapHash(ArrivalMap *arrival_map);

  // tag -> arrival index
  ArrivalMap *arrival_map_;
  size_t hash_;
  unsigned int index_:tag_group_index_bits;
  unsigned int has_clk_tag_:1;
  unsigned int has_genclk_src_tag_:1;
  unsigned int has_filter_tag_:1;
  unsigned int has_loop_tag_:1;
  unsigned int own_arrival_map_:1;

private:
  DISALLOW_COPY_AND_ASSIGN(TagGroup);
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
  bool hasClkTag() const { return has_clk_tag_; }
  bool hasGenClkSrcTag() const { return has_genclk_src_tag_; }
  bool hasFilterTag() const { return has_filter_tag_; }
  bool hasLoopTag() const { return has_loop_tag_; }
  void deleteArrival(Tag *tag);
  void tagArrival(Tag *tag,
		  // Return values.
		  Arrival &arrival,
		  bool &exists) const;
  void tagArrival(Tag *tag,
		  // Return values.
		  Arrival &arrival,
		  int &arrival_index,
		  bool &exists) const;
  void tagMatchArrival(Tag *tag,
		       // Return values.
		       Tag *&tag_match,
		       Arrival &arrival,
		       int &arrival_index) const;
  Arrival arrival(int arrival_index) const;
  void setArrival(Tag *tag,
		  const Arrival &arrival,
		  PathVertexRep *prev_path);
  void setMatchArrival(Tag *tag,
		       Tag *tag_match,
		       const Arrival &arrival,
		       int arrival_index,
		       PathVertexRep *prev_path);
  ArrivalMap *arrivalMap() { return &arrival_map_; }
  void copyArrivals(TagGroup *tag_group,
		    Arrival *arrivals,
		    PathVertexRep *prev_paths);

protected:
  int tagMatchIndex();
  ArrivalMap *makeArrivalMap(const StaState *sta);

  Vertex *vertex_;
  int default_arrival_count_;
  ArrivalMap arrival_map_;
  ArrivalSeq arrivals_;
  PathVertexRepSeq prev_paths_;
  bool has_clk_tag_;
  bool has_genclk_src_tag_:1;
  bool has_filter_tag_;
  bool has_loop_tag_;
  const StaState *sta_;

private:
  DISALLOW_COPY_AND_ASSIGN(TagGroupBldr);
};

void
arrivalMapReport(const ArrivalMap *arrival_map,
		 const StaState *sta);

} // namespace
