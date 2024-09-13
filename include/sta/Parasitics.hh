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

#include <complex>
#include <map>
#include <vector>

#include "StaState.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "ParasiticsClass.hh"

namespace sta {

class Wireload;
class Corner;

typedef std::complex<float> ComplexFloat;
typedef Vector<ComplexFloat> ComplexFloatSeq;
typedef std::vector<ParasiticNode*> ParasiticNodeSeq;
typedef std::vector<ParasiticResistor*> ParasiticResistorSeq;
typedef std::vector<ParasiticCapacitor*> ParasiticCapacitorSeq;
typedef std::map<ParasiticNode *, ParasiticResistorSeq> ParasiticNodeResistorMap;
typedef std::map<ParasiticNode *, ParasiticCapacitorSeq> ParasiticNodeCapacitorMap;

// Parasitics API.
// All parasitic parameters can have multiple values, each corresponding
// to an analysis point.
// Parasitic annotation for a pin or net may exist for one analysis point
// and not another.
class Parasitics : public StaState
{
public:
  Parasitics(StaState *sta);
  virtual ~Parasitics() {}
  virtual bool haveParasitics() = 0;
  // Clear all state.
  virtual void clear() = 0;

  // Delete all parasitics.
  virtual void deleteParasitics() = 0;
  // Delete all parasitics on net at analysis point.
  virtual void deleteParasitics(const Net *net,
				const ParasiticAnalysisPt *ap) = 0;
  // Delete all parasitics on pin at analysis point.
  virtual void deleteParasitics(const Pin *pin,
				const ParasiticAnalysisPt *ap) = 0;
  virtual void deleteReducedParasitics(const Net *net,
                                       const ParasiticAnalysisPt *ap) = 0;
  virtual void deleteDrvrReducedParasitics(const Pin *drvr_pin) = 0;

  virtual bool isReducedParasiticNetwork(const Parasitic *parasitic) const = 0;
  // Flag this parasitic as reduced from a parasitic network.
  virtual void setIsReducedParasiticNetwork(Parasitic *parasitic,
					    bool is_reduced) = 0;

  // Capacitance value of parasitic object.
  virtual float capacitance(const Parasitic *parasitic) const = 0;

  ////////////////////////////////////////////////////////////////
  // Pi model driver load with elmore delays to load pins (RSPF).
  // This follows the SPEF documentation of c2/c1, with c2 being the
  // capacitor on the driver pin.
  virtual bool isPiElmore(const Parasitic *parasitic) const = 0;
  virtual Parasitic *findPiElmore(const Pin *drvr_pin,
				  const RiseFall *rf,
				  const ParasiticAnalysisPt *ap) const = 0;
  virtual Parasitic *makePiElmore(const Pin *drvr_pin,
				  const RiseFall *rf,
				  const ParasiticAnalysisPt *ap,
				  float c2,
				  float rpi,
				  float c1) = 0;

  ////////////////////////////////////////////////////////////////
  // Pi models are common to PiElmore and PiPoleResidue.
  virtual bool isPiModel(const Parasitic *parasitic) const = 0;
  virtual void piModel(const Parasitic *parasitic,
		       float &c2,
		       float &rpi,
		       float &c1) const = 0;
  // Set PI model parameters.
  virtual void setPiModel(Parasitic *parasitic,
			  float c2,
			  float rpi,
			  float c1) = 0;

  ////////////////////////////////////////////////////////////////
  // Elmore driver to load delay.
  // Common to LumpedElmore and PiElmore parasitics.
  virtual void findElmore(const Parasitic *parasitic,
			  const Pin *load_pin,
			  float &elmore,
			  bool &exists) const = 0;
  // Set load elmore delay.
  virtual void setElmore(Parasitic *parasitic,
			 const Pin *load_pin,
			 float elmore) = 0;

