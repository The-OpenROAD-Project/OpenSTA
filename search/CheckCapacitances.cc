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

#include "CheckCapacitances.hh"

#include "ContainerHelpers.hh"
#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Mode.hh"
#include "InputDrive.hh"
#include "GraphDelayCalc.hh"
#include "StaState.hh"
#include "Scene.hh"
#include "PortDirection.hh"
#include "Sim.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "ClkNetwork.hh"
#include "Transition.hh"
#include "BoundedHeap.hh"

namespace sta {

class CapacitanceCheckSlackLess
{
public:
  CapacitanceCheckSlackLess(const StaState *sta);
  bool operator()(const CapacitanceCheck &check1,
                  const CapacitanceCheck &check2) const;

private:
  const StaState *sta_;
};

CapacitanceCheckSlackLess::CapacitanceCheckSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
CapacitanceCheckSlackLess::operator()(const CapacitanceCheck &check1,
                                      const CapacitanceCheck &check2) const
{
  return fuzzyLess(check1.slack(), check2.slack())
    || (fuzzyEqual(check1.slack(), check2.slack())
        // Break ties for the sake of regression stability.
        && sta_->network()->pinLess(check1.pin(), check2.pin()));
}

////////////////////////////////////////////////////////////////

CheckCapacitances::CheckCapacitances(const StaState *sta) :
  sta_(sta)
{
}

void
CheckCapacitances::clear()
{
  checks_.clear();
}

CapacitanceCheck
CheckCapacitances::check(const Pin *pin,
                         const SceneSeq &scenes,
                         const MinMax *min_max) const
{
  return check(pin, false, scenes, min_max);
}

CapacitanceCheck
CheckCapacitances::check(const Pin *pin,
                         bool violators,
                         const SceneSeq &scenes,
                         const MinMax *min_max) const
{
  CapacitanceCheck min_slack_check(nullptr, 0.0, min_max->initValue(),
                                   MinMax::min()->initValue(), nullptr, nullptr);
  GraphDelayCalc *dcalc = sta_->graphDelayCalc();

  for (const Scene *scene : scenes) {
    if (checkPin(pin, scene)) {
      float limit;
      bool limit_exists;
      findLimit(pin, scene, min_max, limit, limit_exists);
      if (limit_exists) {
        for (const RiseFall *rf : RiseFall::range()) {
          float cap = dcalc->loadCap(pin, scene, min_max);
          float slack = (min_max == MinMax::max())
            ? limit - cap : cap - limit;
          if ((!violators || fuzzyLess(slack, 0.0))
              && (min_slack_check.pin() == nullptr
                  || fuzzyLess(slack, min_slack_check.slack())
                  // Break ties for the sake of regression stability.
                  || (fuzzyEqual(slack, min_slack_check.slack())
                      && rf->index() < min_slack_check.rf()->index())))
            min_slack_check = CapacitanceCheck(pin, cap, limit, slack, scene, rf);
        }
      }
    }
  }
  return min_slack_check;
}

// Return the tightest limit.
void
CheckCapacitances::findLimit(const Pin *pin,
                            const Scene *scene,
                            const MinMax *min_max,
                            // Return values.
                            float &limit,
                            bool &exists) const
{
  const Network *network = sta_->network();
  Sdc *sdc = scene->sdc();

  // Default to top ("design") limit.
  Cell *top_cell = network->cell(network->topInstance());
  sdc->capacitanceLimit(top_cell, min_max,
                        limit, exists);

  float limit1;
  bool exists1;
  if (network->isTopLevelPort(pin)) {
    Port *port = network->port(pin);
    sdc->capacitanceLimit(port, min_max, limit1, exists1);
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
          const LibertyPort *scene_port = to_port->scenePort(scene, min_max);
          scene_port->capacitanceLimit(min_max, limit1, exists1);
          if (!exists1
              && scene_port->direction()->isAnyOutput()
              && min_max == MinMax::max())
            scene_port->libertyLibrary()->defaultMaxCapacitance(limit1, exists1);
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
    sdc->capacitanceLimit(cell, min_max,
                          limit1, exists1);
    if (exists1
        && (!exists
            || min_max->compare(limit, limit1))) {
        limit = limit1;
        exists = true;
    }
    LibertyPort *port = network->libertyPort(pin);
    if (port) {
      LibertyPort *scene_port = port->scenePort(scene, min_max);
      scene_port->capacitanceLimit(min_max, limit1, exists1);
      if (!exists1
          && port->direction()->isAnyOutput())
        scene_port->libertyLibrary()->defaultMaxCapacitance(limit1, exists1);
      if (exists1
          && (!exists
              || min_max->compare(limit, limit1))) {
        limit = limit1;
        exists = true;
      }
    }
  }
}

////////////////////////////////////////////////////////////////

CapacitanceCheckSeq &
CheckCapacitances::check(const Net *net,
                         size_t max_count,
                         bool violations,
                         const SceneSeq &scenes,
                         const MinMax *min_max)
{
  clear();
  if (violations)
    return checkViolations(net, scenes, min_max);
  else
    return checkMaxCount(net, max_count, scenes, min_max);
}

CapacitanceCheckSeq &
CheckCapacitances::checkViolations(const Net *net,
                                    const SceneSeq &scenes,
                                    const MinMax *min_max)
{
  const Network *network = sta_->network();
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      CapacitanceCheck cap_check = check(pin, true, scenes, min_max);
      if (!cap_check.isNull())
        checks_.push_back(cap_check);
    }
    delete pin_iter;
  }
  else {
    LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
    while (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      checkCapLimits(inst, true, scenes, min_max);
    }
    delete inst_iter;
    // Check top level ports.
    checkCapLimits(network->topInstance(), true, scenes, min_max);
  }

  sort(checks_, CapacitanceCheckSlackLess(sta_));
  return checks_;
}

