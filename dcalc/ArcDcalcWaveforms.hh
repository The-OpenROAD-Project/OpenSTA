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

class Corner;
class DcalcAnalysisPt;

// Abstract class for the graph delay calculator traversal to interface
class ArcDcalcWaveforms
{
public:
  virtual Table1 inputWaveform(const Pin *in_pin,
                               const RiseFall *in_rf,
                               const Corner *corner,
                               const MinMax *min_max);
  virtual Table1 drvrWaveform(const Pin *in_pin,
                              const RiseFall *in_rf,
                              const Pin *drvr_pin,
                              const RiseFall *drvr_rf,
                              const Corner *corner,
                              const MinMax *min_max) = 0;
  virtual Table1 loadWaveform(const Pin *in_pin,
                              const RiseFall *in_rf,
                              const Pin *drvr_pin,
                              const RiseFall *drvr_rf,
                              const Pin *load_pin,
                              const Corner *corner,
                              const MinMax *min_max) = 0;
  virtual Table1 drvrRampWaveform(const Pin *in_pin,
                                  const RiseFall *in_rf,
                                  const Pin *drvr_pin,
                                  const RiseFall *drvr_rf,
                                  const Pin *load_pin,
                                  const Corner *corner,
                                  const MinMax *min_max);
};

} // namespace

