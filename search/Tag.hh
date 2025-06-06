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
#include "Set.hh"
#include "Transition.hh"
#include "SdcClass.hh"
#include "SearchClass.hh"
#include "Path.hh"

namespace sta {

// Tags are used to distinguish multiple paths that hold
// arrival/required times on a vertex.
//
// Each tag corresponds to a different path on the vertex thru a
// set of exceptions.
//
// Clock paths are distinguished from non-clock paths using separate
// tags. This is because clocks pins can also have input arrivals wrt
// other clocks.
//
// When common clock reconvergence pessimism removal is enabled the
// tag ClkInfo includes the last clock driver pin so that distinct
// paths are used for paths from different sources of min/max clock
// arrivals.

class Tag
{
public:
  Tag(TagIndex index,
      int rf_index,
      PathAPIndex path_ap_index,
      ClkInfo *clk_info,
      bool is_clk,
      InputDelay *input_delay,
      bool is_segment_start,
      ExceptionStateSet *states,
      bool own_states,
      const StaState *sta);
  ~Tag();
  std::string to_string(const StaState *sta) const;
  std::string to_string(bool report_index,
                        bool report_rf_min_max,
                        const StaState *sta) const;
  ClkInfo *clkInfo() const { return clk_info_; }
  bool isClock() const { return is_clk_; }
  const ClockEdge *clkEdge() const;
  const Clock *clock() const;
  const Pin *clkSrc() const;
  int rfIndex() const { return rf_index_; }
  const RiseFall *transition() const;
  PathAnalysisPt *pathAnalysisPt(const StaState *sta) const;
  PathAPIndex pathAPIndex() const { return path_ap_index_; }
  TagIndex index() const { return index_; }
  ExceptionStateSet *states() const { return states_; }
  void setStates(ExceptionStateSet *states);
  bool isGenClkSrcPath() const;
  const Clock *genClkSrcPathClk(const StaState *sta) const;
  // Input delay at search startpoint (not propagated).
  InputDelay *inputDelay() const { return input_delay_; }
  bool isLoop() const { return is_loop_; }
  bool isFilter() const { return is_filter_; }
  bool isSegmentStart() const { return is_segment_start_; }
  size_t hash() const { return hash_; }
  size_t matchHash(bool match_crpr_clk_pin,
                   const StaState *sta) const;

protected:
  void findHash();

private:
  ClkInfo *clk_info_;
  InputDelay *input_delay_;
  ExceptionStateSet *states_;
  size_t hash_;
  size_t match_hash_;
  TagIndex index_;
  bool is_clk_:1;
  bool is_filter_:1;
  bool is_loop_:1;
  bool is_segment_start_:1;
  // Indicates that states_ is owned by the tag.
  bool own_states_:1;
  unsigned int rf_index_:RiseFall::index_bit_count;
  unsigned int path_ap_index_:path_ap_index_bit_count;
};

class TagLess
{
public:
  TagLess(const StaState *sta);
  bool operator()(const Tag *tag1,
		  const Tag *tag2) const;

private:
  const StaState *sta_;
};

class TagIndexLess
{
public:
  bool operator()(const Tag *tag1,
		  const Tag *tag2) const;
};

class TagHash
{
public:
  size_t operator()(const Tag *tag) const;
};

class TagEqual
{
public:
  bool operator()(const Tag *tag1,
		  const Tag *tag2) const;
};

int
tagCmp(const Tag *tag1,
       const Tag *tag2,
       const StaState *sta);

// Match tag clock edge, clock driver and exception states but not clk info.
bool
tagMatch(const Tag *tag1,
	 const Tag *tag2,
	 const StaState *sta);
bool
tagMatch(const Tag *tag1,
	 const Tag *tag2,
 	 bool match_crpr_clk_pin,
	 const StaState *sta);
bool
tagStateEqual(const Tag *tag1,
	      const Tag *tag2);
bool
tagMatchNoCrpr(const Tag *tag1,
	       const Tag *tag2);
int
tagMatchCmp(const Tag *tag1,
	    const Tag *tag2,
	    bool match_clk_clk_pin,
	    const StaState *sta);

bool
tagMatchNoPathAp(const Tag *tag1,
		 const Tag *tag2);
bool
tagMatchCrpr(const Tag *tag1,
	     const Tag *tag2);

} // namespace
