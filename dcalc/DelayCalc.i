%module dcalc

%{

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

#include "Sta.hh"
#include "ArcDelayCalc.hh"
#include "dcalc/ArcDcalcWaveforms.hh"
#include "dcalc/PrimaDelayCalc.hh"

%}

%inline %{

StringSeq
delay_calc_names()
{
  return sta::delayCalcNames();
}

bool
is_delay_calc_name(const char *alg)
{
  return sta::isDelayCalcName(alg);
}

void
set_delay_calculator_cmd(const char *alg)
{
  sta::Sta::sta()->setArcDelayCalc(alg);
}

void
set_delay_calc_incremental_tolerance(float tol)
{
  sta::Sta::sta()->setIncrementalDelayTolerance(tol);
}

string
report_delay_calc_cmd(Edge *edge,
		      TimingArc *arc,
		      const Corner *corner,
		      const MinMax *min_max,
		      int digits)
{
  cmdLinkedNetwork();
  return Sta::sta()->reportDelayCalc(edge, arc, corner, min_max, digits);
}

////////////////////////////////////////////////////////////////

Table1
ccs_input_waveform(const Pin *in_pin,
                   const RiseFall *in_rf,
                   const Corner *corner,
                   const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  ArcDcalcWaveforms *arc_dcalc = dynamic_cast<ArcDcalcWaveforms*>(sta->arcDelayCalc());
  if (arc_dcalc)
    return arc_dcalc->inputWaveform(in_pin, in_rf, corner, min_max);
  else
    return Table1();
}

Table1
ccs_driver_waveform(const Pin *in_pin,
                    const RiseFall *in_rf,
                    const Pin *drvr_pin,
                    const RiseFall *drvr_rf,
                    const Corner *corner,
                    const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  ArcDcalcWaveforms *arc_dcalc = dynamic_cast<ArcDcalcWaveforms*>(sta->arcDelayCalc());
  if (arc_dcalc)
    return arc_dcalc->drvrWaveform(in_pin, in_rf, drvr_pin, drvr_rf, corner, min_max);
  else
    return Table1();
}

Table1
ccs_driver_ramp_waveform(const Pin *in_pin,
                         const RiseFall *in_rf,
                         const Pin *drvr_pin,
                         const RiseFall *drvr_rf,
                         const Pin *load_pin,
                         const Corner *corner,
                         const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  ArcDcalcWaveforms *arc_dcalc = dynamic_cast<ArcDcalcWaveforms*>(sta->arcDelayCalc());
  if (arc_dcalc)
    return arc_dcalc->drvrRampWaveform(in_pin, in_rf, drvr_pin, drvr_rf,
                                       load_pin, corner, min_max);
  else
    return Table1();
}

Table1
ccs_load_waveform(const Pin *in_pin,
                  const RiseFall *in_rf,
                  const Pin *drvr_pin,
                  const RiseFall *drvr_rf,
                  const Pin *load_pin,
                  const Corner *corner,
                  const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  ArcDcalcWaveforms *arc_dcalc = dynamic_cast<ArcDcalcWaveforms*>(sta->arcDelayCalc());
  if (arc_dcalc)
    return arc_dcalc->loadWaveform(in_pin, in_rf, drvr_pin, drvr_rf,
                                   load_pin, corner, min_max);
  else
    return Table1();
}

void
set_prima_reduce_order(size_t order)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  PrimaDelayCalc *dcalc = dynamic_cast<PrimaDelayCalc*>(sta->arcDelayCalc());
  if (dcalc) {
    dcalc->setPrimaReduceOrder(order);
    sta->delaysInvalid();
  }
}

%} // inline
