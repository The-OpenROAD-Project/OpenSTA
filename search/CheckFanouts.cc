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

#include "CheckFanouts.hh"

#include "ContainerHelpers.hh"
#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Mode.hh"
#include "InputDrive.hh"
#include "Sim.hh"
#include "PortDirection.hh"
#include "Graph.hh"
#include "Search.hh"
#include "ClkNetwork.hh"

namespace sta {

CheckFanouts::CheckFanouts(const Sta *sta) :
  sta_(sta),
  heap_(0, FanoutCheckSlackLess(sta))
{
}

void
CheckFanouts::clear()
{
  checks_.clear();
  heap_.clear();
}

FanoutCheck
CheckFanouts::check(const Pin *pin,
                    const Mode *mode,
                    const MinMax *min_max) const
{
  FanoutCheck min_slack_check;
  float fanout = fanoutLoad(pin);
  if (checkPin(pin, mode)) {
    float limit;
    bool limit_exists;
    findLimit(pin, mode->sdc(), min_max, limit, limit_exists);
    if (limit_exists) {
      float slack = (min_max == MinMax::max())
        ? limit - fanout
        : fanout - limit;
      return FanoutCheck(pin, fanout, limit, slack, mode);
    }
  }
  return FanoutCheck();
}

// return the tightest limit.
void
CheckFanouts::findLimit(const Pin *pin,
                        const Sdc *sdc,
                        const MinMax *min_max,
                        // Return values.
                        float &limit,
                        bool &exists) const
{
  const Network *network = sta_->network();

  limit = min_max->initValue();
  exists = false;

  // Default to top ("design") limit.
  // Applies to input ports as well as instance outputs.
  Cell *top_cell = network->cell(network->topInstance());
  sdc->fanoutLimit(top_cell, min_max,
                   limit, exists);

  float limit1;
  bool exists1;
  if (network->isTopLevelPort(pin)) {
    Port *port = network->port(pin);
    sdc->fanoutLimit(port, min_max, limit1, exists1);
    if (exists1
        && (!exists
            || min_max->compare(limit, limit1))) {
      limit = limit1;
      exists = true;
    }
    InputDrive *drive = sdc->findInputDrive(port);
    if (drive) {
      for (auto rf : RiseFall::range()) {
        const LibertyCell *cell;
        const LibertyPort *from_port;
        float *from_slews;
        const LibertyPort *to_port;
        drive->driveCell(rf, min_max, cell, from_port, from_slews, to_port);
        if (to_port) {
          to_port->fanoutLimit(min_max, limit1, exists1);
          if (!exists1
              && min_max == MinMax::max()
              && to_port->direction()->isAnyOutput())
            to_port->libertyLibrary()->defaultMaxFanout(limit1, exists1);
          if (exists1
              && (!exists
                  || min_max->compare(limit, limit1))) {
            limit = limit1;
            exists = true;
          }
        }
      }
    }
  }
  else {
    Cell *cell = network->cell(network->instance(pin));
    sdc->fanoutLimit(cell, min_max,
                     limit1, exists1);
    if (exists1
        && (!exists
            || min_max->compare(limit, limit1))) {
      limit = limit1;
      exists = true;
    }
    LibertyPort *port = network->libertyPort(pin);
    if (port) {
      port->fanoutLimit(min_max, limit1, exists1);
      if (!exists1
          && min_max == MinMax::max()
          && port->direction()->isAnyOutput())
        port->libertyLibrary()->defaultMaxFanout(limit1, exists1);
      if (exists1
          && (!exists
              || min_max->compare(limit, limit1))) {
        limit = limit1;
        exists = true;
      }
    }
  }
}

float
CheckFanouts::fanoutLoad(const Pin *pin) const
{
  float fanout = 0;
  const Network *network = sta_->network();
  NetConnectedPinIterator *pin_iter = network->connectedPinIterator(pin);
  while (pin_iter->hasNext()) {
    const Pin *fanout_pin = pin_iter->next();
    if (network->isLoad(fanout_pin)
        && !network->isTopLevelPort(fanout_pin)) {
      LibertyPort *port = network->libertyPort(fanout_pin);
      if (port) {
        float fanout_load;
        bool exists;
        port->fanoutLoad(fanout_load, exists);
        if (!exists) {
          LibertyLibrary *lib = port->libertyLibrary();
          lib->defaultFanoutLoad(fanout_load, exists);
        }
        if (exists)
          fanout += fanout_load;
      }
      else
        fanout += 1;
    }
  }
  delete pin_iter;
  return fanout;
}

////////////////////////////////////////////////////////////////

FanoutCheckSeq &
CheckFanouts::check(const Net *net,
                   size_t max_count,
                   bool violators,
                   const ModeSeq &modes,
                   const MinMax *min_max)
{
  clear();
  if (!violators)
    heap_.setMaxSize(max_count);

  if (net)
    checkNet(net, violators, modes, min_max);
  else
    checkAll(violators, modes, min_max);

  if (violators)
    sort(checks_, FanoutCheckSlackLess(sta_));
  else
    checks_ = heap_.extract();
  return checks_;
}

void
CheckFanouts::checkNet(const Net *net,
                       bool violators,
                       const ModeSeq &modes,
                       const MinMax *min_max)
{
  const Network *network = sta_->network();
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      checkPin(pin, violators, modes, min_max);
    }
    delete pin_iter;
  }
}

