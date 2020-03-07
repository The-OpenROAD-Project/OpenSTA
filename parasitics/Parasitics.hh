// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <complex>
#include "DisallowCopyAssign.hh"
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
typedef Iterator<ParasiticDevice*> ParasiticDeviceIterator;
typedef Iterator<ParasiticNode*> ParasiticNodeIterator;

// Parasitics API.
// All parasitic parameters can have multiple values, each corresponding
// to an analysis point.
// Parasitic annotation for a pin or net may exist for one analysis point
// and not another.
// If there is only one parasitic for both rise and fall transitions
// the sta parasitic readers will save it under the rise transition.
class Parasitics : public StaState
{
public:
  virtual ~Parasitics() {}
  virtual bool haveParasitics() = 0;
  // Clear all state.
  virtual void clear() = 0;

  // Save parasitics to database file.
  virtual void save() = 0;
  // Delete all parasitics.
  virtual void deleteParasitics() = 0;
  // Delete all parasitics on net at analysis point.
  virtual void deleteParasitics(const Net *net,
				const ParasiticAnalysisPt *ap) = 0;
  // Delete all parasitics on pin at analysis point.
  virtual void deleteParasitics(const Pin *pin,
				const ParasiticAnalysisPt *ap) = 0;
  virtual void deleteUnsavedParasitic(Parasitic *parasitic) = 0;
  virtual void deleteDrvrReducedParasitics(const Pin *drvr_pin) = 0;

  virtual bool isReducedParasiticNetwork(Parasitic *parasitic) const = 0;
  // Flag this parasitic as reduced from a parasitic network.
  virtual void setIsReducedParasiticNetwork(Parasitic *parasitic,
					    bool is_reduced) = 0;

  // Capacitance value of parasitic object.
  virtual float capacitance(Parasitic *parasitic) const = 0;

  ////////////////////////////////////////////////////////////////
  // Pi model driver load with elmore delays to load pins (RSPF).
  // This follows the SPEF documentation of c2/c1, with c2 being the
  // capacitor on the driver pin.
  virtual bool isPiElmore(Parasitic *parasitic) const = 0;
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
  virtual bool isPiModel(Parasitic *parasitic) const = 0;
  virtual void piModel(Parasitic *parasitic,
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
  virtual void findElmore(Parasitic *parasitic,
			  const Pin *load_pin,
			  float &elmore,
			  bool &exists) const = 0;
  // Set load elmore delay.
  virtual void setElmore(Parasitic *parasitic,
			 const Pin *load_pin,
			 float elmore) = 0;

  ////////////////////////////////////////////////////////////////
  // Pi model driver load with pole/residue interconnect model to load pins.
  virtual bool isPiPoleResidue(Parasitic* parasitic) const = 0;
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
			   ComplexFloat &residue) const=0;

  ////////////////////////////////////////////////////////////////
  // Parasitic Network (detailed parasitics).
  // This api assumes that parasitic networks are not rise/fall
  // dependent because they do not include pin capacitances.
  virtual bool isParasiticNetwork(Parasitic *parasitic) const = 0;
  virtual Parasitic *findParasiticNetwork(const Net *net,
					  const ParasiticAnalysisPt *ap) const = 0;
  virtual Parasitic *findParasiticNetwork(const Pin *pin,
					  const ParasiticAnalysisPt *ap) const = 0;
  virtual Parasitic *makeParasiticNetwork(const Net *net,
					  bool includes_pin_caps,
					  const ParasiticAnalysisPt *ap) = 0;
  virtual ParasiticDeviceIterator *deviceIterator(Parasitic *parasitic) = 0;
  virtual ParasiticNodeIterator *nodeIterator(Parasitic *parasitic) = 0;
  // Delete parasitic network if it exists.
  virtual void deleteParasiticNetwork(const Net *net,
				      const ParasiticAnalysisPt *ap) = 0;
  // True if the parasitic network caps include pin capacitances.
  virtual bool includesPinCaps(Parasitic *parasitic) const = 0;
  // Parasitic network component builders.
  // Make a subnode of the parasitic network net.
  virtual ParasiticNode *ensureParasiticNode(Parasitic *parasitic,
					     const Net *net,
					     int id) = 0;
  // Make a subnode of the parasitic network net connected to pin.
  virtual ParasiticNode *ensureParasiticNode(Parasitic *parasitic,
					     const Pin *pin) = 0;
  // Increment the grounded capacitance on node.
  virtual void incrCap(ParasiticNode *node,
		       float cap,
		       const ParasiticAnalysisPt *ap) = 0;
  // Coupling capacitor between parasitic nodes on a net.
  // name is optional.  The device takes ownership of the name string.
  virtual void makeCouplingCap(const char *name,
			       ParasiticNode *node,
			       ParasiticNode *other_node,
			       float cap,
			       const ParasiticAnalysisPt *ap) = 0;
  // Coupling capacitor to parasitic node on a different net.
  // name is optional.  The device takes ownership of the name string.
  virtual void makeCouplingCap(const char *name,
			       ParasiticNode *node,
			       Net *other_node_net,
			       int other_node_id,
			       float cap,
			       const ParasiticAnalysisPt *ap) = 0;
  // Coupling capacitor to pin on a different net.
  // name is optional.  The device takes ownership of the name string.
  virtual void makeCouplingCap(const char *name,
			       ParasiticNode *node,
			       Pin *other_node_pin,
			       float cap,
			       const ParasiticAnalysisPt *ap) = 0;
  // name is optional.  The device takes ownership of the name string.
  virtual void makeResistor(const char *name,
			    ParasiticNode *node1,
			    ParasiticNode *node2,
			    float res,
			    const ParasiticAnalysisPt *ap) = 0;
  // Check integrity of parasitic network.
  void check(Parasitic *parasitic) const;

