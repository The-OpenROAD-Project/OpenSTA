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

#pragma once

#include <vector>

#include "MinMax.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "Sta.hh"
#include "BoundedHeap.hh"

namespace sta {

class StaState;

class FanoutCheck
{
public:
  FanoutCheck();
  FanoutCheck(const Pin *pin,
              float fanout,
              float limit,
              float slack,
              const Mode *mode);
  bool isNull() { return pin_ == nullptr; }
  const Pin *pin() const { return pin_; }
  float fanout() const { return fanout_; }
  float limit() const { return limit_; }
  float slack() const { return slack_; }
  const Mode *mode() const { return mode_; }

private:
  const Pin *pin_;
  float fanout_;
  float limit_;
  float slack_;
  const Mode *mode_;
};

class FanoutCheckSlackLess
{
public:
  FanoutCheckSlackLess(const StaState *sta);
  bool operator()(const FanoutCheck &check1,
                  const FanoutCheck &check2) const;

private:
  const StaState *sta_;
};

using FanoutCheckSeq = std::vector<FanoutCheck>;
using FanoutCheckHeap = BoundedHeap<FanoutCheck, FanoutCheckSlackLess>;

class CheckFanouts
{
public:
  CheckFanouts(const Sta *sta);
  void clear();
  // Return pins with the min/max fanout limit slack.
  // net=null check all nets
  FanoutCheckSeq &check(const Net *net,
                        size_t max_count,
                        bool violators,
                        const ModeSeq &modes,
                        const MinMax *min_max);
  FanoutCheck check(const Pin *pin,
                    const Mode*mode,
                    const MinMax *min_max) const;

protected:
  void checkNet(const Net *net,
                bool violators,
                const ModeSeq &modes,
                const MinMax *min_max);
  void checkAll(bool violators,
                const ModeSeq &modes,
                const MinMax *min_max);
  void checkInst(const Instance *inst,
                 bool violators,
                 const ModeSeq &modes,
                 const MinMax *min_max);
  void checkPin(const Pin *pin,
                bool violators,
                const ModeSeq &modes,
                const MinMax *min_max);
  bool checkPin(const Pin *pin,
                const Mode *mode) const;

  void findLimit(const Pin *pin,
                 const Sdc *sdc,
                 const MinMax *min_max,
                 // Return values.
                 float &limit,
                 bool &limit_exists) const;
  float fanoutLoad(const Pin *pin) const;

  const Sta *sta_;
  FanoutCheckSeq checks_;
  FanoutCheckHeap heap_;
};

} // namespace

