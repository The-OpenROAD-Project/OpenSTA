// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include "PocvMode.hh"

namespace sta {

enum class CrprMode { same_pin, same_transition };

// TCL Variables
class Variables
{
public:
  // TCL variable sta_crpr_enabled.
  bool crprEnabled() const { return crpr_enabled_; }
  void setCrprEnabled(bool enabled);
  // TCL variable sta_crpr_mode.
  CrprMode crprMode() const { return crpr_mode_; }
  void setCrprMode(CrprMode mode);
  // Propagate gated clock enable arrivals.
  bool propagateGatedClockEnable() const { return propagate_gated_clock_enable_; }
  void setPropagateGatedClockEnable(bool enable);
  // TCL variable sta_preset_clear_arcs_enabled.
  // Enable search through preset/clear arcs.
  bool presetClrArcsEnabled() const { return preset_clr_arcs_enabled_; }
  void setPresetClrArcsEnabled(bool enable);
  // TCL variable sta_cond_default_arcs_enabled.
  // Enable/disable default arcs when conditional arcs exist.
  bool condDefaultArcsEnabled() const { return cond_default_arcs_enabled_; }
  void setCondDefaultArcsEnabled(bool enabled);
  // TCL variable sta_internal_bidirect_instance_paths_enabled.
  // Enable/disable timing from bidirect pins back into the instance.
  bool bidirectInstPathsEnabled() const { return bidirect_inst_paths_enabled_; }
  void setBidirectInstPathsEnabled(bool enabled);
  // TCL variable sta_recovery_removal_checks_enabled.
  bool recoveryRemovalChecksEnabled() const { return recovery_removal_checks_enabled_; }
  void setRecoveryRemovalChecksEnabled(bool enabled);
  // TCL variable sta_gated_clock_checks_enabled.
  bool gatedClkChecksEnabled() const { return gated_clk_checks_enabled_; }
  void setGatedClkChecksEnabled(bool enabled);
  // TCL variable sta_dynamic_loop_breaking.
  bool dynamicLoopBreaking() const { return dynamic_loop_breaking_; }
  void setDynamicLoopBreaking(bool enable);
  // TCL variable sta_propagate_all_clocks.
  bool propagateAllClocks() const { return propagate_all_clks_; }
  void setPropagateAllClocks(bool prop);
  // TCL var sta_clock_through_tristate_enabled.
  bool clkThruTristateEnabled() const { return clk_thru_tristate_enabled_; }
  void setClkThruTristateEnabled(bool enable);
  // TCL variable sta_input_port_default_clock.
  bool useDefaultArrivalClock() { return use_default_arrival_clock_; }
  void setUseDefaultArrivalClock(bool enable);
  bool pocvEnabled() const;
  PocvMode pocvMode() const { return pocv_mode_; }
  void setPocvMode(PocvMode mode);
  float pocvQuantile() const { return pocv_quantile_; }
  void setPocvQuantile(float quantile);

private:
  bool crpr_enabled_{true};
  CrprMode crpr_mode_{CrprMode::same_pin};
  bool propagate_gated_clock_enable_{true};
  bool preset_clr_arcs_enabled_{false};
  bool cond_default_arcs_enabled_{true};
  bool bidirect_inst_paths_enabled_{false};
  bool recovery_removal_checks_enabled_{true};
  bool gated_clk_checks_enabled_{true};
  bool clk_thru_tristate_enabled_{false};
  bool dynamic_loop_breaking_{false};
  bool propagate_all_clks_{false};
  bool use_default_arrival_clock_{false};
  PocvMode pocv_mode_{PocvMode::scalar};
  float pocv_quantile_{3.0};
};

} // namespace sta
