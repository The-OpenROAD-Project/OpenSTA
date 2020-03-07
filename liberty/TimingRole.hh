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
#include "Map.hh"
#include "StringUtil.hh"

namespace sta {

class TimingRole;

typedef Map<const char*, TimingRole*, CharPtrLess> TimingRoleMap;

class TimingRole
{
public:
  static void init();
  static void destroy();
  static TimingRole *find(const char *name);
  // Singleton accessors.
  static TimingRole *wire() { return wire_; }
  static TimingRole *combinational() { return combinational_; }
  static TimingRole *tristateEnable() { return tristate_enable_; }
  static TimingRole *tristateDisable() { return tristate_disable_; }
  static TimingRole *regClkToQ() { return reg_clk_q_; }
  static TimingRole *regSetClr() { return reg_set_clr_; }
  static TimingRole *latchEnToQ() { return latch_en_q_; }
  static TimingRole *latchDtoQ() { return latch_d_q_; }
  static TimingRole *setup() { return setup_; }
  static TimingRole *hold() { return hold_; }
  static TimingRole *recovery() { return recovery_; }
  static TimingRole *removal() { return removal_; }
  static TimingRole *width() { return width_; }
  static TimingRole *period() { return period_; }
  static TimingRole *skew() { return skew_; }
  static TimingRole *nochange() { return nochange_; }
  static TimingRole *outputSetup() { return output_setup_; }
  static TimingRole *outputHold() { return output_hold_; }
  static TimingRole *gatedClockSetup() { return gated_clk_setup_; }
  static TimingRole *gatedClockHold() { return gated_clk_hold_; }
  static TimingRole *latchSetup() { return latch_setup_; }
  static TimingRole *latchHold() { return latch_hold_; }
  static TimingRole *dataCheckSetup() { return data_check_setup_; }
  static TimingRole *dataCheckHold() { return data_check_hold_; }
  static TimingRole *nonSeqSetup() { return non_seq_setup_; }
  static TimingRole *nonSeqHold() { return non_seq_hold_; }
  const char *asString() const { return name_; }
  int index() const { return index_; }
  bool isWire() const;
  bool isTimingCheck() const { return is_timing_check_; }
  bool isAsyncTimingCheck() const;
  bool isNonSeqTimingCheck() const { return is_non_seq_check_; }
  bool isDataCheck() const;
  const TimingRole *genericRole() const;
  const TimingRole *sdfRole() const;
  // Timing check data path min/max.
  MinMax *pathMinMax() const { return path_min_max_; }
  // Timing check target clock path insertion delay early/late.
  const EarlyLate *tgtClkEarlyLate() const;

  // Pseudo role to match sdf IOPATH.
  static TimingRole *sdfIopath() { return sdf_iopath_; }
  static bool less(const TimingRole *role1,
		   const TimingRole *role2);
  static const int index_max = 26;

private:
  TimingRole(const char *name,
	     bool is_sdf_iopath,
	     bool is_timing_check,
 	     bool is_non_seq_check,
	     MinMax *path_min_max,
	     // generic_type = nullptr means type is the same as this.
	     TimingRole *generic_role,
	     int index);
  DISALLOW_COPY_AND_ASSIGN(TimingRole);

  const char *name_;
  bool is_timing_check_;
  bool is_sdf_iopath_;
  bool is_non_seq_check_;
  TimingRole *generic_role_;
  unsigned index_;
  MinMax *path_min_max_;

  static TimingRole *wire_;
  static TimingRole *combinational_;
  static TimingRole *tristate_enable_;
  static TimingRole *tristate_disable_;
  static TimingRole *reg_clk_q_;
  static TimingRole *reg_set_clr_;
  static TimingRole *latch_en_q_;
  static TimingRole *latch_d_q_;
  static TimingRole *setup_;
  static TimingRole *hold_;
  static TimingRole *recovery_;
  static TimingRole *removal_;
  static TimingRole *width_;
  static TimingRole *period_;
  static TimingRole *skew_;
  static TimingRole *nochange_;
  static TimingRole *sdf_iopath_;
  static TimingRole *output_setup_;
  static TimingRole *output_hold_;
  static TimingRole *gated_clk_setup_;
  static TimingRole *gated_clk_hold_;
  static TimingRole *latch_setup_;
  static TimingRole *latch_hold_;
  static TimingRole *data_check_setup_;
  static TimingRole *data_check_hold_;
  static TimingRole *non_seq_setup_;
  static TimingRole *non_seq_hold_;
  static TimingRoleMap timing_roles_;

  friend class TimingRoleLess;
};

} // namespace
