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

#include "PinPair.hh"

#include "Network.hh"

namespace sta {

PinPairLess::PinPairLess(const Network *network) :
  network_(network)
{
}

bool
PinPairLess::operator()(const PinPair &pair1,
			const PinPair &pair2) const
{
  const Pin *pair1_pin1 = pair1.first;
  const Pin *pair1_pin2 = pair1.second;
  const Pin *pair2_pin1 = pair2.first;
  const Pin *pair2_pin2 = pair2.second;
  return (pair1_pin1 == nullptr && pair2_pin1)
    || (pair1_pin1 && pair2_pin1
        && (network_->id(pair1_pin1) < network_->id(pair2_pin1)
            || (pair1_pin1 == pair2_pin1
                && ((pair1_pin2 == nullptr && pair2_pin2)
                    || (pair1_pin2 && pair2_pin2
                        && network_->id(pair1_pin2) < network_->id(pair2_pin2))))));
}

bool
PinPairEqual::operator()(const PinPair &pair1,
			 const PinPair &pair2) const
{
  return pair1.first == pair2.first
    && pair1.second == pair2.second;
}

PinPairHash::PinPairHash(const Network *network) :
  network_(network)
{
}

size_t
PinPairHash::operator()(const PinPair &pair) const
{
  size_t hash = hash_init_value;
  hashIncr(hash, network_->id(pair.first));
  hashIncr(hash, network_->id(pair.second));
  return hash;
}

PinPairSet::PinPairSet(const Network *network) :
  Set<PinPair, PinPairLess>(PinPairLess(network))
{
}

} // namespace
