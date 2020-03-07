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
#include "MinMax.hh"
#include "StaState.hh"
#include "SdcClass.hh"
#include "TimingRole.hh"

namespace sta {

class CycleAccting
{
public:
  CycleAccting(const ClockEdge *src,
	       const ClockEdge *tgt);
  // Fill in required times.
  void findDelays(StaState *sta);
  // Find delays when source clk edge is the default arrival clock edge
  // (from unclocked set_input_delay).
  void findDefaultArrivalSrcDelays();
  const ClockEdge *src() const { return src_; }
  const ClockEdge *target() const { return tgt_; }
  float requiredTime(const TimingRole *check_role);
  int sourceCycle(const TimingRole *check_role);
  int targetCycle(const TimingRole *check_role);
  float sourceTimeOffset(const TimingRole *check_role);
  float targetTimeOffset(const TimingRole *check_role);
  bool maxCyclesExceeded() const { return max_cycles_exceeded_; }

private:
  DISALLOW_COPY_AND_ASSIGN(CycleAccting);
  void setHoldAccting(int src_cycle,
		      int tgt_cycle,
		      float delay,
		      float req);
  void setAccting(TimingRole *role,
		  int src_cycle,
		  int tgt_cycle,
		  float delay,
		  float req);
  void setSetupAccting(int src_cycle,
		       int tgt_cycle,
		       float delay,
		       float req);
  void setDefaultSetupAccting(int src_cycle,
			      int tgt_cycle,
			      float delay,
			      float req);
  void setDefaultHoldAccting(int src_cycle,
			     int tgt_cycle,
			     float delay,
			     float req);

  const ClockEdge *src_;
  const ClockEdge *tgt_;
  // Setup/hold delay from source to target.
  float delay_[TimingRole::index_max + 1];
  // Delay from beginning of src_cycle_'th cycle to target edge.
  float required_[TimingRole::index_max + 1];
  // Source clock cycle offset.
  int src_cycle_[TimingRole::index_max + 1];
  // Target clock cycle offset.
  int tgt_cycle_[TimingRole::index_max + 1];
  bool max_cycles_exceeded_;
};

class CycleAcctingLess
{
public:
  bool operator()(const CycleAccting *acct1,
		  const CycleAccting *acct2) const;
};

class CycleAcctingHash
{
public:
  size_t operator()(const CycleAccting *acct) const;
};

class CycleAcctingEqual
{
public:
  bool operator()(const CycleAccting *acct1,
		  const CycleAccting *acct2) const;
};

} // namespace
