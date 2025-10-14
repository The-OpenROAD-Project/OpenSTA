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

#include "Tag.hh"

#include "Report.hh"
#include "Network.hh"
#include "Clock.hh"
#include "PortDelay.hh"
#include "ExceptionPath.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "Search.hh"
#include "PathAnalysisPt.hh"
#include "ClkInfo.hh"

namespace sta {

Tag::Tag(TagIndex index,
	 int rf_index,
	 PathAPIndex path_ap_index,
	 const ClkInfo *clk_info,
	 bool is_clk,
	 InputDelay *input_delay,
	 bool is_segment_start,
	 ExceptionStateSet *states,
	 bool own_states,
	 const StaState *sta) :
  clk_info_(clk_info),
  input_delay_(input_delay),
  states_(states),
  index_(index),
  is_clk_(is_clk),
  is_filter_(false),
  is_loop_(false),
  is_segment_start_(is_segment_start),
  own_states_(own_states),
  rf_index_(rf_index),
  path_ap_index_(path_ap_index)
{
  findHash();
  if (states_) {
    FilterPath *filter = sta->search()->filter();
    for (ExceptionState *state : *states_) {
      ExceptionPath *exception = state->exception();
      if (exception->isLoop())
	is_loop_ = true;
      if (exception == filter)
	is_filter_ = true;
    }
  }
}

Tag::~Tag()
{
  if (own_states_ && states_)
    delete states_;
}

std::string
Tag::to_string(const StaState *sta) const
{
  return to_string(true, true, sta);
}

std::string
Tag::to_string(bool report_index,
               bool report_rf_min_max,
               const StaState *sta) const
{
  const Network *network = sta->network();
  const Corners *corners = sta->corners();
  std::string result;

  if (report_index)
    result += std::to_string(index_);

  if (report_rf_min_max) {
    const RiseFall *rf = transition();
    PathAnalysisPt *path_ap = corners->findPathAnalysisPt(path_ap_index_);
    result += " ";
    result += rf->to_string().c_str();
    result += " ";
    result += path_ap->pathMinMax()->to_string();
    result += "/";
    result += std::to_string(path_ap_index_);
  }

  result += " ";
  const ClockEdge *clk_edge = clkEdge();
  if (clk_edge)
    result += clk_edge->name();
  else
    result += "unclocked";

  bool is_genclk_src = clk_info_->isGenClkSrcPath();
  if (is_clk_ || is_genclk_src) {
    result += " (";
    if (is_clk_) {
      result += "clock";
      if (clk_info_->isPropagated())
	result += " prop";
      else
	result += " ideal";
      if (is_genclk_src)
	result += " ";
    }
    if (clk_info_->isGenClkSrcPath())
      result += "genclk";
    result += ")";
  }

  const Pin *clk_src = clkSrc();
  if (clk_src) {
    result += " clk_src ";
    result += network->pathName(clk_src);
  }

  result += " crpr_pin ";
  const Path *crpr_clk_path = clk_info_->crprClkPath(sta);
  if (crpr_clk_path) {
    result += network->pathName(crpr_clk_path->pin(sta));
    result += " ";
    result += crpr_clk_path->minMax(sta)->to_string();
  }
  else
    result += "null";

  if (input_delay_) {
    result += " input ";
    result += network->pathName(input_delay_->pin());
  }

  if (is_segment_start_)
    result += " segment_start";

  if (states_) {
    for (ExceptionState *state : *states_) {
      ExceptionPath *exception = state->exception();
      result += " ";
      result += exception->asString(network);
      if (state->nextThru()) {
	result += " (next thru ";
	result += state->nextThru()->asString(network);
	result += ")";
      }
      else {
	if (exception->thrus() != nullptr)
	  result += " (thrus complete)";
      }
    }
  }
  return result;
}

const RiseFall *
Tag::transition() const
{
  return RiseFall::find(rf_index_);
}

PathAnalysisPt *
Tag::pathAnalysisPt(const StaState *sta) const
{
  const Corners *corners = sta->corners();
  return corners->findPathAnalysisPt(path_ap_index_);
}

void
Tag::setStates(ExceptionStateSet *states)
{
  states_ = states;
}

const ClockEdge *
Tag::clkEdge() const
{
  return clk_info_->clkEdge();
}

const Clock *
Tag::clock() const
{
  return clk_info_->clock();
}

const Pin *
Tag::clkSrc() const
{
  return clk_info_->clkSrc();
}

bool
Tag::isGenClkSrcPath() const
{
  return clk_info_->isGenClkSrcPath();
}

const Clock *
Tag::genClkSrcPathClk(const StaState *sta) const
{
  if (clk_info_->isGenClkSrcPath()
      && states_) {
    FilterPath *filter = sta->search()->filter();
    for (ExceptionState *state : *states_) {
      ExceptionPath *except = state->exception();
      if (except->isFilter()
	  && except != filter) {
	ExceptionTo *to = except->to();
	if (to) {
	  ClockSet *clks = to->clks();
	  if (clks && clks->size() == 1) {
	    ClockSet::Iterator clk_iter(clks);
	    const Clock *clk = clk_iter.next();
	    return clk;
	  }
	}
      }
    }
  }
  return nullptr;
}

void
Tag::findHash()
{
  // Common to hash_ and match_hash_.
  hash_ = hash_init_value;
  hashIncr(hash_, rf_index_);
  hashIncr(hash_, path_ap_index_);
  hashIncr(hash_, is_clk_);
  hashIncr(hash_, is_segment_start_);
  if (states_) {
    for (ExceptionState *state : *states_)
      hashIncr(hash_, state->hash());
  }
  hashIncr(hash_, clk_info_->hash());
  match_hash_ = hash_;

  // Finish hash_.
  if (input_delay_)
    hashIncr(hash_, input_delay_->index());

  // Finish match_hash_.
  hashIncr(match_hash_, clk_info_->isGenClkSrcPath());
}

size_t
Tag::hash(bool match_crpr_clk_pin,
	  const StaState *sta) const
{
  if (match_crpr_clk_pin)
    return hashSum(hash_, clk_info_->crprClkVertexId(sta));
  else
    return hash_;
}

size_t
Tag::matchHash(bool match_crpr_clk_pin,
               const StaState *sta) const
{
  if (match_crpr_clk_pin)
    return hashSum(match_hash_, clk_info_->crprClkVertexId(sta));
  else
    return match_hash_;
}

////////////////////////////////////////////////////////////////

TagLess::TagLess(const StaState *sta) :
  sta_(sta)
{
}

bool
TagLess::operator()(const Tag *tag1,
		    const Tag *tag2) const
{
  return Tag::cmp(tag1, tag2, sta_) < 0;
}

int
Tag::cmp(const Tag *tag1,
	 const Tag *tag2,
	 const StaState *sta)
{
  if (tag1 == tag2)
    return 0;

  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  int clk_cmp = ClkInfo::cmp(clk_info1, clk_info2, sta);
  if (clk_cmp != 0)
    return clk_cmp;

  PathAPIndex path_ap_index1 = tag1->pathAPIndex();
  PathAPIndex path_ap_index2 = tag2->pathAPIndex();
  if (path_ap_index1 < path_ap_index2)
    return -1;
  if (path_ap_index1 > path_ap_index2)
    return 1;

  int rf_index1 = tag1->rfIndex();
  int rf_index2 = tag2->rfIndex();
  if (rf_index1 < rf_index2)
    return -1;
  if (rf_index1 > rf_index2)
    return 1;

  bool is_clk1 = tag1->isClock();
  bool is_clk2 = tag2->isClock();
  if (!is_clk1 && is_clk2)
    return -1;
  if (is_clk1 && !is_clk2)
    return 1;

  InputDelay *input_delay1 = tag1->inputDelay();
  InputDelay *input_delay2 = tag2->inputDelay();
  int input_delay_index1 = input_delay1 ? input_delay1->index() : -1;
  int input_delay_index2 = input_delay2 ? input_delay2->index() : -1;
  if (input_delay_index1 < input_delay_index2)
    return -1;
  if (input_delay_index1 > input_delay_index2)
    return 1;

  bool is_segment_start1 = tag1->isSegmentStart();
  bool is_segment_start2 = tag2->isSegmentStart();
  if (!is_segment_start1 && is_segment_start2)
    return -1;
  if (is_segment_start1 && !is_segment_start2)
    return 1;

  return stateCmp(tag1, tag2);
}

bool
Tag::equal(const Tag *tag1,
	   const Tag *tag2,
	   const StaState *sta)
{
  return cmp(tag1, tag2, sta) == 0;
}

////////////////////////////////////////////////////////////////

bool
TagIndexLess::operator()(const Tag *tag1,
			 const Tag *tag2) const
{
  return tag1->index() < tag2->index();
}

////////////////////////////////////////////////////////////////

TagMatchLess::TagMatchLess(bool match_crpr_clk_pin,
			   const StaState *sta) :
  match_crpr_clk_pin_(match_crpr_clk_pin),
  sta_(sta)
{
}

bool
TagMatchLess::operator()(const Tag *tag1,
			 const Tag *tag2) const
{
  return Tag::matchCmp(tag1, tag2, match_crpr_clk_pin_, sta_) < 0;
}

////////////////////////////////////////////////////////////////

bool
Tag::match(const Tag *tag1,
	   const Tag *tag2,
	   const StaState *sta)
{
  return Tag::matchCmp(tag1, tag2, true, sta) == 0;
}

bool
Tag::match(const Tag *tag1,
	   const Tag *tag2,
	   bool match_crpr_clk_pin,
	   const StaState *sta)
{
  return Tag::matchCmp(tag1, tag2, match_crpr_clk_pin, sta) == 0;
}

int
Tag::matchCmp(const Tag *tag1,
	      const Tag *tag2,
	      bool match_crpr_clk_pin,
	      const StaState *sta)
{
  if (tag1 == tag2)
    return 0;

  int rf_index1 = tag1->rfIndex();
  int rf_index2 = tag2->rfIndex();
  if (rf_index1 < rf_index2)
    return -1;
  if (rf_index1 > rf_index2)
    return 1;

  PathAPIndex path_ap_index1 = tag1->pathAPIndex();
  PathAPIndex path_ap_index2 = tag2->pathAPIndex();
  if (path_ap_index1 < path_ap_index2)
    return -1;
  if (path_ap_index1 > path_ap_index2)
    return 1;

  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  const ClockEdge *clk_edge1 = clk_info1->clkEdge();
  const ClockEdge *clk_edge2 = clk_info2->clkEdge();
  int edge_index1 = clk_edge1 ? clk_edge1->index() : -1;
  int edge_index2 = clk_edge2 ? clk_edge2->index() : -1;
  if (edge_index1 < edge_index2)
    return -1;
  if (edge_index1 > edge_index2)
    return 1;

  bool is_clk1 = tag1->isClock();
  bool is_clk2 = tag2->isClock();
  if (!is_clk1 && is_clk2)
    return -1;
  if (is_clk1 && !is_clk2)
    return 1;

  bool is_genclk_src1 = clk_info1->isGenClkSrcPath();
  bool is_genclk_src2 = clk_info2->isGenClkSrcPath();
  if (!is_genclk_src1 && is_genclk_src2)
    return -1;
  if (is_genclk_src1 && !is_genclk_src2)
    return 1;

  bool is_segment_start1 = tag1->isSegmentStart();
  bool is_segment_start2 = tag2->isSegmentStart();
  if (!is_segment_start1 && is_segment_start2)
    return -1;
  if (is_segment_start1 && !is_segment_start2)
    return 1;

  if (match_crpr_clk_pin
      && sta->crprActive()) {
    VertexId crpr_vertex1 = clk_info1->crprClkVertexId(sta);
    VertexId crpr_vertex2 = clk_info2->crprClkVertexId(sta);
    if (crpr_vertex1 < crpr_vertex2)
      return -1;
    if (crpr_vertex1 > crpr_vertex2)
      return 1;
  }

  return stateCmp(tag1, tag2);
}

bool
Tag::matchNoCrpr(const Tag *tag1,
		 const Tag *tag2)
{
  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  return tag1 == tag2
    || (clk_info1->clkEdge() == clk_info2->clkEdge()
	&& tag1->rfIndex() == tag2->rfIndex()
	&& tag1->pathAPIndex() == tag2->pathAPIndex()
	&& tag1->isClock() == tag2->isClock()
	&& clk_info1->isGenClkSrcPath() == clk_info2->isGenClkSrcPath()
	&& Tag::stateEqual(tag1, tag2));
}

bool
Tag::matchNoPathAp(const Tag *tag1,
		   const Tag *tag2)
{
  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  return tag1 == tag2
    || (clk_info1->clkEdge() == clk_info2->clkEdge()
	&& tag1->rfIndex() == tag2->rfIndex()
	&& tag1->isClock() == tag2->isClock()
	&& tag1->isSegmentStart() == tag2->isSegmentStart()
	&& clk_info1->isGenClkSrcPath() == clk_info2->isGenClkSrcPath()
	&& Tag::stateEqual(tag1, tag2));
}

bool
Tag::matchCrpr(const Tag *tag1,
	       const Tag *tag2)
{
  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  return tag1 == tag2
    || (clk_info1->clkEdge() == clk_info2->clkEdge()
	&& tag1->rfIndex() == tag2->rfIndex()
	&& tag1->isClock() == tag2->isClock()
	&& tag1->isSegmentStart() == tag2->isSegmentStart()
	&& clk_info1->isGenClkSrcPath() == clk_info2->isGenClkSrcPath()
	&& stateEqualCrpr(tag1, tag2));
}

////////////////////////////////////////////////////////////////

int
Tag::stateCmp(const Tag *tag1,
	      const Tag *tag2)
{
  ExceptionStateSet *states1 = tag1->states();
  ExceptionStateSet *states2 = tag2->states();
  bool states_null1 = (states1 == nullptr || states1->empty());
  bool states_null2 = (states2 == nullptr || states2->empty());
  if (states_null1
      && states_null2)
    return 0;
  if (states_null1
      && !states_null2)
    return -1;
  if (!states_null1
      && states_null2)
    return 1;

  size_t state_size1 = states1->size();
  size_t state_size2 = states2->size();
  if (state_size1 < state_size2)
    return -1;
  if (state_size1 > state_size2)
    return 1;

  ExceptionStateSet::Iterator state_iter1(states1);
  ExceptionStateSet::Iterator state_iter2(states2);
  while (state_iter1.hasNext()
	 && state_iter2.hasNext()) {
    ExceptionState *state1 = state_iter1.next();
    ExceptionState *state2 = state_iter2.next();
    if (exceptionStateLess(state1, state2))
      return -1;
    if (exceptionStateLess(state2, state1))
      return 1;
  }
  return 0;
}

bool
Tag::stateEqual(const Tag *tag1,
		const Tag *tag2)
{
  return stateCmp(tag1, tag2) == 0;
}

// Match loop exception states only for crpr min/max paths.
bool
Tag::stateEqualCrpr(const Tag *tag1,
		    const Tag *tag2)
{
  ExceptionStateSet *states1 = tag1->states();
  ExceptionStateSet *states2 = tag2->states();
  ExceptionStateSet::Iterator state_iter1(states1);
  ExceptionStateSet::Iterator state_iter2(states2);
  ExceptionState *state1, *state2;
  do {
    state1 = nullptr;
    while (state_iter1.hasNext()) {
      state1 = state_iter1.next();
      ExceptionPath *exception1 = state1->exception();
      if (exception1->isLoop())
	break;
      else
	state1 = nullptr;
    }
    state2 = nullptr;
    while (state_iter2.hasNext()) {
      state2 = state_iter2.next();
      ExceptionPath *exception2 = state2->exception();
      if (exception2->isLoop())
	break;
      else
	state2 = nullptr;
    }
    if (state1 != state2)
      return false;
  } while (state1 && state2);
  return state1 == nullptr
    && state2 == nullptr;
}

////////////////////////////////////////////////////////////////

TagHash::TagHash(const StaState *sta) :
  sta_(sta)
{
}

size_t
TagHash::operator()(const Tag *tag) const
{
  bool crpr_on = sta_->crprActive();
  return tag->matchHash(crpr_on, sta_);
}

TagEqual::TagEqual(const StaState *sta) :
  sta_(sta)
{
}

bool
TagEqual::operator()(const Tag *tag1,
		     const Tag *tag2) const
{
  return Tag::equal(tag1, tag2, sta_);
}

TagMatchHash::TagMatchHash(bool match_crpr_clk_pin,
			   const StaState *sta) :
  match_crpr_clk_pin_(match_crpr_clk_pin),
  sta_(sta)
{
}

size_t
TagMatchHash::operator()(const Tag *tag) const
{
  return tag->matchHash(match_crpr_clk_pin_, sta_);
}

TagMatchEqual::TagMatchEqual(bool match_crpr_clk_pin,
			     const StaState *sta) :
  match_crpr_clk_pin_(match_crpr_clk_pin),
  sta_(sta)
{
}

bool
TagMatchEqual::operator()(const Tag *tag1, 
			  const Tag *tag2) const
{
  return Tag::match(tag1, tag2, match_crpr_clk_pin_, sta_);
}

} // namespace
