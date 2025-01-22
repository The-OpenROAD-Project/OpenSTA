// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

%module dcalc

%{

#include "DelayCalc.hh"
#include "ArcDelayCalc.hh"
#include "dcalc/ArcDcalcWaveforms.hh"
#include "dcalc/PrimaDelayCalc.hh"
#include "Sta.hh"

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
  Sta::sta()->setIncrementalDelayTolerance(tol);
}

string
report_delay_calc_cmd(Edge *edge,
		      TimingArc *arc,
		      const Corner *corner,
		      const MinMax *min_max,
		      int digits)
{
  Sta *sta = Sta::sta();
  return sta->reportDelayCalc(edge, arc, corner, min_max, digits);
}

void
set_prima_reduce_order(size_t order)
{
  Sta *sta = Sta::sta();
  PrimaDelayCalc *dcalc = dynamic_cast<PrimaDelayCalc*>(sta->arcDelayCalc());
  if (dcalc) {
    dcalc->setPrimaReduceOrder(order);
    sta->delaysInvalid();
  }
}

void
find_delays()
{
  Sta::sta()->findDelays();
}

void
delays_invalid()
{
  Sta *sta = Sta::sta();
  sta->delaysInvalid();
}

%} // inline
