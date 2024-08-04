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

#include <memory>

#include "ArcDcalcWaveforms.hh"

#include "Report.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "ArcDelayCalc.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"

namespace sta {

using std::make_shared;

Waveform
ArcDcalcWaveforms::inputWaveform(ArcDcalcArg &dcalc_arg,
                                 const DcalcAnalysisPt *dcalc_ap,
                                 const StaState *sta)
{
  const Network *network = sta->network();
  Graph *graph = sta->graph();
  Report *report = sta->report();
  const Pin *in_pin = dcalc_arg.inPin();
  LibertyPort *port = network->libertyPort(in_pin);
  if (port) {
    const RiseFall *in_rf = dcalc_arg.inEdge();
    DriverWaveform *driver_waveform = port->driverWaveform(in_rf);
    if (driver_waveform) {
      const Vertex *in_vertex = graph->pinLoadVertex(in_pin);
      GraphDelayCalc *graph_dcalc = sta->graphDelayCalc();
      Slew in_slew = graph_dcalc->edgeFromSlew(in_vertex, in_rf,
                                               dcalc_arg.arc()->role(), dcalc_ap);
      LibertyLibrary *library = port->libertyLibrary();
      float vdd;
      bool vdd_exists;
      library->supplyVoltage("VDD", vdd, vdd_exists);
      if (!vdd_exists)
        report->error(1751, "VDD not defined in library %s", library->name());
      Waveform in_waveform = driver_waveform->waveform(delayAsFloat(in_slew));
      // Delay time axis.
      FloatSeq *time_values = new FloatSeq;
      for (float time : *in_waveform.axis1()->values())
        time_values->push_back(time + dcalc_arg.inputDelay());
      TableAxisPtr time_axis = make_shared<TableAxis>(TableAxisVariable::time, time_values);
      // Scale the waveform from 0:vdd.
      FloatSeq *scaled_values = new FloatSeq;
      for (float value : *in_waveform.values()) {
        float scaled_value = (in_rf == RiseFall::rise())
          ? value * vdd
          : (1.0 - value) * vdd;
        scaled_values->push_back(scaled_value);
      }
      return Waveform(scaled_values, time_axis);
    }
  }
  return Waveform();
}

} // namespace
