// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include "Machine.hh"
#include "ClockInsertion.hh"

namespace sta {

ClockInsertion::ClockInsertion(const Clock *clk, const Pin *pin) :
  clk_(clk),
  pin_(pin)
{
}

void
ClockInsertion::setDelay(const TransRiseFallBoth *tr,
			 const MinMaxAll *min_max,
			 const EarlyLateAll *early_late, float delay)
{
  EarlyLateIterator el_iter(early_late);
  while (el_iter.hasNext()) {
    EarlyLate *el = el_iter.next();
    delays_[el->index()].setValue(tr, min_max, delay);
  }
}

float
ClockInsertion::delay(const TransRiseFall *tr, const MinMax *min_max,
		      const EarlyLate *early_late)
{
  float insertion;
  bool exists;
  delays_[early_late->index()].value(tr, min_max, insertion, exists);
  if (exists)
    return insertion;
  else
    return 0.0;
}

void
ClockInsertion::delay(const TransRiseFall *tr, const MinMax *min_max,
		      const EarlyLate *early_late,
		      // Return values.
		      float &insertion, bool &exists)

{
  delays_[early_late->index()].value(tr, min_max, insertion, exists);
  if (!exists)
    insertion = 0.0;
}

void
ClockInsertion::setDelay(const TransRiseFall *tr,
			 const MinMax *min_max,
			 const EarlyLate *early_late, float delay)
{
  delays_[early_late->index()].setValue(tr, min_max, delay);
}

void
ClockInsertion::setDelays(RiseFallMinMax *delays)
{
  EarlyLateIterator el_iter;
  while (el_iter.hasNext()) {
    EarlyLate *el = el_iter.next();
    delays_[el->index()].setValues(delays);
  }
}

RiseFallMinMax *
ClockInsertion::delays(const EarlyLate *early_late)
{
  return &delays_[early_late->index()];
}

} // namespace
