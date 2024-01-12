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

#include "Map.hh"
#include "Set.hh"
#include "StaState.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SdcClass.hh"

namespace sta {

typedef Map<const Pin*, ClockSet> PinClksMap;
typedef Map<const Clock *, PinSet*> ClkPinsMap;

class Sta;

// Find clock network pins.
// This is not as reliable as Search::isClock but is much cheaper.
class ClkNetwork : public StaState
{
public:
  ClkNetwork(StaState *sta);
  ~ClkNetwork();
  void ensureClkNetwork();
  void clear();
  bool isClock(const Pin *pin) const;
  bool isClock(const Net *net) const;
  bool isIdealClock(const Pin *pin) const;
  bool isPropagatedClock(const Pin *pin) const;
  const ClockSet *clocks(const Pin *pin);
  const ClockSet *idealClocks(const Pin *pin);
  const PinSet *pins(const Clock *clk);
  void clkPinsInvalid();
  float idealClkSlew(const Pin *pin,
                     const RiseFall *rf,
                     const MinMax *min_max);

protected:
  void deletePinBefore(const Pin *pin);
  void connectPinAfter(const Pin *pin);
  void disconnectPinBefore(const Pin *pin);
  friend class Sta;

private:
  void findClkPins();
  void findClkPins(bool ideal_only,
		   PinClksMap &clk_pin_map);

  bool clk_pins_valid_;
  // pin -> clks
  PinClksMap pin_clks_map_;
  // pin -> ideal clks
  PinClksMap pin_ideal_clks_map_;
  // clock -> pins
  ClkPinsMap clk_pins_map_;
};

} // namespace
