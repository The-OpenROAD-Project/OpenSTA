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

#include <mutex>
#include <array>
#include <map>

#include "MinMax.hh"
#include "Parasitics.hh"

namespace sta {

class ConcreteParasitic;
class ConcreteParasiticNetwork;

constexpr size_t min_max_rise_fall_count = MinMax::index_count * RiseFall::index_count;
// Min/maxrise/fall reduced parasitics for each driver pin.
// When min parastitic network != max parasitic network only
// the min values are populated for the min parasitics and
// max values for max parasitics.
using MinMaxRiseFallParasitics = std::array<ConcreteParasitic*, min_max_rise_fall_count>;
using ConcreteParasiticMap = std::map<const Pin*, MinMaxRiseFallParasitics>;
using ConcreteParasiticNetworkMap = std::map<const Net*, ConcreteParasiticNetwork>;

// This class acts as a BUILDER for parasitics.
class ConcreteParasitics : public Parasitics
{
public:
  ConcreteParasitics(std::string name,
                     std::string filename,
                     StaState *sta);
  virtual ~ConcreteParasitics();
  const std::string &name() const override { return name_; };
  const std::string &filename() const override { return filename_; };
  bool haveParasitics() override;
  void clear() override;

  void deleteParasitics() override;
  void deleteParasitics(const Net *net) override;
  void deleteParasitics(const Pin *drvr_pin) override;

  bool isReducedParasiticNetwork(const Parasitic *parasitic) const override;
  void setIsReducedParasiticNetwork(Parasitic *parasitic,
                                    bool is_reduced) override;

  float capacitance(const Parasitic *parasitic) const override;

  bool isPiElmore(const Parasitic *parasitic) const override;
  Parasitic *findPiElmore(const Pin *drvr_pin,
                          const RiseFall *rf,
                          const MinMax *min_max) const override;
  Parasitic *makePiElmore(const Pin *drvr_pin,
                          const RiseFall *rf,
                          const MinMax *min_max,
                          float c2,
                          float rpi,
                          float c1) override;

  bool isPiModel(const Parasitic *parasitic) const override;
  void piModel(const Parasitic *parasitic,
               float &c2,
               float &rpi,
               float &c1) const override;
  void setPiModel(Parasitic *parasitic,
                  float c2,
                  float rpi,
                  float c1) override;

  void findElmore(const Parasitic *parasitic,
                  const Pin *load_pin,
                  float &elmore,
                  bool &exists) const override;
  void setElmore(Parasitic *parasitic,
                 const Pin *load_pin,
                 float elmore) override;

  bool isPiPoleResidue(const Parasitic* parasitic) const override;
  Parasitic *findPiPoleResidue(const Pin *drvr_pin,
                               const RiseFall *rf,
                               const MinMax *min_max) const override;
  Parasitic *findPoleResidue(const Parasitic *parasitic,
                             const Pin *load_pin) const override;
  Parasitic *makePiPoleResidue(const Pin *drvr_pin,
                               const RiseFall *rf,
                               const MinMax *min_max,
                               float c2,
                               float rpi,
                               float c1) override;
  void setPoleResidue(Parasitic *parasitic, const Pin *load_pin,
                      ComplexFloatSeq *poles,
                      ComplexFloatSeq *residues) override;
  bool isPoleResidue(const Parasitic* parasitic) const override;
  size_t poleResidueCount(const Parasitic *parasitic) const override;
  void poleResidue(const Parasitic *parasitic,
                   int pole_index,
                   ComplexFloat &pole,
                   ComplexFloat &residue) const override;

  bool isParasiticNetwork(const Parasitic *parasitic) const override;
  Parasitic *findParasiticNetwork(const Net *net) override;
  Parasitic *findParasiticNetwork(const Pin *pin)  override;
  Parasitic *makeParasiticNetwork(const Net *net,
                                  bool includes_pin_caps) override;
  void deleteParasiticNetwork(const Net *net) override;
  const Net *net(const Parasitic *parasitic) const override;
  bool includesPinCaps(const Parasitic *parasitic) const override;
  ParasiticNode *findParasiticNode(Parasitic *parasitic,
                                   const Net *net,
                                   int id,
                                   const Network *network) const override;
  ParasiticNode *ensureParasiticNode(Parasitic *parasitic,
                                     const Net *net,
                                     int id,
                                     const Network *network) override;
  ParasiticNode *findParasiticNode(const Parasitic *parasitic,
                                   const Pin *pin) const override;
  ParasiticNode *ensureParasiticNode(Parasitic *parasitic,
                                     const Pin *pin,
                                     const Network *network) override;
  ParasiticNodeSeq nodes(const Parasitic *parasitic) const override;
  void incrCap(ParasiticNode *node,
               float cap) override;
  const char *name(const ParasiticNode *node) const override;
  const Pin *pin(const ParasiticNode *node) const override;
  const Net *net(const ParasiticNode *node,
                 const Network *network) const override;
  unsigned netId(const ParasiticNode *node) const override;
  bool isExternal(const ParasiticNode *node) const override;
  float nodeGndCap(const ParasiticNode *node) const override;

  ParasiticResistorSeq resistors(const Parasitic *parasitic) const override;
  void makeResistor(Parasitic *parasitic,
                    size_t id,
                    float res,
                    ParasiticNode *node1,
                    ParasiticNode *node2) override;
  size_t id(const ParasiticResistor *resistor) const override;
  float value(const ParasiticResistor *resistor) const override;
  ParasiticNode *node1(const ParasiticResistor *resistor) const override;
  ParasiticNode *node2(const ParasiticResistor *resistor) const override;
  ParasiticCapacitorSeq capacitors(const Parasitic *parasitic) const override;
  void makeCapacitor(Parasitic *parasitic,
                     size_t id,
                     float cap,
                     ParasiticNode *node1,
                     ParasiticNode *node2) override;
  size_t id(const ParasiticCapacitor *capacitor) const override;
  float value(const ParasiticCapacitor *capacitor) const override;
  ParasiticNode *node1(const ParasiticCapacitor *capacitor) const override;
  ParasiticNode *node2(const ParasiticCapacitor *capacitor) const override;

  PinSet unannotatedLoads(const Parasitic *parasitic,
                          const Pin *drvr_pin) const override;

  void disconnectPinBefore(const Pin *pin) override;
  void deletePinBefore(const Pin *pin) override;
  void loadPinCapacitanceChanged(const Pin *pin) override;

  void deleteReducedParasitics(const Net *net) override;
  void deleteDrvrReducedParasitics(const Pin *drvr_pin) override;

protected:
  Parasitic *ensureRspf(const Pin *drvr_pin);
  void makeAnalysisPtAfter();
  void deleteReducedParasitics(const Pin *pin);

  std::string name_;
  std::string filename_;

  // Driver pin to array of parasitics indexed by analysis pt index
  // and transition.
  ConcreteParasiticMap drvr_parasitic_map_;
  ConcreteParasiticNetworkMap parasitic_network_map_;
  mutable std::mutex lock_;

  friend class ConcretePiElmore;
  friend class ConcreteParasiticNode;
  friend class ConcreteParasiticNetwork;
};

} // namespace
