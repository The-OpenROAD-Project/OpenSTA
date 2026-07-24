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

#include "ArcDcalcWaveforms.hh"
#include "LumpedCapDelayCalc.hh"

namespace sta {

using WatchPinValuesMap = std::map<const Pin*, FloatSeq, PinIdLess>;

ArcDelayCalc *
makeCcsCeffDelayCalc(StaState *sta);

class CcsCeffDelayCalc : public LumpedCapDelayCalc,
                         public ArcDcalcWaveforms
{
public:
  CcsCeffDelayCalc(StaState *sta);
  CcsCeffDelayCalc(const CcsCeffDelayCalc &dcalc);
  ~CcsCeffDelayCalc() override;
  ArcDelayCalc *copy() override;
  std::string_view name() const override { return "ccs_ceff"; }
  bool reduceSupported() const override { return true; }
  ArcDcalcResult gateDelay(const Pin *drvr_pin,
                           const TimingArc *arc,
                           const Slew &in_slew,
                           float load_cap,
                           const Parasitic *parasitic,
                           const LoadPinIndexMap &load_pin_index_map,
                           const Scene *scene,
                           const MinMax *min_max) override;
  std::string reportGateDelay(const Pin *drvr_pin,
                              const TimingArc *arc,
                              const Slew &in_slew,
                              float load_cap,
                              const Parasitic *parasitic,
                              const LoadPinIndexMap &load_pin_index_map,
                              const Scene *scene,
                              const MinMax *min_max,
                              int digits) override;

  // OpenROAD-fork: ccs-delay2 -- read-only accessor exposing the effective
  // capacitance (region 0) that the most recent successful CCS gateDelay()
  // converged to. Returns a negative value if the last gateDelay() fell back
  // to the NLDM table calculator (no CCS waveforms / out-of-bounds / no
  // pi-model parasitic). Purely additive: does not affect any delay value or
  // the active delay path.
  double effectiveCapacitance() const override { return last_ceff_; }

  // OpenROAD-fork: ccs-receiver -- process-global toggle for the CCS
  // receiver-capacitance model in the Ceff/charge solve. Default OFF so the
  // ccs_ceff engine is byte-identical to the constant-Cp behavior. When ON,
  // the per-region far-cap contribution of any load pin that carries a CCS
  // receiver_capacitance model is swapped from its constant NLDM pin cap to
  // the library's region-dependent receiver cap (Cr1 in the active/Miller
  // region below the receiver threshold, Cr2 in the settled region). Pins
  // without a receiver model keep their constant NLDM contribution. The flag
  // is static because the active ArcDelayCalc is selected by name and copied
  // per-thread; a process-global toggle keeps every copy consistent without
  // threading new state through the copy ctor.
  static void setReceiverModelEnabled(bool enabled);
  static bool receiverModelEnabled();

  // Record waveform for drvr/load pin.
  void watchPin(const Pin *pin) override;
  void clearWatchPins() override;
  PinSeq watchPins() const override;
  Waveform watchWaveform(const Pin *pin) override;

protected:
  using Region = std::vector<double>;

  void gateDelaySlew(const LibertyLibrary *drvr_library,
                     // Return values.
                     double &gate_delay,
                     double &drvr_slew);
  void initRegions(const LibertyLibrary *drvr_library,
                   const RiseFall *rf);
  void findCsmWaveform();
  void loadDelaySlew(const Pin *load_pin,
                     const LibertyLibrary *drvr_library,
                     double drvr_slew,
                     // Return values.
                     double &wire_delay,
                     double &load_slew);
  void loadDelaySlew(const Pin *load_pin,
                     double &drvr_slew,
                     float elmore,
                     // Return values.
                     double &delay,
                     double &slew);
  double findVlTime(double v,
                    double elmore);
  bool makeWaveformPreamble(const Pin *in_pin,
                            const RiseFall *in_rf,
                            const Pin *drvr_pin,
                            const RiseFall *drvr_rf,
                            const Scene *scene,
                            const MinMax *min_max);
  Waveform drvrWaveform();
  Waveform loadWaveform(const Pin *load_pin);
  Waveform drvrRampWaveform(const Pin *in_pin,
                            const RiseFall *in_rf,
                            const Pin *drvr_pin,
                            const RiseFall *drvr_rf,
                            const Pin *load_pin,
                            const Scene *scene,
                            const MinMax *min_max);
  void vLoad(double t,
             double elmore,
             // Return values.
             double &vl,
             double &dvl_dt);
  double vLoad(double t,
               double elmore);
  void fail(std::string_view reason);

  const Pin *drvr_pin_;
  const RiseFall *drvr_rf_;
  double in_slew_;
  double load_cap_;
  Parasitics *parasitics_;
  const Parasitic *parasitic_;

  OutputWaveforms *output_waveforms_{nullptr};
  double ref_time_;
  float vdd_;
  float vth_;
  float vl_;
  float vh_;

  float c2_;
  float rpi_;
  float c1_;

  size_t region_count_{0};
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
  bool vl_fail_{false};
  // OpenROAD-fork: ccs-delay2 -- last converged effective cap (region 0), or
  // a negative sentinel when the previous gateDelay() used the NLDM fallback.
  double last_ceff_{-1.0};
  // OpenROAD-fork: ccs-receiver -- per-solve receiver-capacitance contributions
  // summed over the load pins that carry a CCS receiver_capacitance model.
  // recv_has_model_ is true when at least one load pin contributed; when false
  // the Ceff solve falls back to the constant-Cp behavior (byte-identical).
  //   recv_nldm_cap_ : sum of the NLDM static pin caps of those pins (the
  //                    quantity already folded into c1_, to be removed).
  //   recv_cr1_cap_  : sum of segment-0 (Cr1) receiver caps  (active/Miller).
  //   recv_cr2_cap_  : sum of segment-1 (Cr2) receiver caps  (settled region).
  // These are only consulted when receiverModelEnabled() is true.
  bool recv_has_model_{false};
  double recv_nldm_cap_{0.0};
  double recv_cr1_cap_{0.0};
  double recv_cr2_cap_{0.0};
  // Compute the per-segment far-cap (c1) value for region/segment seg_idx,
  // applying the receiver model when enabled; otherwise returns c1_.
  double regionFarCap(size_t seg_idx) const;
  // Populate recv_* from the load pins; no-op (clears) when the flag is off.
  void initReceiverModel(const LoadPinIndexMap &load_pin_index_map,
                         const Scene *scene,
                         const MinMax *min_max);
  // Waveform recording.
  WatchPinValuesMap watch_pin_values_;

  const Unit *capacitance_unit_;
  // Delay calculator to use when ccs waveforms are missing from liberty.
  ArcDelayCalc *table_dcalc_;
};

} // namespace sta
