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

#include "LumpedCapDelayCalc.hh"
#include "ArcDcalcWaveforms.hh"

namespace sta {

using std::vector;

typedef map<const Pin*, FloatSeq, PinIdLess> WatchPinValuesMap;

ArcDelayCalc *
makeCcsCeffDelayCalc(StaState *sta);

class CcsCeffDelayCalc : public LumpedCapDelayCalc,
                         public ArcDcalcWaveforms
{
public:
  CcsCeffDelayCalc(StaState *sta);
  virtual ~CcsCeffDelayCalc();
  ArcDelayCalc *copy() override;
  const char *name() const override { return "ccs_ceff"; }
  bool reduceSupported() const override { return true; }
  ArcDcalcResult gateDelay(const Pin *drvr_pin,
                           const TimingArc *arc,
                           const Slew &in_slew,
                           float load_cap,
                           const Parasitic *parasitic,
                           const LoadPinIndexMap &load_pin_index_map,
                           const DcalcAnalysisPt *dcalc_ap) override;
  string reportGateDelay(const Pin *drvr_pin,
                         const TimingArc *arc,
                         const Slew &in_slew,
                         float load_cap,
                         const Parasitic *parasitic,
                         const LoadPinIndexMap &load_pin_index_map,
                         const DcalcAnalysisPt *dcalc_ap,
                         int digits) override;

  // Record waveform for drvr/load pin.
  void watchPin(const Pin *pin) override;
  void clearWatchPins() override;
  PinSeq watchPins() const override;
  Waveform watchWaveform(const Pin *pin) override;

protected:
  typedef vector<double> Region;

  void gateDelaySlew(const LibertyLibrary *drvr_library,
                     const RiseFall *rf,
                     // Return values.
                     ArcDelay &gate_delay,
                     Slew &drvr_slew);
  void initRegions(const LibertyLibrary *drvr_library,
                   const RiseFall *rf);
  void findCsmWaveform();
  ArcDcalcResult makeResult(const LibertyLibrary *drvr_library,
                            const RiseFall *rf,
                            ArcDelay &gate_delay,
                            Slew &drvr_slew,
                            const LoadPinIndexMap &load_pin_index_map);
  void loadDelaySlew(const Pin *load_pin,
                     const LibertyLibrary *drvr_library,
                     const RiseFall *rf,
                     Slew &drvr_slew,
                     // Return values.
                     ArcDelay &wire_delay,
                     Slew &load_slew);
  void loadDelaySlew(const Pin *load_pin,
                     Slew &drvr_slew,
                     float elmore,
                     // Return values.
                     ArcDelay &delay,
                     Slew &slew);
  double findVlTime(double v,
                    double elmore);
  bool makeWaveformPreamble(const Pin *in_pin,
                            const RiseFall *in_rf,
                            const Pin *drvr_pin,
                            const RiseFall *drvr_rf,
                            const Corner *corner,
                            const MinMax *min_max);
  Waveform drvrWaveform();
  Waveform loadWaveform(const Pin *load_pin);
  Waveform drvrRampWaveform(const Pin *in_pin,
                            const RiseFall *in_rf,
                            const Pin *drvr_pin,
                            const RiseFall *drvr_rf,
                            const Pin *load_pin,
                            const Corner *corner,
                            const MinMax *min_max);
  void vl(double t,
          double elmore,
          // Return values.
          double &vl,
          double &dvl_dt);
  double vl(double t,
           double elmore);
  void fail(const char *reason);

  const Pin *drvr_pin_;
  const RiseFall *drvr_rf_;
  double in_slew_;
  double load_cap_;
  const Parasitic *parasitic_;

  OutputWaveforms *output_waveforms_;
  double ref_time_;
  float vdd_;
  float vth_;
  float vl_;
  float vh_;

  float c2_;
  float rpi_;
  float c1_;

  size_t region_count_;
  size_t region_vl_idx_;
  size_t region_vth_idx_;
  size_t region_vh_idx_;

  Region region_volts_;
  Region region_ceff_;
  Region region_times_;
  Region region_begin_times_;
  Region region_end_times_;
  Region region_time_offsets_;
  Region region_ramp_times_;
  Region region_ramp_slopes_;
  bool vl_fail_;
  // Waveform recording.
  WatchPinValuesMap watch_pin_values_;

  const Unit *capacitance_unit_;
  // Delay calculator to use when ccs waveforms are missing from liberty.
  ArcDelayCalc *table_dcalc_;
};

} // namespace
