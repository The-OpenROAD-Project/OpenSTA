// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#pragma once

#include "UnorderedSet.hh"
#include "MinMax.hh"
#include "TimingRole.hh"
#include "StaState.hh"
#include "SdcClass.hh"

namespace sta {

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

class CycleAcctingLess
{
public:
  bool operator()(const CycleAccting *acct1,
		  const CycleAccting *acct2) const;
};

typedef UnorderedSet<CycleAccting*, CycleAcctingHash, CycleAcctingEqual> CycleAcctingSet;

class CycleAcctings
{
public:
  CycleAcctings(Sdc *sdc);
  ~CycleAcctings();
  void clear();
  // Find the cycle accounting info for paths that start at src clock
  // edge and end at target clock edge.
  CycleAccting *cycleAccting(const ClockEdge *src,
			     const ClockEdge *tgt);
  void reportClkToClkMaxCycleWarnings(Report *report);

private:
  Sdc *sdc_;
  CycleAcctingSet cycle_acctings_;
};

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
  int firstCycle(const ClockEdge *clk_edge) const;

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

} // namespace
