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

#include "Machine.hh"
#include "Report.hh"
#include "Debug.hh"
#include "Graph.hh"
#include "PathAnalysisPt.hh"
#include "ClkInfo.hh"
#include "Tag.hh"
#include "Corner.hh"
#include "Search.hh"
#include "PathVertexRep.hh"
#include "TagGroup.hh"

namespace sta {

TagGroup::TagGroup(TagGroupIndex index,
		   ArrivalMap *arrival_map,
		   bool has_clk_tag,
		   bool has_genclk_src_tag,
		   bool has_filter_tag,
		   bool has_loop_tag) :
  arrival_map_(arrival_map),
  hash_(arrivalMapHash(arrival_map_)),
  index_(index),
  has_clk_tag_(has_clk_tag),
  has_genclk_src_tag_(has_genclk_src_tag),
  has_filter_tag_(has_filter_tag),
  has_loop_tag_(has_loop_tag),
  own_arrival_map_(true)
{
}

TagGroup::TagGroup(TagGroupBldr *tag_bldr) :
  arrival_map_(tag_bldr->arrivalMap()),
  hash_(arrivalMapHash(arrival_map_)),
  own_arrival_map_(false)
{
}

TagGroup::~TagGroup()
{
  if (own_arrival_map_)
    delete arrival_map_;
}

size_t
TagGroup::arrivalMapHash(ArrivalMap *arrival_map)
{
  size_t hash = 0;
  ArrivalMap::Iterator arrival_iter(arrival_map);
  while (arrival_iter.hasNext()) {
    Tag *tag;
    int arrival_index;
    arrival_iter.next(tag, arrival_index);
    hash += tag->hash();
  }
  return hash;
}

bool
TagGroup::hasTag(Tag *tag) const
{
  return arrival_map_->hasKey(tag);
}

void
TagGroup::arrivalIndex(Tag *tag,
		       int &arrival_index,
		       bool &exists) const
{
  arrival_map_->findKey(tag, arrival_index, exists);
}

void
TagGroup::requiredIndex(Tag *tag,
			int &req_index,
			bool &exists) const
{
  arrivalIndex(tag, req_index, exists);
  if (exists)
    req_index += arrivalCount();
}

int
TagGroup::requiredIndex(int arrival_index) const
{
  return arrival_index + arrivalCount();
}

void
TagGroup::report(const StaState *sta) const
{
  Report *report = sta->report();
  report->print("Group %u hash = %u\n", index_, hash_);
  arrivalMapReport(arrival_map_, sta);
}

void
TagGroup::reportArrivalMap(const StaState *sta) const
{
  arrivalMapReport(arrival_map_, sta);
}

void
arrivalMapReport(const ArrivalMap *arrival_map,
		 const StaState *sta)
{
  Report *report = sta->report();
  ArrivalMap::ConstIterator arrival_iter(arrival_map);
  while (arrival_iter.hasNext()) {
    Tag *tag;
    int arrival_index;
    arrival_iter.next(tag, arrival_index);
    report->print(" %2u %s\n",
		  arrival_index,
		  tag->asString(sta));
  }
  report->print("\n");
}

////////////////////////////////////////////////////////////////

TagGroupBldr::TagGroupBldr(bool match_crpr_clk_pin,
			   const StaState *sta) :
  default_arrival_count_(sta->corners()->count()
			 * RiseFall::index_count
			 * MinMax::index_count),
  arrival_map_(default_arrival_count_,
	       TagMatchHash(match_crpr_clk_pin, sta),
	       TagMatchEqual(match_crpr_clk_pin, sta)),
  arrivals_(default_arrival_count_),
  prev_paths_(default_arrival_count_),
  has_clk_tag_(false),
  has_genclk_src_tag_(false),
  has_filter_tag_(false),
  has_loop_tag_(false),
  sta_(sta)
{
}

bool
TagGroupBldr::empty()
{
  return arrival_map_.empty();
}

void
TagGroupBldr::init(Vertex *vertex)
{
  vertex_ = vertex;
  arrival_map_.clear();
  arrivals_.clear();
  prev_paths_.clear();
  has_clk_tag_ = false;
  has_genclk_src_tag_ = false;
  has_filter_tag_ = false;
  has_loop_tag_ = false;
}

void
TagGroupBldr::reportArrivalEntries() const
{
  arrivalMapReport(&arrival_map_, sta_);
}

void
TagGroupBldr::tagArrival(Tag *tag,
			 // Return values.
			 Arrival &arrival,
			 int &arrival_index,
			 bool &exists) const
{
  arrival_map_.findKey(tag, arrival_index, exists);
  if (exists)
    arrival = arrivals_[arrival_index];
}

void
TagGroupBldr::tagArrival(Tag *tag,
			 // Return values.
			 Arrival &arrival,
			 bool &exists) const
{
  int arrival_index;
  arrival_map_.findKey(tag, arrival_index, exists);
  if (exists)
    arrival = arrivals_[arrival_index];
}

void
TagGroupBldr::tagMatchArrival(Tag *tag,
			      // Return values.
			      Tag *&tag_match,
			      Arrival &arrival,
			      int &arrival_index) const
{
  // Find matching group tag.
  // Match is not necessarily equal to original tag because it
  // must only satisfy tagMatch.
  bool exists;
  arrival_map_.findKey(tag, tag_match, arrival_index, exists);
  if (exists)
    arrival = arrivals_[arrival_index];
  else {
    tag_match = nullptr;
    arrival = -1.0;
    arrival_index = -1;
  }
}

Arrival 
TagGroupBldr::arrival(int arrival_index) const
{
  return arrivals_[arrival_index];
}

void
TagGroupBldr::setArrival(Tag *tag,
			 const Arrival &arrival,
			 PathVertexRep *prev_path)
{
  Tag *tag_match;
  Arrival ignore;
  int arrival_index;
  // Find matching group tag (not necessarily equal to original tag).
  tagMatchArrival(tag, tag_match, ignore, arrival_index);
  setMatchArrival(tag, tag_match, arrival, arrival_index, prev_path);
}

void
TagGroupBldr::setMatchArrival(Tag *tag,
			      Tag *tag_match,
			      const Arrival &arrival,
			      int arrival_index,
			      PathVertexRep *prev_path)
{
  if (tag_match) {
    // If the group_tag exists there has to be an arrival map entry for it.
    if (tag_match != tag) {
      // Replace tag in arrival map.
      arrival_map_.erase(tag_match);
      arrival_map_.insert(tag, arrival_index);
    }
    arrivals_[arrival_index] = arrival;
    prev_paths_[arrival_index].init(prev_path);
  }
  else {
    arrival_index = arrivals_.size();
    arrival_map_.insert(tag, arrival_index);
    arrivals_.push_back(arrival);
    if (prev_path)
      prev_paths_.push_back(*prev_path);
    else
      prev_paths_.push_back(PathVertexRep());

    if (tag->isClock())
      has_clk_tag_ = true;
    if (tag->isGenClkSrcPath())
      has_genclk_src_tag_ = true;
    if (tag->isFilter()
	|| tag->clkInfo()->refsFilter(sta_))
      has_filter_tag_ = true;
    if (tag->isLoop())
      has_loop_tag_ = true;
  }
}

void
TagGroupBldr::deleteArrival(Tag *tag)
{
  arrival_map_.erase(tag);
}

TagGroup *
TagGroupBldr::makeTagGroup(TagGroupIndex index,
			   const StaState *sta)
{
  return new TagGroup(index, makeArrivalMap(sta),
		      has_clk_tag_, has_genclk_src_tag_, has_filter_tag_,
		      has_loop_tag_);

}

ArrivalMap *
TagGroupBldr::makeArrivalMap(const StaState *sta)
{
  ArrivalMap *arrival_map = new ArrivalMap(arrival_map_.size(),
					   TagMatchHash(true, sta),
					   TagMatchEqual(true, sta));
  int arrival_index = 0;
  ArrivalMap::Iterator arrival_iter(arrival_map_);
  while (arrival_iter.hasNext()) {
    Tag *tag;
    int arrival_index1;
    arrival_iter.next(tag, arrival_index1);
    arrival_map->insert(tag, arrival_index);
    arrival_index++;
  }
  return arrival_map;
}

void
TagGroupBldr::copyArrivals(TagGroup *tag_group,
			   Arrival *arrivals,
			   PathVertexRep *prev_paths)
{
  ArrivalMap::Iterator arrival_iter1(arrival_map_);
  while (arrival_iter1.hasNext()) {
    Tag *tag1;
    int arrival_index1, arrival_index2;
    arrival_iter1.next(tag1, arrival_index1);
    bool exists2;
    tag_group->arrivalIndex(tag1, arrival_index2, exists2);
    if (!exists2)
      internalError("tag group missing tag");
    arrivals[arrival_index2] = arrivals_[arrival_index1];
    if (prev_paths) {
      PathVertexRep *prev_path = &prev_paths_[arrival_index1];
      prev_paths[arrival_index2].init(prev_path);
    }
  }
}

////////////////////////////////////////////////////////////////

size_t
TagGroupHash::operator()(const TagGroup *group) const
{
  return group->hash();
}

static bool
arrivalMapEqual(const ArrivalMap *arrival_map1,
		const ArrivalMap *arrival_map2)
{
  int arrival_count1 = arrival_map1->size();
  int arrival_count2 = arrival_map2->size();
  if (arrival_count1 == arrival_count2) {
    ArrivalMap::ConstIterator arrival_iter1(arrival_map1);
    while (arrival_iter1.hasNext()) {
      Tag *tag1, *tag2;
      int arrival_index1, arrival_index2;
      arrival_iter1.next(tag1, arrival_index1);
      bool exists2;
      arrival_map2->findKey(tag1, tag2, arrival_index2, exists2);
      if (!exists2 
	  // ArrivalMap equal function is TagMatchEqual, so make sure
	  // the tag is an exact match.
	  || tag2 != tag1)
	return false;
    }
    return true;
  }
  else
    return false;
}

bool
TagGroupEqual::operator()(const TagGroup *tag_group1,
			  const TagGroup *tag_group2) const
{
  return tag_group1 == tag_group2
    || (tag_group1->hash() == tag_group2->hash()
	&& arrivalMapEqual(tag_group1->arrivalMap(),
			   tag_group2->arrivalMap()));
}

} // namespace
