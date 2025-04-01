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

#include "SdcClass.hh"
#include "StaState.hh"
#include "SearchClass.hh"

namespace sta {

class CrprPaths;

// Clock Reconvergence Pessimism Removal.
class CheckCrpr : public StaState
{
public:
  explicit CheckCrpr(StaState *sta);

  // Find the maximum possible crpr (clock min/max delta delay) for path.
  Arrival maxCrpr(ClkInfo *clk_info);
  // Timing check CRPR.
  Crpr checkCrpr(const Path *src_clk_path,
		 const Path *tgt_clk_path);
  void checkCrpr(const Path *src_path,
		 const Path *tgt_clk_path,
		 // Return values.
		 Crpr &crpr,
		 Pin *&crpr_pin);
  // Output delay CRPR.
  Crpr outputDelayCrpr(const Path *src_clk_path,
		       const ClockEdge *tgt_clk_edge);
  void outputDelayCrpr(const Path *src_clk_path,
		       const ClockEdge *tgt_clk_edge,
		       // Return values.
		       Crpr &crpr,
		       Pin *&crpr_pin);

private:
  void clkPathPrev(const Path *path,
                   Path &prev);
  Arrival otherMinMaxArrival(const Path *path);
  void checkCrpr1(const Path *src_path,
		  const Path *tgt_clk_path,
		  bool same_pin,
		  // Return values.
		  Crpr &crpr,
		  Pin *&crpr_pin);
  void outputDelayCrpr1(const Path *src_path,
			const ClockEdge *tgt_clk_edge,
			const PathAnalysisPt *tgt_path_ap,
			bool same_pin,
			// Return values.
			Crpr &crpr,
			Pin *&crpr_pin);
  bool crprPossible(const Clock *clk1,
		    const Clock *clk2);
  ConstPathSeq genClkSrcPaths(const Path *path);
  void findCrpr(const Path *src_clk_path,
		const Path *tgt_clk_path,
		bool same_pin,
		// Return values.
		Crpr &crpr,
		Pin *&common_pin);
  Path *portClkPath(const ClockEdge *clk_edge,
                    const Pin *clk_src_pin,
                    const PathAnalysisPt *path_ap);
  Crpr findCrpr1(const Path *src_clk_path,
		 const Path *tgt_clk_path);
  float crprArrivalDiff(const Path *path);
};

} // namespace
