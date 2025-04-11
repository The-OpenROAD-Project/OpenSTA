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

#include <map>

#include "MinMax.hh"
#include "StringUtil.hh"

namespace sta {

class TimingRole;

typedef std::map<std::string, const TimingRole*> TimingRoleMap;

class TimingRole
{
public:
  static const TimingRole *find(const char *name);
  // Singleton accessors.
  static const TimingRole *wire() { return &wire_; }
  static const TimingRole *combinational() { return &combinational_; }
  static const TimingRole *tristateEnable() { return &tristate_enable_; }
  static const TimingRole *tristateDisable() { return &tristate_disable_; }
  static const TimingRole *regClkToQ() { return &reg_clk_q_; }
  static const TimingRole *regSetClr() { return &reg_set_clr_; }
  static const TimingRole *latchEnToQ() { return &latch_en_q_; }
  static const TimingRole *latchDtoQ() { return &latch_d_q_; }
  static const TimingRole *setup() { return &setup_; }
  static const TimingRole *hold() { return &hold_; }
  static const TimingRole *recovery() { return &recovery_; }
  static const TimingRole *removal() { return &removal_; }
  static const TimingRole *width() { return &width_; }
  static const TimingRole *period() { return &period_; }
  static const TimingRole *skew() { return &skew_; }
  static const TimingRole *nochange() { return &nochange_; }
  static const TimingRole *outputSetup() { return &output_setup_; }
  static const TimingRole *outputHold() { return &output_hold_; }
  static const TimingRole *gatedClockSetup() { return &gated_clk_setup_; }
  static const TimingRole *gatedClockHold() { return &gated_clk_hold_; }
  static const TimingRole *latchSetup() { return &latch_setup_; }
  static const TimingRole *latchHold() { return &latch_hold_; }
  static const TimingRole *dataCheckSetup() { return &data_check_setup_; }
  static const TimingRole *dataCheckHold() { return &data_check_hold_; }
  static const TimingRole *nonSeqSetup() { return &non_seq_setup_; }
  static const TimingRole *nonSeqHold() { return &non_seq_hold_; }
  static const TimingRole *clockTreePathMin() { return &clock_tree_path_min_; }
  static const TimingRole *clockTreePathMax() { return &clock_tree_path_max_; }
  const std::string &to_string() const { return name_; }
  int index() const { return index_; }
  bool isWire() const;
  bool isTimingCheck() const { return is_timing_check_; }
  // TIming check but not width or period.
  bool isTimingCheckBetween() const;
  bool isAsyncTimingCheck() const;
  bool isNonSeqTimingCheck() const { return is_non_seq_check_; }
  bool isDataCheck() const;
  bool isLatchDtoQ() const;
  const TimingRole *genericRole() const;
  const TimingRole *sdfRole() const;
  // Timing check data path min/max.
  const MinMax *pathMinMax() const { return path_min_max_; }
  // Timing check target clock path insertion delay early/late.
  const EarlyLate *tgtClkEarlyLate() const;

  // Pseudo role to match sdf IOPATH.
  static const TimingRole *sdfIopath() { return &sdf_iopath_; }
  static bool less(const TimingRole *role1,
		   const TimingRole *role2);
  static const int index_max = 26;

private:
  TimingRole(const char *name,
	     bool is_sdf_iopath,
	     bool is_timing_check,
 	     bool is_non_seq_check,
	     const MinMax *path_min_max,
	     // generic_type = nullptr means type is the same as this.
	     const TimingRole *generic_role,
	     int index);

  const std::string name_;
  bool is_timing_check_;
  bool is_sdf_iopath_;
  bool is_non_seq_check_;
  const TimingRole *generic_role_;
  unsigned index_;
  const MinMax *path_min_max_;

  static const TimingRole wire_;
  static const TimingRole combinational_;
  static const TimingRole tristate_enable_;
  static const TimingRole tristate_disable_;
  static const TimingRole reg_clk_q_;
  static const TimingRole reg_set_clr_;
  static const TimingRole latch_en_q_;
  static const TimingRole latch_d_q_;
  static const TimingRole setup_;
  static const TimingRole hold_;
  static const TimingRole recovery_;
  static const TimingRole removal_;
  static const TimingRole width_;
  static const TimingRole period_;
  static const TimingRole skew_;
  static const TimingRole nochange_;
  static const TimingRole sdf_iopath_;
  static const TimingRole output_setup_;
  static const TimingRole output_hold_;
  static const TimingRole gated_clk_setup_;
  static const TimingRole gated_clk_hold_;
  static const TimingRole latch_setup_;
  static const TimingRole latch_hold_;
  static const TimingRole data_check_setup_;
  static const TimingRole data_check_hold_;
  static const TimingRole non_seq_setup_;
  static const TimingRole non_seq_hold_;
  static const TimingRole clock_tree_path_min_;
  static const TimingRole clock_tree_path_max_;
  static TimingRoleMap timing_roles_;

  friend class TimingRoleLess;
};

} // namespace
