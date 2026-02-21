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

#include "CheckSlews.hh"

#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Mode.hh"
#include "InputDrive.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "StaState.hh"
#include "Scene.hh"
#include "Path.hh"
#include "PortDirection.hh"
#include "Sim.hh"
#include "Search.hh"
#include "ClkNetwork.hh"

namespace sta {

CheckSlews::CheckSlews(const StaState *sta) :
  heap_(0, SlewCheckSlackLess(sta)),
  sta_(sta)
{
}

void
CheckSlews::clear()
{
  checks_.clear();
  heap_.clear();
}

SlewCheckSeq &
CheckSlews::check(const Net *net,
                  size_t max_count,
                  bool violators,
                  const SceneSeq &scenes,
                  const MinMax *min_max)
{
  clear();
  if (!violators)
    heap_.setMaxSize(max_count);

  if (net)
    checkNet(net, violators, scenes, min_max);
  else
    checkAll(violators, scenes, min_max);

  if (violators)
    sort(checks_, SlewCheckSlackLess(sta_));
  else
    checks_ = heap_.extract();
  return checks_;
}

void
CheckSlews::checkNet(const Net *net,
                     bool violators,
                     const SceneSeq &scenes,
                     const MinMax *min_max)
{
  const Network *network = sta_->network();
  NetPinIterator *pin_iter = network->pinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    checkPin(pin, violators, scenes, min_max);
  }
  delete pin_iter;
}

void
CheckSlews::checkAll(bool violators,
                     const SceneSeq &scenes,
                     const MinMax *min_max)
{
  const Network *network = sta_->network();
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    const Instance *inst = inst_iter->next();
    checkInst(inst, violators, scenes, min_max);
  }
  delete inst_iter;
  // Check top level ports.
  checkInst(network->topInstance(), violators, scenes, min_max);
}

void
CheckSlews::checkInst(const Instance *inst,
                      bool violators,
                      const SceneSeq &scenes,
                      const MinMax *min_max)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    checkPin(pin, violators, scenes, min_max);
  }
  delete pin_iter;
}

void
CheckSlews::checkPin(const Pin *pin,
                     bool violators,
                     const SceneSeq &scenes,
                     const MinMax *min_max)
{
  const Scene *scene;
  const RiseFall *rf;
  Slew slew;
  float limit, slack;
  check(pin, scenes, min_max, true, slew, limit, slack, rf, scene);
  if (scene) {
    if (violators) {
      if (slack < 0.0)
        checks_.emplace_back(pin, rf, slew, limit, slack, scene);
    }
    else
      heap_.insert(SlewCheck(pin, rf, slew, limit, slack, scene));
  }
}

void
CheckSlews::check(const Pin *pin,
                  const SceneSeq &scenes,
                  const MinMax *min_max,
                  bool check_clks,
                  // Return values.
                  Slew &slew,
                  float &limit,
                  float &slack,
                  const RiseFall *&rf,
                  const Scene *&scene) const
{
  scene = nullptr;
  rf = nullptr;
  slew = 0.0;
  limit = 0.0;
  slack = MinMax::min()->initValue();

  for (const Scene *scene1 : scenes) {
    Vertex *vertex, *bidirect_drvr_vertex;
    sta_->graph()->pinVertices(pin, vertex, bidirect_drvr_vertex);
    if (vertex)
      check2(vertex, scene1, min_max, check_clks,
             scene, rf, slew, limit, slack);
    if (bidirect_drvr_vertex)
      check2(bidirect_drvr_vertex, scene1, min_max, check_clks,
             scene, rf, slew, limit, slack);
  }
}

void
CheckSlews::check2(const Vertex *vertex,
                   const Scene *scene,
                   const MinMax *min_max,
                   bool check_clks,
                   // Return values.
                   const Scene *&scene1,
                   const RiseFall *&rf1,
                   Slew &slew1,
                   float &limit1,
                   float &slack1) const
{
  const Mode *mode = scene->mode();
  const Sdc *sdc = mode->sdc();
  const ClkNetwork *clk_network = mode->clkNetwork();
  const Pin *pin = vertex->pin();
  if (!sdc->isDisabledConstraint(pin)
      && !clk_network->isIdealClock(pin)) {
    ConstClockSet clks;
    if (check_clks)
      clks = clockDomains(vertex, scene);
    for (const RiseFall *rf : RiseFall::range()) {
      float limit;
      bool exists;
      findLimit(pin, scene, rf, min_max, clks,
                limit, exists);
      if (exists) {
        check3(vertex, scene, rf, min_max, limit,
               scene1, rf1, slew1, slack1, limit1);
      }
    }
  }
}