  virtual const char *name(const ParasiticNode *node) = 0;
  virtual const Pin *connectionPin(const ParasiticNode *node) const = 0;
  // Find the parasitic node connected to pin.
  virtual ParasiticNode *findNode(Parasitic *parasitic,
				  const Pin *pin) const = 0;
  // Node capacitance to ground.
  virtual float nodeGndCap(const ParasiticNode *node,
			   const ParasiticAnalysisPt *ap) const = 0;
  virtual ParasiticDeviceIterator *
  deviceIterator(ParasiticNode *node) const = 0;
  virtual bool isResistor(const ParasiticDevice *device) const = 0;
  virtual bool isCouplingCap(const ParasiticDevice *device) const = 0;
  virtual const char *name(const ParasiticDevice *device) const = 0;
  // Device "value" (resistance, capacitance).
  virtual float value(const ParasiticDevice *device,
		      const ParasiticAnalysisPt *ap) const = 0;
  virtual ParasiticNode *node1(const ParasiticDevice *device) const = 0;
  virtual ParasiticNode *node2(const ParasiticDevice *device) const = 0;
  virtual ParasiticNode *otherNode(const ParasiticDevice *device,
				   ParasiticNode *node) const = 0;

  // Reduce parasitic network to reduce_to model.
  virtual void reduceTo(Parasitic *parasitic,
			const Net *net,
			ReduceParasiticsTo reduce_to,
			const OperatingConditions *op_cond,
			const Corner *corner,
			const MinMax *cnst_min_max,
			const ParasiticAnalysisPt *ap) = 0;
  // Reduce parasitic network to pi elmore models.
  virtual void reduceToPiElmore(Parasitic *parasitic,
				const Net *net,
				const OperatingConditions *op_cond,
				const Corner *corner,
				const MinMax *cnst_min_max,
				const ParasiticAnalysisPt *ap) = 0;
  // Reduce parasitic network to pi elmore model for drvr_pin.
  virtual void reduceToPiElmore(Parasitic *parasitic,
				const Pin *drvr_pin,
				const OperatingConditions *op_cond,
				const Corner *corner,
				const MinMax *cnst_min_max,
				const ParasiticAnalysisPt *ap) = 0;
  // Reduce parasitic network to pi and 2nd order pole/residue models.
  virtual void reduceToPiPoleResidue2(Parasitic *parasitic,
				      const Net *net,
				      const OperatingConditions *op_cond,
				      const Corner *corner,
				      const MinMax *cnst_min_max,
				      const ParasiticAnalysisPt *ap) = 0;
  // Reduce parasitic network to pi and 2nd order pole/residue models
  // for drvr_pin.
  virtual void reduceToPiPoleResidue2(Parasitic *parasitic,
				      const Pin *drvr_pin,
				      const OperatingConditions *op_cond,
				      const Corner *corner,
				      const MinMax *cnst_min_max,
				      const ParasiticAnalysisPt *ap) = 0;

  // Estimate parasitic as pi elmore using wireload model.
  virtual Parasitic *estimatePiElmore(const Pin *drvr_pin,
				      const RiseFall *rf,
				      const Wireload *wireload,
				      float fanout,
				      float net_pin_cap,
				      const OperatingConditions *op_cond,
				      const Corner *corner,
				      const MinMax *min_max,
				      const ParasiticAnalysisPt *ap) = 0;
  Parasitic *makeWireloadNetwork(const Pin *drvr_pin,
				 const Wireload *wireload,
				 float fanout,
				 const OperatingConditions *op_cond,
				 const ParasiticAnalysisPt *ap);
  // Network edit before/after methods.
  virtual void disconnectPinBefore(const Pin *pin) = 0;
  virtual void loadPinCapacitanceChanged(const Pin *pin) = 0;

protected:
  void makeWireloadNetworkWorst(Parasitic *parasitic,
				const Pin *drvr_pin,
				float wireload_cap,
				float wireload_res,
				float fanout,
				const ParasiticAnalysisPt *ap);
  void makeWireloadNetworkBest(Parasitic *parasitic,
			       const Pin *drvr_pin,
			       float wireload_cap,
			       float wireload_res,
			       float fanout,
			       const ParasiticAnalysisPt *ap);
  void makeWireloadNetworkBalanced(Parasitic *parasitic,
				   const Pin *drvr_pin,
				   float wireload_cap,
				   float wireload_res,
				   float fanout,
				   const ParasiticAnalysisPt *ap);

  Parasitics(StaState *sta);
  Net *findParasiticNet(const Pin *pin) const;

private:
  DISALLOW_COPY_AND_ASSIGN(Parasitics);
};

// Managed by the Corner class.
class ParasiticAnalysisPt
{
public:
  ParasiticAnalysisPt(const char *name,
		      int index,
		      const MinMax *min_max);
  ~ParasiticAnalysisPt();
  const char *name() const { return name_; }
  int index() const { return index_; }
  const MinMax *minMax() const { return min_max_; }
  // Coupling capacitor factor used by all reduction functions.
  float couplingCapFactor() const { return coupling_cap_factor_; }
  void setCouplingCapFactor(float factor);

private:
  const char *name_;
  int index_;
  const MinMax *min_max_;
  float coupling_cap_factor_;
};

} // namespace
