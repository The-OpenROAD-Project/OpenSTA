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
#include "Tag.hh"

namespace sta {

static int
tagStateCmp(const Tag *tag1,
	    const Tag *tag2);
static bool
tagStateEqual(ExceptionStateSet *states1,
	      ExceptionStateSet *states2);
static bool
tagStateEqualCrpr(const Tag *tag1,
		  const Tag *tag2);

Tag::Tag(TagIndex index,
	 int tr_index,
	 PathAPIndex path_ap_index,
	 ClkInfo *clk_info,
	 bool is_clk,
	 InputDelay *input_delay,
	 bool is_segment_start,
	 ExceptionStateSet *states,
	 bool own_states,
	 const StaState *sta) :
  clk_info_(clk_info),
  input_delay_(input_delay),
  states_(states),
  is_clk_(is_clk),
  is_filter_(false),
  is_loop_(false),
  is_segment_start_(is_segment_start),
  own_states_(own_states),
  index_(index),
  tr_index_(tr_index),
  path_ap_index_(path_ap_index)
{
  findHash();
  if (states_) {
    FilterPath *filter = sta->search()->filter();
    ExceptionStateSet::ConstIterator state_iter(states_);
    while (state_iter.hasNext()) {
      ExceptionState *state = state_iter.next();
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

const char *
Tag::asString(const StaState *sta) const
{
  return asString(true, sta);
}

const char *
Tag::asString(bool report_index,
	      const StaState *sta) const
{
  const Network *network = sta->network();
  const Corners *corners = sta->corners();
  string str;

  if (report_index)
    str += stringPrintTmp("%4d ", index_);

  const RiseFall *rf = transition();
  PathAnalysisPt *path_ap = corners->findPathAnalysisPt(path_ap_index_);
  str += stringPrintTmp("%s %s/%d ",
			rf->asString(),
			path_ap->pathMinMax()->asString(),
			path_ap_index_);

  ClockEdge *clk_edge = clkEdge();
  if (clk_edge)
    str += clk_edge->name();
  else
    str += "unclocked";

  bool is_genclk_src = clk_info_->isGenClkSrcPath();
  if (is_clk_ || is_genclk_src) {
    str += " (";
    if (is_clk_) {
      str += "clock";
      if (clk_info_->isPropagated())
	str += " prop";
      else
	str += " ideal";
      if (is_genclk_src)
	str += " ";
    }
    if (clk_info_->isGenClkSrcPath())
      str += "genclk";
    str += ")";
  }

  const Pin *clk_src = clkSrc();
  if (clk_src) {
    str += " clk_src ";
    str += network->pathName(clk_src);
  }

  const PathVertex crpr_clk_path(clk_info_->crprClkPath(), sta);
  if (!crpr_clk_path.isNull()) {
    str += " crpr_clk ";
    str += crpr_clk_path.name(sta);
  }

  if (input_delay_) {
    str += " input ";
    str += network->pathName(input_delay_->pin());
  }

  if (is_segment_start_)
    str += " segment_start";

  if (states_) {
    ExceptionStateSet::ConstIterator state_iter(states_);
    while (state_iter.hasNext()) {
      ExceptionState *state = state_iter.next();
      ExceptionPath *exception = state->exception();
      str += " ";
      str += exception->asString(network);
      if (state->nextThru()) {
	str += " (next thru ";
	str += state->nextThru()->asString(network);
	str += ")";
      }
      else {
	if (exception->thrus() != nullptr)
	  str += " (thrus complete)";
      }
    }
  }

  char *result = makeTmpString(str.size() + 1);
  strcpy(result, str.c_str());
  return result;
}

const RiseFall *
Tag::transition() const
{
  return RiseFall::find(tr_index_);
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

ClockEdge *
Tag::clkEdge() const
{
  return clk_info_->clkEdge();
}

Clock *
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

Clock *
Tag::genClkSrcPathClk(const StaState *sta) const
{
  if (clk_info_->isGenClkSrcPath()
      && states_) {
    FilterPath *filter = sta->search()->filter();
    ExceptionStateSet::ConstIterator state_iter(states_);
    while (state_iter.hasNext()) {
      ExceptionState *state = state_iter.next();
      ExceptionPath *except = state->exception();
      if (except->isFilter()
	  && except != filter) {
	ExceptionTo *to = except->to();
	if (to) {
	  ClockSet *clks = to->clks();
	  if (clks && clks->size() == 1) {
	    ClockSet::Iterator clk_iter(clks);
	    Clock *clk = clk_iter.next();
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
  hashIncr(hash_, tr_index_);
  hashIncr(hash_, path_ap_index_);
  hashIncr(hash_, is_clk_);
  hashIncr(hash_, is_segment_start_);
  if (states_) {
    ExceptionStateSet::Iterator state_iter(states_);
    while (state_iter.hasNext()) {
      ExceptionState *state = state_iter.next();
      hashIncr(hash_, state->hash());
    }
  }
  match_hash_ = hash_;

  // Finish hash_.
  hashIncr(hash_, clk_info_->hash());
  if (input_delay_)
    hashIncr(hash_, input_delay_->index());

  // Finish match_hash_.
  ClockEdge *clk_edge = clk_info_->clkEdge();
  if (clk_edge)
    hashIncr(match_hash_, clk_edge->index());
  hashIncr(match_hash_, clk_info_->isGenClkSrcPath());
}

size_t
Tag::matchHash(bool match_crpr_clk_pin) const
{
  if (match_crpr_clk_pin)
    // match_hash_ with crpr clk pin thrown in.
    return hashSum(match_hash_, clk_info_->crprClkVertexId());
  else
    return match_hash_;
}

////////////////////////////////////////////////////////////////

bool
TagLess::operator()(const Tag *tag1,
		    const Tag *tag2) const
{
  return tagCmp(tag1, tag2, true) < 0;
}

int
tagCmp(const Tag *tag1,
       const Tag *tag2,
       bool cmp_rf)
{
  if (tag1 == tag2)
    return 0;

  if (cmp_rf) {
    int tr_index1 = tag1->trIndex();
    int tr_index2 = tag2->trIndex();
    if (tr_index1 < tr_index2)
      return -1;
    if (tr_index1 > tr_index2)
      return 1;
  }

  PathAPIndex path_ap_index1 = tag1->pathAPIndex();
  PathAPIndex path_ap_index2 = tag2->pathAPIndex();
  if (path_ap_index1 < path_ap_index2)
    return -1;
  if (path_ap_index1 > path_ap_index2)
    return 1;

  size_t clk_info1 = tag1->clkInfo()->hash();
  size_t clk_info2 = tag2->clkInfo()->hash();
  if (clk_info1 < clk_info2)
    return -1;
  if (clk_info1 > clk_info2)
    return 1;

  bool is_clk1 = tag1->isClock();
  bool is_clk2 = tag2->isClock();
  if (!is_clk1 && is_clk2)
    return -1;
  if (is_clk1 && !is_clk2)
    return 1;

  InputDelay *input_delay1 = tag1->inputDelay();
  InputDelay *input_delay2 = tag2->inputDelay();
  int input_delay_index1 = input_delay1 ? input_delay1->index() : 0;
  int input_delay_index2 = input_delay2 ? input_delay2->index() : 0;
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

  return tagStateCmp(tag1, tag2);
}

int
tagEqual(const Tag *tag1,
	 const Tag *tag2)
{
  return tag1 == tag2
    || (tag1->trIndex() == tag2->trIndex()
	&& tag1->pathAPIndex() == tag2->pathAPIndex()
	&& tag1->clkInfo() == tag2->clkInfo()
	&& tag1->isClock() == tag2->isClock()
	&& tag1->inputDelay() == tag2->inputDelay()
	&& tag1->isSegmentStart() == tag2->isSegmentStart()
	&& tagStateEqual(tag1, tag2));
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
  return tagMatchCmp(tag1, tag2, match_crpr_clk_pin_, sta_) < 0;
}

////////////////////////////////////////////////////////////////

bool
tagMatch(const Tag *tag1,
	 const Tag *tag2,
	 const StaState *sta)
{
  return tagMatch(tag1, tag2, true, sta);
}

bool
tagMatch(const Tag *tag1,
  	 const Tag *tag2,
	 bool match_crpr_clk_pin,
	 const StaState *sta)
{
  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  return tag1 == tag2
    || (clk_info1->clkEdge() == clk_info2->clkEdge()
	&& tag1->trIndex() == tag2->trIndex()
	&& tag1->pathAPIndex() == tag2->pathAPIndex()
	&& tag1->isClock() == tag2->isClock()
	&& tag1->isSegmentStart() == tag2->isSegmentStart()
	&& clk_info1->isGenClkSrcPath() == clk_info2->isGenClkSrcPath()
	&& (!match_crpr_clk_pin
	    || !sta->sdc()->crprActive()
	    || clk_info1->crprClkVertexId() == clk_info2->crprClkVertexId())
	&& tagStateEqual(tag1, tag2));
}

int
tagMatchCmp(const Tag *tag1,
	    const Tag *tag2,
	    bool match_crpr_clk_pin,
	    const StaState *sta)
{
  if (tag1 == tag2)
    return 0;

  int tr_index1 = tag1->trIndex();
  int tr_index2 = tag2->trIndex();
  if (tr_index1 < tr_index2)
    return -1;
  if (tr_index1 > tr_index2)
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
      && sta->sdc()->crprActive()) {
    VertexId crpr_vertex1 = clk_info1->crprClkVertexId();
    VertexId crpr_vertex2 = clk_info2->crprClkVertexId();
    if (crpr_vertex1 < crpr_vertex2)
      return -1;
    if (crpr_vertex1 > crpr_vertex2)
      return 1;
  }

  return tagStateCmp(tag1, tag2);
}

bool
tagMatchNoCrpr(const Tag *tag1,
	       const Tag *tag2)
{
  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  return tag1 == tag2
    || (clk_info1->clkEdge() == clk_info2->clkEdge()
	&& tag1->trIndex() == tag2->trIndex()
	&& tag1->pathAPIndex() == tag2->pathAPIndex()
	&& tag1->isClock() == tag2->isClock()
	&& clk_info1->isGenClkSrcPath() == clk_info2->isGenClkSrcPath()
	&& tagStateEqual(tag1, tag2));
}

bool
tagMatchNoPathAp(const Tag *tag1,
		 const Tag *tag2)
{
  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  return tag1 == tag2
    || (clk_info1->clkEdge() == clk_info2->clkEdge()
	&& tag1->trIndex() == tag2->trIndex()
	&& tag1->isClock() == tag2->isClock()
	&& tag1->isSegmentStart() == tag2->isSegmentStart()
	&& clk_info1->isGenClkSrcPath() == clk_info2->isGenClkSrcPath()
	&& tagStateEqual(tag1, tag2));
}

bool
tagMatchCrpr(const Tag *tag1,
	     const Tag *tag2)
{
  const ClkInfo *clk_info1 = tag1->clkInfo();
  const ClkInfo *clk_info2 = tag2->clkInfo();
  return tag1 == tag2
    || (clk_info1->clkEdge() == clk_info2->clkEdge()
	&& tag1->trIndex() == tag2->trIndex()
	&& tag1->isClock() == tag2->isClock()
	&& tag1->isSegmentStart() == tag2->isSegmentStart()
	&& clk_info1->isGenClkSrcPath() == clk_info2->isGenClkSrcPath()
	&& tagStateEqualCrpr(tag1, tag2));
}

////////////////////////////////////////////////////////////////

static int
tagStateCmp(const Tag *tag1,
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
    if (state1 < state2)
      return -1;
    if (state1 > state2)
      return 1;
  }
  return 0;
}

bool
tagStateEqual(const Tag *tag1,
	      const Tag *tag2)
{
  return tagStateEqual(tag1->states(), tag2->states());
}

static bool
tagStateEqual(ExceptionStateSet *states1,
	      ExceptionStateSet *states2)
{
  bool states_null1 = (states1 == nullptr || states1->empty());
  bool states_null2 = (states2 == nullptr || states2->empty());
  if (states_null1 && states_null2)
    return true;
  else if (states_null1 != states_null2)
    return false;

  size_t state_size1 = states1->size();
  size_t state_size2 = states2->size();
  if (state_size1 == state_size2) {
    ExceptionStateSet::Iterator state_iter1(states1);
    ExceptionStateSet::Iterator state_iter2(states2);
    while (state_iter1.hasNext()
	   && state_iter2.hasNext()) {
      ExceptionState *state1 = state_iter1.next();
      ExceptionState *state2 = state_iter2.next();
      if (state1 != state2)
	return false;
    }
    return true;
  }
  else
    return false;
}

// Match false, loop exception states only for crpr min/max paths.
static bool
tagStateEqualCrpr(const Tag *tag1,
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
      if (exception1->isFalse()
	  || exception1->isLoop())
	break;
      else
	state1 = nullptr;
    }
    state2 = nullptr;
    while (state_iter2.hasNext()) {
      state2 = state_iter2.next();
      ExceptionPath *exception2 = state2->exception();
      if (exception2->isFalse()
	  || exception2->isLoop())
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

size_t
TagHash::operator()(const Tag *tag)
{
  return tag->hash();
}

bool
TagEqual::operator()(const Tag *tag1,
		     const Tag *tag2)
{
  return tagEqual(tag1, tag2);
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
  return tag->matchHash(match_crpr_clk_pin_);
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
  return tagMatch(tag1, tag2, match_crpr_clk_pin_, sta_);
}

} // namespace