void
CheckSlews::check3(const Vertex *vertex,
                   const Scene *scene,
                   const RiseFall *rf,
                   const MinMax *min_max,
                   float limit,
                   // Return values.
                   const Scene *&scene1,
                   const RiseFall *&rf1,
                   Slew &slew1,
                   float &slack1,
                   float &limit1) const
{
  DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
  Slew slew = sta_->graph()->slew(vertex, rf, ap_index);
  float slew2 = delayAsFloat(slew);
  float slack = (min_max == MinMax::max())
    ? limit - slew2 : slew2 - limit;
  if (scene1 == nullptr
      || (slack < slack1
          // Break ties for the sake of regression stability.
          || (fuzzyEqual(slack, slack1)
              && rf->index() < rf1->index()))) {
    scene1 = scene;
    rf1 = rf;
    slew1 = slew;
    slack1 = slack;
    limit1 = limit;
  }
}

// Return the tightest limit.
void
CheckSlews::findLimit(const Pin *pin,
                      const Scene *scene,
                      const RiseFall *rf,
                      const MinMax *min_max,
                      const ConstClockSet &clks,
                      // Return values.
                      float &limit,
                      bool &exists) const
{
  const Network *network = sta_->network();
  const Sdc *sdc = scene->sdc();
  LibertyPort *port = network->libertyPort(pin);
  findLimit(port, scene, min_max,
            limit, exists);

  float limit1;
  bool exists1;
  if (!clks.empty()) {
    // Look for clock slew limits.
    const ClkNetwork *clk_network = scene->mode()->clkNetwork();
    bool is_clk = clk_network->isIdealClock(pin);
    for (const Clock *clk : clks) {
      PathClkOrData clk_data = is_clk ? PathClkOrData::clk : PathClkOrData::data;
      sdc->slewLimit(clk, rf, clk_data, min_max,
                     limit1, exists1);
      if (exists1
          && (!exists
              || min_max->compare(limit, limit1))) {
        limit = limit1;
        exists = true;
      }
    }
  }

  if (network->isTopLevelPort(pin)) {
    Port *port = network->port(pin);
    sdc->slewLimit(port, min_max, limit1, exists1);
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
          scene_port->slewLimit(min_max, limit1, exists1);
          if (!exists1
              && scene_port->direction()->isAnyOutput()
              && min_max == MinMax::max())
            scene_port->libertyLibrary()->defaultMaxSlew(limit1, exists1);
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
}

void
CheckSlews::findLimit(const LibertyPort *port,
                      const Scene *scene,
                      const MinMax *min_max,
                      // Return values.
                      float &limit,
                      bool &exists) const
{
  limit = INF;
  exists = false;

  const Network *network = sta_->network();
  const Sdc *sdc = scene->sdc();
  float limit1;
  bool exists1;

  // Default to top ("design") limit.
  Cell *top_cell = network->cell(network->topInstance());
  sdc->slewLimit(top_cell, min_max,
                 limit1, exists1);
  if (exists1) {
    limit = limit1;
    exists = true;
  }

  if (port) {
    const LibertyPort *scene_port = port->scenePort(scene, min_max);
    scene_port->slewLimit(min_max, limit1, exists1);
    if (!exists1
        // default_max_transition only applies to outputs.
        && scene_port->direction()->isAnyOutput()
        && min_max == MinMax::max())
      scene_port->libertyLibrary()->defaultMaxSlew(limit1, exists1);
    if (exists1
        && (!exists
            || min_max->compare(limit, limit1))) {
      limit = limit1;
      exists = true;
    }
  }
}

ConstClockSet
CheckSlews::clockDomains(const Vertex *vertex,
                         const Scene *scene) const
{
  ConstClockSet clks;
  VertexPathIterator path_iter(const_cast<Vertex*>(vertex), sta_);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    if (path->scene(sta_) == scene) {
      const Clock *clk = path->clock(sta_);
      if (clk)
        clks.insert(clk);
    }
  }
  return clks;
}

////////////////////////////////////////////////////////////////

SlewCheck::SlewCheck() :
  pin_(nullptr),
  rf_(nullptr),
  slew_(0.0),
  limit_(0.0),
  slack_(0.0),
  scene_(nullptr)
{
}

SlewCheck::SlewCheck(const Pin *pin,
                     const RiseFall *rf,
                     Slew &slew,
                     float limit,
                     float slack,
                     const Scene *scene) :
  pin_(pin),
  rf_(rf),
  slew_(slew),
  limit_(limit),
  slack_(slack),
  scene_(scene)
{
}

////////////////////////////////////////////////////////////////

SlewCheckSlackLess::SlewCheckSlackLess(const StaState *sta) :
  sta_(sta)
{
}

bool
SlewCheckSlackLess::operator()(const SlewCheck &check1,
                               const SlewCheck &check2) const
{
  float slack1 = check1.slack();
  float slack2 = check2.slack();
  return fuzzyLess(slack1, slack2)
    || (fuzzyEqual(slack1, slack2)
        // Break ties for the sake of regression stability.
        && sta_->network()->pinLess(check1.pin(), check2.pin()));
}

} // namespace
