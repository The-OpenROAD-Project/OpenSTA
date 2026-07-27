// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

// OpenROAD fork: analysis_corner support.

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "SdcClass.hh"
#include "StringUtil.hh"

namespace sta {

class AnalysisCorner;
class Edge;
class Mode;
class Scene;
class Sta;

// Same aliases as Sdc.hh (alias redeclaration to the same type is legal);
// avoids including Sdc.hh here.
using InputDelaySet = std::set<InputDelay*>;
using OutputDelaySet = std::set<OutputDelay*>;

using AnalysisCornerSeq = std::vector<AnalysisCorner*>;
using AnalysisCornerNameMap = std::map<std::string, AnalysisCorner*, std::less<>>;

// Register the "analysis_corner" property on scene objects.
// Called once from Sta::makeComponents.
void
defineAnalysisCornerProperties(Sta *sta);

// Corner-overlay IO delay lookups (defined in search/AnalysisCorner.cc).
// Override semantics are wholesale per pin: when a scene's corner overlay
// defines any input (output) delays on a pin, they replace the mode's
// delays on that pin for that scene; otherwise the mode's delays apply.

// The overlay's delay set for the pin, or nullptr when the scene has no
// corner, no overlay, or no overlay delays on the pin.
InputDelaySet *cornerInputDelaysLeafPin(const Scene *scene,
                                        const Pin *pin);
OutputDelaySet *cornerOutputDelaysLeafPin(const Scene *scene,
                                          const Pin *pin);
// True when input_delay applies to this scene under the override rule.
// A nullptr input_delay stands for the default (unconstrained) arrival
// seed, which applies only to scenes with no effective delays on the pin.
bool sceneSeesInputDelay(const Scene *scene,
                         const Pin *pin,
                         const InputDelay *input_delay);
// Distinct corner-overlay delay sets on the pin across the mode's scenes.
std::vector<InputDelaySet*> cornerInputDelaySets(const Mode *mode,
                                                 const Pin *pin);
std::vector<OutputDelaySet*> cornerOutputDelaySets(const Mode *mode,
                                                   const Pin *pin);
bool modeHasCornerInputDelay(const Mode *mode,
                             const Pin *pin);
bool modeHasCornerOutputDelay(const Mode *mode,
                              const Pin *pin);

// Corner-overlay clock uncertainty lookups (defined in
// search/AnalysisCorner.cc). Selection order at the read sites mirrors
// upstream specificity (pin beats clock) with the corner beating the mode
// at equal specificity.

// Overlay's set_clock_uncertainty on the pin, or nullptr.
const ClockUncertainties *cornerPinClockUncertainties(const Scene *scene,
                                                      const Pin *pin);
// Corner's set_clock_uncertainty on the clock, or nullptr.
const ClockUncertainties *cornerClkUncertainties(const Scene *scene,
                                                 const Clock *clk);
// Corner-overlay clock latency/insertion lookups (defined in
// search/AnalysisCorner.cc). Per-lookup fallback: exists=false when the
// scene has no corner, no overlay, or the overlay defines no value; the
// read sites then keep the mode's value. Upstream pin-beats-clock
// precedence is replicated at the sites.
void cornerClockLatencyPin(const Scene *scene,
                           const Clock *clk,
                           const Pin *pin,
                           const RiseFall *rf,
                           const MinMax *min_max,
                           // Return values.
                           float &latency,
                           bool &exists);
void cornerClockLatencyClk(const Scene *scene,
                           const Clock *clk,
                           const RiseFall *rf,
                           const MinMax *min_max,
                           // Return values.
                           float &latency,
                           bool &exists);
void cornerClockLatencyEdge(const Scene *scene,
                            Edge *edge,
                            const RiseFall *rf,
                            const MinMax *min_max,
                            // Return values.
                            float &latency,
                            bool &exists);
// pin may be nullptr for clock-level insertion.
void cornerClockInsertion(const Scene *scene,
                          const Clock *clk,
                          const Pin *pin,
                          const RiseFall *rf,
                          const MinMax *min_max,
                          const EarlyLate *early_late,
                          // Return values.
                          float &insertion,
                          bool &exists);

// Overlay's inter-clock uncertainty; exists=false when the overlay does
// not define a value for this transition/min_max combination.
void cornerInterClockUncertainty(const Scene *scene,
                                 const Clock *src_clk,
                                 const RiseFall *src_rf,
                                 const Clock *tgt_clk,
                                 const RiseFall *tgt_rf,
                                 const MinMax *min_max,
                                 // Return values.
                                 float &uncertainty,
                                 bool &exists);

// PVT/RC analysis corner. Groups the scenes that share an operating
// point across modes and bundles the liberty/spef authoring data
// define_scene composes from. Liberty maps, parasitics bindings and
// SDC remain on Scene/Mode.
class AnalysisCorner
{
public:
  AnalysisCorner(std::string_view name,
                 size_t index);
  const std::string &name() const { return name_; }
  size_t index() const { return index_; }
  const StringSeq &libertyMinFiles() const { return liberty_min_files_; }
  const StringSeq &libertyMaxFiles() const { return liberty_max_files_; }
  const std::string &spefMinName() const { return spef_min_name_; }
  const std::string &spefMaxName() const { return spef_max_name_; }
  const StringSeq &sdcFiles() const { return sdc_files_; }
  void setLiberty(const StringSeq &min_files,
                  const StringSeq &max_files)
  {
    liberty_min_files_ = min_files;
    liberty_max_files_ = max_files;
  }
  void setSpef(const std::string &min_name,
               const std::string &max_name)
  {
    spef_min_name_ = min_name;
    spef_max_name_ = max_name;
  }
  void setSdc(const StringSeq &sdc_files) { sdc_files_ = sdc_files; }

  // Corner-scoped set_clock_uncertainty on a clock. Stored here keyed by
  // the mode's Clock object because upstream stores clock uncertainty on
  // the (corner-shared) Clock itself, not in the Sdc. Wholesale override:
  // a corner entry replaces the clock's own uncertainties entirely.
  const ClockUncertainties *clockUncertainty(const Clock *clk) const;
  void setClockUncertainty(const Clock *clk,
                           const MinMaxAll *setup_hold,
                           float uncertainty);
  void removeClockUncertainty(const Clock *clk,
                              const MinMaxAll *setup_hold);
  // Lifecycle: the map keys are the mode's Clock objects, which die on
  // remove_clock / Sta::clear.
  void removeClockUncertainties(const Clock *clk) { clk_uncertainties_.erase(clk); }
  void clearClockUncertainties() { clk_uncertainties_.clear(); }

private:
  std::string name_;
  size_t index_;
  // Liberty names/filenames and read_spef -name keys, composed into
  // define_scene when the scene names this corner.
  StringSeq liberty_min_files_;
  StringSeq liberty_max_files_;
  std::string spef_min_name_;
  std::string spef_max_name_;
  // Corner-scoped SDC files, applied to the (mode, corner) overlay Sdc
  // when the first scene naming this corner is defined
  // (see tcl/AnalysisCorner.tcl).
  StringSeq sdc_files_;
  // Corner-scoped clock uncertainties keyed by the mode's Clock objects.
  std::map<const Clock*, ClockUncertainties> clk_uncertainties_;
};

} // namespace sta
