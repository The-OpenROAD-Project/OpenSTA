// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2019-2025, The OpenROAD Authors

// OpenROAD fork: analysis_corner support.

#include "AnalysisCorner.hh"

#include <algorithm>

#include "ContainerHelpers.hh"
#include "Mode.hh"
#include "PatternMatch.hh"
#include "Property.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "Search.hh"
#include "Sta.hh"

namespace sta {

AnalysisCorner::AnalysisCorner(std::string_view name,
                               size_t index) :
  name_(name),
  index_(index)
{
}

////////////////////////////////////////////////////////////////

// Register the "analysis_corner" property on scene objects so
// get_property and get_scenes -filter resolve it natively.
void
defineAnalysisCornerProperties(Sta *sta)
{
  sta->properties().defineProperty("analysis_corner",
    [] (const Scene *scene,
        Sta *) -> PropertyValue {
      AnalysisCorner *corner = scene->analysisCorner();
      if (corner)
        return PropertyValue(corner->name());
      return PropertyValue();
    });
}

AnalysisCorner *
Sta::makeAnalysisCorner(std::string_view name)
{
  AnalysisCorner *corner = findAnalysisCorner(name);
  if (corner == nullptr) {
    corner = new AnalysisCorner(name, analysis_corners_.size());
    analysis_corner_name_map_[std::string(name)] = corner;
    analysis_corners_.push_back(corner);
  }
  return corner;
}

AnalysisCorner *
Sta::findAnalysisCorner(std::string_view name) const
{
  return findStringKey(analysis_corner_name_map_, name);
}

AnalysisCornerSeq
Sta::findAnalysisCorners(const std::string &pattern) const
{
  AnalysisCornerSeq matches;
  PatternMatch pattern_match(pattern);
  for (AnalysisCorner *corner : analysis_corners_) {
    if (pattern_match.match(corner->name()))
      matches.push_back(corner);
  }
  return matches;
}

void
Sta::setSceneAnalysisCorner(Scene *scene,
                            AnalysisCorner *corner)
{
  scene->setAnalysisCorner(corner);
  // The association changes which overlay Sdc the scene reads (derates,
  // IO delays, uncertainty, latency), so cached results are stale.
  search_->arrivalsInvalid();
}

Sdc *
Sta::cmdCornerSdc() const
{
  if (cmd_analysis_corner_)
    return cmd_mode_->makeCornerSdc(cmd_analysis_corner_);
  return cmd_mode_->sdc();
}

void
Sta::deleteAnalysisCorners()
{
  deleteContents(analysis_corners_);
  analysis_corners_.clear();
  analysis_corner_name_map_.clear();
  cmd_analysis_corner_ = nullptr;
}

////////////////////////////////////////////////////////////////

// Mode overlay Sdc definitions (declared in Mode.hh).

Sdc *
Mode::cornerSdc(const AnalysisCorner *corner) const
{
  return findKey(corner_sdcs_, corner);
}

Sdc *
Mode::makeCornerSdc(const AnalysisCorner *corner)
{
  Sdc *&sdc = corner_sdcs_[corner];
  if (sdc == nullptr)
    sdc = new Sdc(this, sta_);
  return sdc;
}

void
Mode::clearCornerSdcs()
{
  for (const auto [corner, sdc] : corner_sdcs_)
    sdc->clear();
}

////////////////////////////////////////////////////////////////

// Corner-overlay lookups (declared in AnalysisCorner.hh / Scene.hh).

static Sdc *
sceneCornerSdc(const Scene *scene)
{
  AnalysisCorner *corner = scene->analysisCorner();
  if (corner)
    return scene->mode()->cornerSdc(corner);
  return nullptr;
}

// Corner overlay Sdc for derate queries (declared in Scene.hh).
// The overlay overrides the mode Sdc wholesale iff it defines derates.
const Sdc *
Scene::sdcOverlayForDerate() const
{
  const Sdc *overlay = sceneCornerSdc(this);
  if (overlay && overlay->hasDeratingFactors())
    return overlay;
  return nullptr;
}

template <typename DelaySet>
static DelaySet *
cornerDelaysLeafPin(const Scene *scene,
                    const Pin *pin,
                    DelaySet *(Sdc::*delays_leaf_pin)(const Pin*) const)
{
  const Sdc *overlay = sceneCornerSdc(scene);
  if (overlay) {
    DelaySet *delays = (overlay->*delays_leaf_pin)(pin);
    if (delays && !delays->empty())
      return delays;
  }
  return nullptr;
}

InputDelaySet *
cornerInputDelaysLeafPin(const Scene *scene,
                         const Pin *pin)
{
  return cornerDelaysLeafPin(scene, pin, &Sdc::inputDelaysLeafPin);
}

OutputDelaySet *
cornerOutputDelaysLeafPin(const Scene *scene,
                          const Pin *pin)
{
  return cornerDelaysLeafPin(scene, pin, &Sdc::outputDelaysLeafPin);
}

bool
sceneSeesInputDelay(const Scene *scene,
                    const Pin *pin,
                    const InputDelay *input_delay)
{
  const InputDelaySet *overlay_delays = cornerInputDelaysLeafPin(scene, pin);
  const InputDelaySet *mode_delays =
    scene->mode()->sdc()->inputDelaysLeafPin(pin);
  if (input_delay == nullptr)
    // Default (unconstrained) arrival seed: only for scenes with no
    // effective input delays on the pin.
    return overlay_delays == nullptr
      && (mode_delays == nullptr || mode_delays->empty());
  const InputDelaySet *effective =
    overlay_delays ? overlay_delays : mode_delays;
  return effective
    && effective->contains(const_cast<InputDelay*>(input_delay));
}

template <typename DelaySet>
static std::vector<DelaySet*>
cornerDelaySets(const Mode *mode,
                const Pin *pin,
                DelaySet *(Sdc::*delays_leaf_pin)(const Pin*) const)
{
  std::vector<DelaySet*> delay_sets;
  for (const Scene *scene : mode->scenes()) {
    DelaySet *delays = cornerDelaysLeafPin(scene, pin, delays_leaf_pin);
    if (delays
        && std::find(delay_sets.begin(), delay_sets.end(), delays)
           == delay_sets.end())
      delay_sets.push_back(delays);
  }
  return delay_sets;
}

std::vector<InputDelaySet*>
cornerInputDelaySets(const Mode *mode,
                     const Pin *pin)
{
  return cornerDelaySets(mode, pin, &Sdc::inputDelaysLeafPin);
}

std::vector<OutputDelaySet*>
cornerOutputDelaySets(const Mode *mode,
                      const Pin *pin)
{
  return cornerDelaySets(mode, pin, &Sdc::outputDelaysLeafPin);
}

template <typename DelaySet>
static bool
modeHasCornerDelay(const Mode *mode,
                   const Pin *pin,
                   DelaySet *(Sdc::*delays_leaf_pin)(const Pin*) const)
{
  if (mode->cornerSdcs().empty())
    return false;
  for (const Scene *scene : mode->scenes()) {
    if (cornerDelaysLeafPin(scene, pin, delays_leaf_pin))
      return true;
  }
  return false;
}

bool
modeHasCornerInputDelay(const Mode *mode,
                        const Pin *pin)
{
  return modeHasCornerDelay(mode, pin, &Sdc::inputDelaysLeafPin);
}

bool
modeHasCornerOutputDelay(const Mode *mode,
                         const Pin *pin)
{
  return modeHasCornerDelay(mode, pin, &Sdc::outputDelaysLeafPin);
}

////////////////////////////////////////////////////////////////

// Corner-scoped clock uncertainties (declared in AnalysisCorner.hh).

const ClockUncertainties *
AnalysisCorner::clockUncertainty(const Clock *clk) const
{
  auto iter = clk_uncertainties_.find(clk);
  if (iter != clk_uncertainties_.end())
    return &iter->second;
  return nullptr;
}

void
AnalysisCorner::setClockUncertainty(const Clock *clk,
                                    const MinMaxAll *setup_hold,
                                    float uncertainty)
{
  clk_uncertainties_[clk].setValue(setup_hold, uncertainty);
}

void
AnalysisCorner::removeClockUncertainty(const Clock *clk,
                                       const MinMaxAll *setup_hold)
{
  auto iter = clk_uncertainties_.find(clk);
  if (iter != clk_uncertainties_.end()) {
    iter->second.removeValue(setup_hold);
    if (iter->second.empty())
      clk_uncertainties_.erase(iter);
  }
}

void
Sta::setAnalysisCornerClockUncertainty(AnalysisCorner *corner,
                                       const Clock *clk,
                                       const MinMaxAll *setup_hold,
                                       float uncertainty)
{
  corner->setClockUncertainty(clk, setup_hold, uncertainty);
  search_->arrivalsInvalid();
}

void
Sta::removeAnalysisCornerClockUncertainty(AnalysisCorner *corner,
                                          const Clock *clk,
                                          const MinMaxAll *setup_hold)
{
  corner->removeClockUncertainty(clk, setup_hold);
  search_->arrivalsInvalid();
}

// Corner clock uncertainties are keyed by Clock objects; purge them when
// clocks die (remove_clock, Sta::clear) so recycled Clock addresses cannot
// inherit stale corner data.
void
Sta::purgeAnalysisCornerClockUncertainties(const Clock *clk)
{
  for (AnalysisCorner *corner : analysis_corners_) {
    if (clk)
      corner->removeClockUncertainties(clk);
    else
      corner->clearClockUncertainties();
  }
}

const ClockUncertainties *
cornerPinClockUncertainties(const Scene *scene,
                            const Pin *pin)
{
  const Sdc *overlay = sceneCornerSdc(scene);
  if (overlay)
    return overlay->clockUncertainties(pin);
  return nullptr;
}

const ClockUncertainties *
cornerClkUncertainties(const Scene *scene,
                       const Clock *clk)
{
  AnalysisCorner *corner = scene->analysisCorner();
  if (corner)
    return corner->clockUncertainty(clk);
  return nullptr;
}

void
cornerClockLatencyPin(const Scene *scene,
                      const Clock *clk,
                      const Pin *pin,
                      const RiseFall *rf,
                      const MinMax *min_max,
                      // Return values.
                      float &latency,
                      bool &exists)
{
  exists = false;
  const Sdc *overlay = sceneCornerSdc(scene);
  if (overlay)
    overlay->clockLatency(clk, pin, rf, min_max, latency, exists);
}

void
cornerClockLatencyClk(const Scene *scene,
                      const Clock *clk,
                      const RiseFall *rf,
                      const MinMax *min_max,
                      // Return values.
                      float &latency,
                      bool &exists)
{
  exists = false;
  const Sdc *overlay = sceneCornerSdc(scene);
  if (overlay)
    overlay->clockLatency(clk, rf, min_max, latency, exists);
}

void
cornerClockLatencyEdge(const Scene *scene,
                       Edge *edge,
                       const RiseFall *rf,
                       const MinMax *min_max,
                       // Return values.
                       float &latency,
                       bool &exists)
{
  exists = false;
  const Sdc *overlay = sceneCornerSdc(scene);
  if (overlay)
    overlay->clockLatency(edge, rf, min_max, latency, exists);
}

void
cornerClockInsertion(const Scene *scene,
                     const Clock *clk,
                     const Pin *pin,
                     const RiseFall *rf,
                     const MinMax *min_max,
                     const EarlyLate *early_late,
                     // Return values.
                     float &insertion,
                     bool &exists)
{
  exists = false;
  const Sdc *overlay = sceneCornerSdc(scene);
  if (overlay)
    overlay->clockInsertion(clk, pin, rf, min_max, early_late,
                            insertion, exists);
}

void
cornerInterClockUncertainty(const Scene *scene,
                            const Clock *src_clk,
                            const RiseFall *src_rf,
                            const Clock *tgt_clk,
                            const RiseFall *tgt_rf,
                            const MinMax *min_max,
                            // Return values.
                            float &uncertainty,
                            bool &exists)
{
  exists = false;
  const Sdc *overlay = sceneCornerSdc(scene);
  if (overlay)
    overlay->clockUncertainty(src_clk, src_rf, tgt_clk, tgt_rf, min_max,
                              uncertainty, exists);
}

} // namespace sta
