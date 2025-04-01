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

#include "TimingRole.hh"

namespace sta {

TimingRoleMap TimingRole::timing_roles_;
const TimingRole TimingRole::wire_("wire", false, false, false, nullptr, nullptr, 0);
const TimingRole TimingRole::combinational_("combinational", true, false,  false,
                                            nullptr, nullptr, 1);
const TimingRole TimingRole::tristate_enable_("tristate enable",
                                              true, false, false,
                                              nullptr, nullptr, 2);
const TimingRole TimingRole::tristate_disable_("tristate disable",
                                               true, false, false,
                                               nullptr, nullptr, 3);
const TimingRole TimingRole::reg_clk_q_("Reg Clk to Q", true, false, false,
                                        nullptr, nullptr, 4);
const TimingRole TimingRole::reg_set_clr_("Reg Set/Clr", true, false, false,
                                          nullptr, nullptr, 5);
const TimingRole TimingRole::latch_en_q_("Latch En to Q", true, false, false,
                                         nullptr, TimingRole::regClkToQ(), 6);
const TimingRole TimingRole::latch_d_q_("Latch D to Q", true, false, false,
                                        nullptr, nullptr, 7);
const TimingRole TimingRole::sdf_iopath_("sdf IOPATH", true, false, false,
                                         nullptr, nullptr, 8);
const TimingRole TimingRole::setup_("setup", false, true, false,
                                    MinMax::max(), nullptr, 9);
const TimingRole TimingRole::hold_("hold", false, true, false,
                                   MinMax::min(), nullptr, 10);
const TimingRole TimingRole::recovery_("recovery", false, true, false,
                                       MinMax::max(), TimingRole::setup(), 11);
const TimingRole TimingRole::removal_("removal", false, true, false,
                                      MinMax::min(), TimingRole::hold(), 12);
const TimingRole TimingRole::width_("width", false, true, false,
                                    nullptr, nullptr, 13);
const TimingRole TimingRole::period_("period", false, true, false,
                                     nullptr, nullptr, 14);
const TimingRole TimingRole::skew_("skew", false, true, false,
                                   nullptr, nullptr, 15);
const TimingRole TimingRole::nochange_("nochange", true, false, false,
                                       nullptr, nullptr, 16);
const TimingRole TimingRole::output_setup_("output setup", false, true, false,
                                           MinMax::max(), TimingRole::setup(), 17);
const TimingRole TimingRole::output_hold_("output hold", false, true, false,
                                          MinMax::min(), TimingRole::hold(), 18);
const TimingRole TimingRole::gated_clk_setup_("clock gating setup",
                                              false, true, false,
                                              MinMax::max(), TimingRole::setup(),19);
const TimingRole TimingRole::gated_clk_hold_("clock gating hold", false, true, false,
                                             MinMax::min(), TimingRole::hold(),20);
const TimingRole TimingRole::latch_setup_("latch setup", false, true, false,
                                          MinMax::max(), TimingRole::setup(),21);
const TimingRole TimingRole::latch_hold_("latch hold", false, true, false,
                                         MinMax::min(), TimingRole::hold(),22);
const TimingRole TimingRole::data_check_setup_("data check setup",
                                               false, true, false,
                                               MinMax::max(),TimingRole::setup(),23);
const TimingRole TimingRole::data_check_hold_("data check hold", false, true, false,
                                              MinMax::min(), TimingRole::hold(), 24);
const TimingRole TimingRole::non_seq_setup_("non-sequential setup", false, true, true,
                                            MinMax::max(), TimingRole::setup(), 25);
const TimingRole TimingRole::non_seq_hold_("non-sequential hold", false, true, true,
                                           MinMax::min(), TimingRole::hold(), 26);
const TimingRole TimingRole::clock_tree_path_min_("min clock tree path", false, false,
                                                  false, MinMax::min(), nullptr, 27);
const TimingRole TimingRole::clock_tree_path_max_("max clock tree path", false, false,
                                                  false, MinMax::max(), nullptr, 28);

TimingRole::TimingRole(const char *name,
		       bool is_sdf_iopath,
		       bool is_timing_check,
		       bool is_non_seq_check,
		       const MinMax *path_min_max,
		       const TimingRole *generic_role,
		       int index) :
  name_(name),
  is_timing_check_(is_timing_check),
  is_sdf_iopath_(is_sdf_iopath),
  is_non_seq_check_(is_non_seq_check),
  generic_role_(generic_role),
  index_(index),
  path_min_max_(path_min_max)
{
  timing_roles_[name] = this;
}

const TimingRole *
TimingRole::find(const char *name)
{
  return timing_roles_[name];
}

const TimingRole *
TimingRole::sdfRole() const
{
  if (is_sdf_iopath_)
    return &sdf_iopath_;
  else
    return this;
}

const TimingRole *
TimingRole::genericRole() const
{
  if (generic_role_ == nullptr)
    return this;
  else
    return generic_role_;
}

const EarlyLate *
TimingRole::tgtClkEarlyLate() const
{
  return path_min_max_->opposite();
}

bool
TimingRole::isWire() const
{
  return this == &wire_;
}

bool
TimingRole::isAsyncTimingCheck() const
{
  return this == &recovery_
    || this == &removal_;
}

bool
TimingRole::isDataCheck() const
{
  return this == &data_check_setup_
    || this == &data_check_hold_;
}

bool
TimingRole::isLatchDtoQ() const
{
  return this == &latch_d_q_;
}

bool
TimingRole::isTimingCheckBetween() const
{
  return is_timing_check_
    && this != &width_
    && this != &period_;
}

bool
TimingRole::less(const TimingRole *role1,
		 const TimingRole *role2)
{
  return role1->index() < role2->index();
}

} // namespace
