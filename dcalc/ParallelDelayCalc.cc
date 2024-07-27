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

#include "ParallelDelayCalc.hh"

#include "TimingArc.hh"
#include "Corner.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Liberty.hh"
#include "GraphDelayCalc.hh"

namespace sta {

ParallelDelayCalc::ParallelDelayCalc(StaState *sta):
  DelayCalcBase(sta)
{
}

ArcDcalcResultSeq
ParallelDelayCalc::gateDelays(ArcDcalcArgSeq &dcalc_args,
                              const LoadPinIndexMap &load_pin_index_map,
                              const DcalcAnalysisPt *dcalc_ap)
{
  if (dcalc_args.size() == 1) {
    ArcDcalcArg &dcalc_arg = dcalc_args[0];
    ArcDcalcResult dcalc_result =  gateDelay(dcalc_arg.drvrPin(), dcalc_arg.arc(),
                                             dcalc_arg.inSlew(), dcalc_arg.loadCap(),
                                             dcalc_arg.parasitic(),
                                             load_pin_index_map, dcalc_ap);
    ArcDcalcResultSeq dcalc_results;
    dcalc_results.push_back(dcalc_result);
    return dcalc_results;
  }
  return gateDelaysParallel(dcalc_args, load_pin_index_map, dcalc_ap);
}

ArcDcalcResultSeq
ParallelDelayCalc::gateDelaysParallel(ArcDcalcArgSeq &dcalc_args,
                                      const LoadPinIndexMap &load_pin_index_map,
                                      const DcalcAnalysisPt *dcalc_ap)
{
  size_t drvr_count = dcalc_args.size();
  ArcDcalcResultSeq dcalc_results(drvr_count);
  Slew slew_sum = 0.0;
  ArcDelay load_delay_sum = 0.0;
  vector<ArcDelay> intrinsic_delays(dcalc_args.size());
  vector<ArcDelay> load_delays(dcalc_args.size());
  for (size_t drvr_idx = 0; drvr_idx < drvr_count; drvr_idx++) {
    ArcDcalcArg &dcalc_arg = dcalc_args[drvr_idx];
    ArcDcalcResult &dcalc_result = dcalc_results[drvr_idx];
    const Pin *drvr_pin = dcalc_arg.drvrPin();
    const TimingArc *arc = dcalc_arg.arc();
    Slew in_slew = dcalc_arg.inSlew();

    ArcDcalcResult intrinsic_result = gateDelay(drvr_pin, arc, in_slew, 0.0, nullptr,
                                                load_pin_index_map, dcalc_ap);
    ArcDelay intrinsic_delay = intrinsic_result.gateDelay();
    intrinsic_delays[drvr_idx] = intrinsic_result.gateDelay();

    ArcDcalcResult gate_result = gateDelay(drvr_pin, arc, in_slew, dcalc_arg.loadCap(),
                                           dcalc_arg.parasitic(),
                                           load_pin_index_map, dcalc_ap);
    ArcDelay gate_delay = gate_result.gateDelay();
    Slew drvr_slew = gate_result.drvrSlew();
    ArcDelay load_delay = gate_delay - intrinsic_delay;
    load_delays[drvr_idx] = load_delay;

    if (!delayZero(load_delay))
      load_delay_sum += 1.0 / load_delay;
    if (!delayZero(drvr_slew))
      slew_sum += 1.0 / drvr_slew;

    dcalc_result.setLoadCount(load_pin_index_map.size());
    for (const auto [load_pin, load_idx] : load_pin_index_map) {
      dcalc_result.setWireDelay(load_idx, gate_result.wireDelay(load_idx));
      dcalc_result.setLoadSlew(load_idx, gate_result.loadSlew(load_idx));
    }
  }

  ArcDelay gate_load_delay = delayZero(load_delay_sum)
    ? delay_zero
    : 1.0 / load_delay_sum;
  ArcDelay drvr_slew = delayZero(slew_sum) ? delay_zero : 1.0 / slew_sum;

  for (size_t drvr_idx = 0; drvr_idx < drvr_count; drvr_idx++) {
    ArcDcalcResult &dcalc_result = dcalc_results[drvr_idx];
    dcalc_result.setGateDelay(intrinsic_delays[drvr_idx] + gate_load_delay);
    dcalc_result.setDrvrSlew(drvr_slew);
  }
  return dcalc_results;
}

} // namespace
