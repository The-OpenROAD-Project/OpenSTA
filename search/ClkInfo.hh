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

#include "Transition.hh"
#include "SearchClass.hh"
#include "Sdc.hh"
#include "Path.hh"

namespace sta {

class Path;

class ClkInfo
{
public:
  ClkInfo(const ClockEdge *clk_edge,
	  const Pin *clk_src,
	  bool is_propagated,
	  const Pin *gen_clk_src,
	  bool is_gen_clk_src_path,
	  const RiseFall *pulse_clk_sense,
	  Arrival insertion,
	  float latency,
	  ClockUncertainties *uncertainties,
          PathAPIndex path_ap_index,
	  const Path *crpr_clk_path,
	  const StaState *sta);
  ~ClkInfo();
  std::string to_string(const StaState *sta) const;
  const ClockEdge *clkEdge() const { return clk_edge_; }
  const Clock *clock() const;
  const Pin *clkSrc() const { return clk_src_; }
  bool isPropagated() const { return is_propagated_; }
  const Pin *genClkSrc() const { return gen_clk_src_; }
  bool isPulseClk() const { return is_pulse_clk_; }
  const RiseFall *pulseClkSense() const;
  int pulseClkSenseRfIndex() const { return pulse_clk_sense_; }
  float latency() const { return latency_; }
  Arrival &insertion() { return insertion_; }
  const Arrival &insertion() const { return insertion_; }
  ClockUncertainties *uncertainties() const { return uncertainties_; }
  PathAPIndex pathAPIndex() const { return path_ap_index_; }
  // Clock path used for crpr resolution.
  // Null for clocks because the path cannot point to itself.
  Path *crprClkPath(const StaState *sta);
  const Path *crprClkPath(const StaState *sta) const;
  VertexId crprClkVertexId(const StaState *sta) const;
  bool hasCrprClkPin() const { return !crpr_clk_path_.isNull(); }
  // This clk_info/tag is used for a generated clock source path.
  bool isGenClkSrcPath() const { return is_gen_clk_src_path_; }
  size_t hash() const { return hash_; }
  bool crprPathRefsFilter() const { return crpr_path_refs_filter_; }
  const Path *crprClkPathRaw() const;

  static int cmp(const ClkInfo *clk_info1,
		 const ClkInfo *clk_info2,
		 const StaState *sta);
  static bool equal(const ClkInfo *clk_info1,
		    const ClkInfo *clk_info2,
		    const StaState *sta);
protected:
  void findHash(const StaState *sta);

private:
  const ClockEdge *clk_edge_;
  const Pin *clk_src_;
  const Pin *gen_clk_src_;
  Path crpr_clk_path_;
  ClockUncertainties *uncertainties_;
  Arrival insertion_;
  float latency_;
  size_t hash_;
  bool is_propagated_:1;
  bool is_gen_clk_src_path_:1;
  // This is used to break a circular dependency in Search::deleteFilteredArrival
  // between tags and clk infos that reference a filter.
  bool crpr_path_refs_filter_:1;
  bool is_pulse_clk_:1;
  unsigned int pulse_clk_sense_:RiseFall::index_bit_count;
  unsigned int path_ap_index_:path_ap_index_bit_count;
};

class ClkInfoLess
{
public:
  explicit ClkInfoLess(const StaState *sta);
  ~ClkInfoLess() {}
  bool operator()(const ClkInfo *clk_info1,
		  const ClkInfo *clk_info2) const;

protected:
  const StaState *sta_;
};

class ClkInfoHash
{
public:
  size_t operator()(const ClkInfo *clk_info) const;
};

class ClkInfoEqual
{
public:
  ClkInfoEqual(const StaState *sta);
  bool operator()(const ClkInfo *clk_info1,
		  const ClkInfo *clk_info2) const;

protected:
  const StaState *sta_;
};

} // namespace
