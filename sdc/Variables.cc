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

#include "Variables.hh"

namespace sta {

void
Variables::setUseKahnsBfs(bool use)
{
  use_kahns_bfs_ = use;
}

void
Variables::setCrprEnabled(bool enabled)
{
  crpr_enabled_ = enabled;
}

void
Variables::setCrprMode(CrprMode mode)
{
  crpr_mode_ = mode;
}

void
Variables::setPropagateGatedClockEnable(bool enable)
{
  propagate_gated_clock_enable_ = enable;
}

void
Variables::setPresetClrArcsEnabled(bool enable)
{
  preset_clr_arcs_enabled_ = enable;
}

void
Variables::setCondDefaultArcsEnabled(bool enabled)
{
  cond_default_arcs_enabled_ = enabled;
}

void
Variables::setBidirectInstPathsEnabled(bool enabled)
{
  bidirect_inst_paths_enabled_ = enabled;
}

void
Variables::setRecoveryRemovalChecksEnabled(bool enabled)
{
  recovery_removal_checks_enabled_ = enabled;
}

void
Variables::setGatedClkChecksEnabled(bool enabled)
{
  gated_clk_checks_enabled_ = enabled;
}

void
Variables::setDynamicLoopBreaking(bool enable)
{
  dynamic_loop_breaking_ = enable;
}

void
Variables::setPropagateAllClocks(bool prop)
{
  propagate_all_clks_ = prop;
}

void
Variables::setClkThruTristateEnabled(bool enable)
{
  clk_thru_tristate_enabled_ = enable;
}

void
Variables::setUseDefaultArrivalClock(bool enable)
{
  use_default_arrival_clock_ = enable;
}

////////////////////////////////////////////////////////////////

bool
Variables::pocvEnabled() const
{
  return pocv_mode_ != PocvMode::scalar;
}

void
Variables::setPocvMode(PocvMode mode)
{
  pocv_mode_ = mode;
}

void
Variables::setPocvQuantile(float quantile)
{
  pocv_quantile_ = quantile;
}

} // namespace sta