CapacitanceCheckSeq &
CheckCapacitances::checkMaxCount(const Net *net,
                                  size_t max_count,
                                  const SceneSeq &scenes,
                                  const MinMax *min_max)
{
  const Network *network = sta_->network();
  CapacitanceCheckHeap heap(max_count, CapacitanceCheckSlackLess(sta_));
  
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      check(pin, scenes, min_max, heap);
    }
    delete pin_iter;
  }
  else {
    LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
    while (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      checkCapLimits(inst, scenes, min_max, heap);
    }
    delete inst_iter;
    // Check top level ports.
    checkCapLimits(network->topInstance(), scenes, min_max, heap);
  }

  checks_ = heap.extract();
  return checks_;
}

void
CheckCapacitances::checkCapLimits(const Instance *inst,
                                 bool violators,
                                 const SceneSeq &scenes,
                                 const MinMax *min_max)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    CapacitanceCheck cap_check = check(pin, violators, scenes, min_max);
    if (!cap_check.isNull())
      checks_.push_back(cap_check);
  }
  delete pin_iter;
}

void
CheckCapacitances::checkCapLimits(const Instance *inst,
                                  const SceneSeq &scenes,
                                  const MinMax *min_max,
                                  CapacitanceCheckHeap &heap)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    check(pin, scenes, min_max, heap);
  }
  delete pin_iter;
}

void
CheckCapacitances::check(const Pin *pin,
                         const SceneSeq &scenes,
                         const MinMax *min_max,
                         CapacitanceCheckHeap &heap)
{
  CapacitanceCheck cap_check = check(pin, false, scenes, min_max);
  if (!cap_check.isNull())
    heap.insert(cap_check);
}

bool
CheckCapacitances::checkPin(const Pin *pin,
                           const Scene *scene) const
{
  const Network *network = sta_->network();
  const Mode *mode = scene->mode();
  return network->isDriver(pin)
    && !mode->sim()->isConstant(pin)
    && !mode->sdc()->isDisabledConstraint(pin)
    && !mode->clkNetwork()->isIdealClock(pin);
}

////////////////////////////////////////////////////////////////

CapacitanceCheck::CapacitanceCheck() :
  pin_(nullptr),
  capacitance_(0.0),
  limit_(INF),
  slack_(-INF),
  scene_(nullptr),
  rf_(nullptr)
{
}

CapacitanceCheck::CapacitanceCheck(const Pin *pin,
                                   float capacitance,
                                   float limit,
                                   float slack,
                                   const Scene *scene,
                                   const RiseFall *rf) :
  pin_(pin),
  capacitance_(capacitance),
  limit_(limit),
  slack_(slack),
  scene_(scene),
  rf_(rf)
{
}

} // namespace
