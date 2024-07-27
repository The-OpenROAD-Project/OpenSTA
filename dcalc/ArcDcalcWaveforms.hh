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

#include "MinMax.hh"
#include "TableModel.hh"
#include "NetworkClass.hh"

namespace sta {

class StaState;
class Corner;
class DcalcAnalysisPt;
class ArcDcalcArg;

// Abstract class for delay calculation waveforms for ploting.
class ArcDcalcWaveforms
{
public:
  // Record waveform for drvr/load pin.
  virtual void watchPin(const Pin *pin) = 0;
  virtual void clearWatchPins() = 0;
  virtual PinSeq watchPins() const = 0;
  virtual Waveform watchWaveform(const Pin *pin) = 0;

protected:
  Waveform inputWaveform(ArcDcalcArg &dcalc_arg,
                         const DcalcAnalysisPt *dcalc_ap,
                         const StaState *sta);
};

} // namespace

