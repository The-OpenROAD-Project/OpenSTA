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

#include "TimingModel.hh"

namespace sta {

class GateLinearModel : public GateTimingModel
{
public:
  GateLinearModel(LibertyCell *cell,
                  float intrinsic,
                  float resistance);
  void gateDelay(const Pvt *pvt,
                 float in_slew,
                 float load_cap,
                 // Return values.
                 float &gate_delay,
                 float &drvr_slew) const override;
  void gateDelayPocv(const Pvt *pvt,
                     float in_slew,
                     float load_cap,
                     const MinMax *min_max,
                     PocvMode pocv_mode,
                     // Return values.
                     ArcDelay &gate_delay,
                     Slew &drvr_slew) const override;
  std::string reportGateDelay(const Pvt *pvt,
                              float in_slew,
                              float load_cap,
                              const MinMax *min_max,
                              PocvMode pocv_mode,
                              int digits) const override;
  float driveResistance(const Pvt *pvt) const override;

protected:
  void setIsScaled(bool is_scaled) override;

  float intrinsic_;
  float resistance_;
};

class CheckLinearModel : public CheckTimingModel
{
public:
  CheckLinearModel(LibertyCell *cell,
                   float intrinsic);
  ArcDelay checkDelay(const Pvt *pvt,
                      float from_slew,
                      float to_slew,
                      float related_out_cap,
                      const MinMax *min_max,
                      PocvMode pocv_mode) const override;
  std::string reportCheckDelay(const Pvt *pvt,
                               float from_slew,
                               std::string_view from_slew_annotation,
                               float to_slew,
                               float related_out_cap,
                               const MinMax *min_max,
                               PocvMode pocv_mode,
                               int digits) const override;

protected:
  void setIsScaled(bool is_scaled) override;

  float intrinsic_;
};

} // namespace
