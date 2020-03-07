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
#include "TimingModel.hh"

namespace sta {

class GateLinearModel : public GateTimingModel
{
public:
  GateLinearModel(float intrinsic, float resistance);
  // gate delay calculation
  virtual void gateDelay(const LibertyCell *cell,
			 const Pvt *pvt,
			 float load_cap, float in_slew,
			 float related_out_cap,
			 bool pocv_enabled,
			 // return values
			 ArcDelay &gate_delay,
			 Slew &drvr_slew) const;
  virtual void reportGateDelay(const LibertyCell *cell,
			       const Pvt *pvt,
			       float load_cap,
			       float in_slew,
			       float related_out_cap,
			       bool pocv_enabled,
			       int digits,
			       string *result) const;
  virtual float driveResistance(const LibertyCell *cell,
				const Pvt *pvt) const;

protected:
  virtual void setIsScaled(bool is_scaled);

  float intrinsic_;
  float resistance_;

private:
  DISALLOW_COPY_AND_ASSIGN(GateLinearModel);
};

class CheckLinearModel : public CheckTimingModel
{
public:
  explicit CheckLinearModel(float intrinsic);
  // Timing check margin delay calculation.
  virtual void checkDelay(const LibertyCell *cell,
			  const Pvt *pvt,
			  float from_slew,
			  float to_slew,
			  float related_out_cap,
			  bool pocv_enabled,
			  ArcDelay &margin) const;
  virtual void reportCheckDelay(const LibertyCell *cell,
				const Pvt *pvt,
				float from_slew,
				const char *from_slew_annotation,
				float to_slew,
				float related_out_cap,
				bool pocv_enabled,
				int digits,
				string *result) const;

protected:
  virtual void setIsScaled(bool is_scaled);

  float intrinsic_;

private:
  DISALLOW_COPY_AND_ASSIGN(CheckLinearModel);
};

} // namespace