void
CheckFanouts::checkAll(bool violators,
                       const ModeSeq &modes,
                       const MinMax *min_max)
{
  const Network *network = sta_->network();
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    const Instance *inst = inst_iter->next();
    checkInst(inst, violators, modes, min_max);
  }
  delete inst_iter;
  // Check top level ports.
  checkInst(network->topInstance(), violators, modes, min_max);
}

void
CheckFanouts::checkInst(const Instance *inst,
                        bool violators,
                        const ModeSeq &modes,
                        const MinMax *min_max)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    checkPin(pin, violators, modes, min_max);
  }
  delete pin_iter;
}

void
CheckFanouts::checkPin(const Pin *pin,
                       bool violators,
                       const ModeSeq &modes,
                       const MinMax *min_max)
{
  for (const Mode *mode : modes) {
    if (checkPin(pin, mode)) {
      FanoutCheck fanout_check = check(pin, mode, min_max);
      if (!fanout_check.isNull()) {
        if (violators) {
          if (fanout_check.slack() < 0.0)
            checks_.push_back(fanout_check);
        }
        else
          heap_.insert(fanout_check);
      }
    }
  }
}

bool
CheckFanouts::checkPin(const Pin *pin,
                       const Mode *mode) const
{
  const Network *network = sta_->network();
  return network->isDriver(pin)
    && !mode->sim()->isConstant(pin)
    && !mode->sdc()->isDisabledConstraint(pin)
    && !mode->clkNetwork()->isIdealClock(pin);
}

////////////////////////////////////////////////////////////////

FanoutCheck::FanoutCheck() :
  pin_(nullptr),
  fanout_(0.0),
  limit_(INF),
  slack_(INF),
  mode_(nullptr)
{
}

FanoutCheck::FanoutCheck(const Pin *pin,
                         float fanout,
                         float limit,
                         float slack,
                         const Mode *mode) :
  pin_(pin),
  fanout_(fanout),
  limit_(limit),
  slack_(slack),
  mode_(mode)
{
}

////////////////////////////////////////////////////////////////

FanoutCheckSlackLess::FanoutCheckSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
FanoutCheckSlackLess::operator()(const FanoutCheck &check1,
                                 const FanoutCheck &check2) const
{
  return fuzzyLess(check1.slack(), check2.slack())
    || (fuzzyEqual(check1.slack(), check2.slack())
        // Break ties for the sake of regression stability.
        && sta_->network()->pinLess(check1.pin(), check2.pin()));
}

} // namespace