  ////////////////////////////////////////////////////////////////
  // Pi model driver load with pole/residue interconnect model to load pins.
  virtual bool isPiPoleResidue(const Parasitic* parasitic) const = 0;
  virtual Parasitic *findPiPoleResidue(const Pin *drvr_pin,
				       const RiseFall *rf,
				       const ParasiticAnalysisPt *ap) const=0;
  virtual Parasitic *makePiPoleResidue(const Pin *drvr_pin,
				       const RiseFall *rf,
				       const ParasiticAnalysisPt *ap,
				       float c2,
				       float rpi,
				       float c1) = 0;
  virtual Parasitic *findPoleResidue(const Parasitic *parasitic,
				     const Pin *load_pin) const = 0;
  // Make pole/residue model for load_pin.
  virtual void setPoleResidue(Parasitic *parasitic,
			      const Pin *load_pin,
			      ComplexFloatSeq *poles,
			      ComplexFloatSeq *residues) = 0;
  virtual bool isPoleResidue(const Parasitic* parasitic) const = 0;
  // Return the number of poles and residues in a pole/residue parasitic.
  virtual size_t poleResidueCount(const Parasitic *parasitic) const = 0;
  // Find the pole_index'th pole/residue in a pole/residue parasitic.
  virtual void poleResidue(const Parasitic *parasitic,
			   int pole_index,
			   ComplexFloat &pole,
			   ComplexFloat &residue) const = 0;

  ////////////////////////////////////////////////////////////////
  // Parasitic Network (detailed parasitics).
  // This api assumes that parasitic networks are not rise/fall
  // dependent because they do not include pin capacitances.
  virtual bool isParasiticNetwork(const Parasitic *parasitic) const = 0;
  virtual Parasitic *findParasiticNetwork(const Net *net,
					  const ParasiticAnalysisPt *ap) const = 0;
  virtual Parasitic *findParasiticNetwork(const Pin *pin,
					  const ParasiticAnalysisPt *ap) const = 0;
  virtual Parasitic *makeParasiticNetwork(const Net *net,
					  bool includes_pin_caps,
					  const ParasiticAnalysisPt *ap) = 0;
  virtual ParasiticNodeSeq nodes(const Parasitic *parasitic) const = 0;
  virtual void report(const Parasitic *parasitic) const;
  virtual const Net *net(const Parasitic *parasitic) const = 0;
  virtual ParasiticResistorSeq resistors(const Parasitic *parasitic) const = 0;
  virtual ParasiticCapacitorSeq capacitors(const Parasitic *parasitic) const = 0;
  // Delete parasitic network if it exists.
  virtual void deleteParasiticNetwork(const Net *net,
				      const ParasiticAnalysisPt *ap) = 0;
  virtual void deleteParasiticNetworks(const Net *net) = 0;
  // True if the parasitic network caps include pin capacitances.
  virtual bool includesPinCaps(const Parasitic *parasitic) const = 0;
  // Parasitic network component builders.
  virtual ParasiticNode *findParasiticNode(Parasitic *parasitic,
                                           const Net *net,
                                           int id,
                                           const Network *network) const = 0;
  // Make a subnode of the parasitic network net.
  virtual ParasiticNode *ensureParasiticNode(Parasitic *parasitic,
					     const Net *net,
					     int id,
                                             const Network *network) = 0;
  // Find the parasitic node connected to pin.
  virtual ParasiticNode *findParasiticNode(const Parasitic *parasitic,
                                           const Pin *pin) const = 0;
  // deprecated 2024-02-27
  virtual ParasiticNode *findNode(const Parasitic *parasitic,
				  const Pin *pin) const __attribute__ ((deprecated));
  // Make a subnode of the parasitic network net connected to pin.
  virtual ParasiticNode *ensureParasiticNode(Parasitic *parasitic,
					     const Pin *pin,
                                             const Network *network) = 0;
  // Increment the grounded capacitance on node.
  virtual void incrCap(ParasiticNode *node,
		       float cap) = 0;
  virtual const char *name(const ParasiticNode *node) const = 0;
  virtual const Pin *pin(const ParasiticNode *node) const = 0;
  virtual const Net *net(const ParasiticNode *node,
                         const Network *network) const = 0;
  virtual unsigned netId(const ParasiticNode *node) const = 0;
  virtual bool isExternal(const ParasiticNode *node) const = 0;
  // Node capacitance to ground.
  virtual float nodeGndCap(const ParasiticNode *node) const = 0;

  // Coupling capacitor between parasitic nodes on a net.
  virtual void makeCapacitor(Parasitic *parasitic,
                             size_t id,
                             float cap,
                             ParasiticNode *node1,
                             ParasiticNode *node2) = 0;
  virtual size_t id(const ParasiticCapacitor *capacitor) const = 0;
  virtual float value(const ParasiticCapacitor *capacitor) const = 0;
  virtual ParasiticNode *node1(const ParasiticCapacitor *capacitor) const = 0;
  virtual ParasiticNode *node2(const ParasiticCapacitor *capacitor) const = 0;
  virtual ParasiticNode *otherNode(const ParasiticCapacitor *capacitor,
                                   ParasiticNode *node) const;

  virtual void makeResistor(Parasitic *parasitic,
			    size_t id,
			    float res,
                            ParasiticNode *node1,
			    ParasiticNode *node2) = 0;
  virtual size_t id(const ParasiticResistor *resistor) const = 0;
  virtual float value(const ParasiticResistor *resistor) const = 0;
  virtual ParasiticNode *node1(const ParasiticResistor *resistor) const = 0;
  virtual ParasiticNode *node2(const ParasiticResistor *resistor) const = 0;
  virtual ParasiticNode *otherNode(const ParasiticResistor *capacitor,
                                   ParasiticNode *node) const;

  // Iteration over resistors connected to a nodes.
  // ParasiticNodeResistorMap resistor_map =
  //   parasitics_->parasiticNodeResistorMap(parasitic_network);
  // ParasiticResistorSeq &resistors = resistor_map_[node];
  // for (ParasiticResistor *resistor : resistors) {
  // }
  ParasiticNodeResistorMap parasiticNodeResistorMap(const Parasitic *parasitic) const;
  ParasiticNodeCapacitorMap parasiticNodeCapacitorMap(const Parasitic *parasitic) const;

  // Filters loads that are missing path from driver.
  virtual PinSet unannotatedLoads(const Parasitic *parasitic,
                                  const Pin *drvr_pin) const = 0;
  // unannotatedLoads helper.
  PinSet loads(const Pin *drvr_pin) const;

  // Reduce parasitic network to pi elmore model for drvr_pin.
  Parasitic *reduceToPiElmore(const Parasitic *parasitic,
                              const Pin *drvr_pin,
                              const RiseFall *rf,
                              const Corner *corner,
                              const MinMax *cnst_min_max,
                              const ParasiticAnalysisPt *ap);
  // Reduce parasitic network to pi and 2nd order pole/residue models
  // for drvr_pin.
  Parasitic *reduceToPiPoleResidue2(const Parasitic *parasitic,
                                    const Pin *drvr_pin,
                                    const RiseFall *rf,
                                    const Corner *corner,
                                    const MinMax *cnst_min_max,
                                    const ParasiticAnalysisPt *ap);

  // Estimate parasitic as pi elmore using wireload model.
  Parasitic *estimatePiElmore(const Pin *drvr_pin,
                              const RiseFall *rf,
                              const Wireload *wireload,
                              float fanout,
                              float net_pin_cap,
                              const Corner *corner,
                              const MinMax *min_max);
  Parasitic *makeWireloadNetwork(const Pin *drvr_pin,
				 const Wireload *wireload,
				 float fanout,
                                 const MinMax *min_max,
				 const ParasiticAnalysisPt *ap);
  // Network edit before/after methods.
  virtual void disconnectPinBefore(const Pin *pin,
                                   const Network *network) = 0;
  virtual void loadPinCapacitanceChanged(const Pin *pin) = 0;

protected:
  void makeWireloadNetworkWorst(Parasitic *parasitic,
				const Pin *drvr_pin,
                                const Net *net,
				float wireload_cap,
				float wireload_res,
				float fanout);
  void makeWireloadNetworkBest(Parasitic *parasitic,
			       const Pin *drvr_pin,
			       float wireload_cap,
			       float wireload_res,
			       float fanout);
  void makeWireloadNetworkBalanced(Parasitic *parasitic,
				   const Pin *drvr_pin,
				   float wireload_cap,
				   float wireload_res,
				   float fanout);

  const Net *findParasiticNet(const Pin *pin) const;
};

// Managed by the Corner class.
class ParasiticAnalysisPt
{
public:
  ParasiticAnalysisPt(const char *name,
		      int index,
                      int index_max);
  const char *name() const { return name_.c_str(); }
  int index() const { return index_; }
  int indexMax() const { return index_max_; }
  // Coupling capacitor factor used by all reduction functions.
  float couplingCapFactor() const { return coupling_cap_factor_; }
  void setCouplingCapFactor(float factor);

private:
  string name_;
  int index_;
  int index_max_;
  float coupling_cap_factor_;
};

class ParasiticNodeLess
{
public:
  ParasiticNodeLess(const Parasitics *parasitics,
                    const Network *network);
  ParasiticNodeLess(const ParasiticNodeLess &less);
  bool operator()(const ParasiticNode *node1,
                  const ParasiticNode *node2) const;
private:
  const Parasitics *parasitics_;
  const Network *network_;
};

} // namespace
