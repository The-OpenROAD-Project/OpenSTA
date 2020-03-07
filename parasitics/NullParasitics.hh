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

#include "DisallowCopyAssign.hh"
#include "Parasitics.hh"

namespace sta {

// Parasitics that are not in the house.
class NullParasitics : public Parasitics
{
public:
  NullParasitics(StaState *sta);
  virtual bool haveParasitics();
  virtual void clear();
  virtual void save();
  virtual void deleteParasitics();
  virtual void deleteParasitics(const Net *net,
				const ParasiticAnalysisPt *ap);
  virtual void deleteParasitics(const Pin *pin,
				const ParasiticAnalysisPt *ap);
  virtual void deleteUnsavedParasitic(Parasitic *parasitic);
  virtual void deleteDrvrReducedParasitics(const Pin *drvr_pin);

  virtual float capacitance(Parasitic *parasitic) const;

  virtual Parasitic *
  findPiElmore(const Pin *drvr_pin,
	       const RiseFall *rf,
	       const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *makePiElmore(const Pin *drvr_pin,
				  const RiseFall *rf,
				  const ParasiticAnalysisPt *ap,
				  float c2, float rpi, float c1);
  virtual bool isPiElmore(Parasitic *parasitic) const;
  virtual bool
  isReducedParasiticNetwork(Parasitic *parasitic) const;
  virtual void setIsReducedParasiticNetwork(Parasitic *parasitic,
					    bool is_reduced);
  virtual void piModel(Parasitic *parasitic, float &c2, float &rpi,
		       float &c1) const;
  virtual void setPiModel(Parasitic *parasitic, float c2, float rpi,
			  float c1);
  virtual void findElmore(Parasitic *parasitic, const Pin *load_pin,
			  float &elmore, bool &exists) const;
  virtual void setElmore(Parasitic *parasitic, const Pin *load_pin,
			 float elmore);

  virtual bool isPiModel(Parasitic* parasitic) const;
  virtual bool isPiPoleResidue(Parasitic* parasitic) const;
  virtual Parasitic *
  findPiPoleResidue(const Pin *drvr_pin,
		    const RiseFall *rf,
		    const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *makePiPoleResidue(const Pin *drvr_pin,
				       const RiseFall *rf,
				       const ParasiticAnalysisPt *ap,
				       float c2, float rpi,
				       float c1);
  virtual Parasitic *findPoleResidue(const Parasitic *parasitic,
				     const Pin *load_pin) const;
  virtual void setPoleResidue(Parasitic *parasitic, const Pin *load_pin,
			      ComplexFloatSeq *poles,
			      ComplexFloatSeq *residues);
  virtual bool isPoleResidue(const Parasitic* parasitic) const;
  virtual size_t poleResidueCount(const Parasitic *parasitic) const;
  virtual void poleResidue(const Parasitic *parasitic, int pole_index,
			   ComplexFloat &pole, ComplexFloat &residue) const;

  virtual bool isParasiticNetwork(Parasitic *parasitic) const;
  virtual Parasitic *findParasiticNetwork(const Net *net,
					  const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *
  findParasiticNetwork(const Pin *pin,
		       const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *
  makeParasiticNetwork(const Net *net,
		       bool pin_cap_included,
		       const ParasiticAnalysisPt *ap);
  virtual ParasiticDeviceIterator *deviceIterator(Parasitic *) { return nullptr; }
  virtual ParasiticNodeIterator *nodeIterator(Parasitic *) { return nullptr; }
  virtual bool includesPinCaps(Parasitic *parasitic) const;
  virtual void deleteParasiticNetwork(const Net *net,
				      const ParasiticAnalysisPt *ap);
  virtual ParasiticNode *ensureParasiticNode(Parasitic *parasitic,
					     const Net *net,
					     int id);
  virtual ParasiticNode *ensureParasiticNode(Parasitic *parasitic,
					     const Pin *pin);
  virtual void incrCap(ParasiticNode *node, float cap,
		       const ParasiticAnalysisPt *ap);
  virtual void makeCouplingCap(const char *name,
			       ParasiticNode *node,
			       ParasiticNode *other_node,
			       float cap, const ParasiticAnalysisPt *ap);
  virtual void makeCouplingCap(const char *name,
			       ParasiticNode *node,
			       Net *other_node_net, int other_node_id,
			       float cap, const ParasiticAnalysisPt *ap);
  virtual void makeCouplingCap(const char *name,
			       ParasiticNode *node,
			       Pin *other_node_pin,
			       float cap, const ParasiticAnalysisPt *ap);
  virtual void makeResistor(const char *name, ParasiticNode *node1,
			    ParasiticNode *node2, float res,
			    const ParasiticAnalysisPt *ap);

  virtual const char *name(const ParasiticNode *node);
  virtual const Pin *connectionPin(const ParasiticNode *node) const;
  virtual ParasiticNode *findNode(Parasitic *parasitic,
				  const Pin *pin) const;
  virtual float nodeGndCap(const ParasiticNode *node,
			   const ParasiticAnalysisPt *ap) const;
  virtual ParasiticDeviceIterator *
  deviceIterator(ParasiticNode *node) const;
  virtual bool isResistor(const ParasiticDevice *device) const;
  virtual bool isCouplingCap(const ParasiticDevice *device)const;
  virtual const char *name(const ParasiticDevice *device) const;
  virtual float value(const ParasiticDevice *device,
		      const ParasiticAnalysisPt *ap) const;
  virtual ParasiticNode *node1(const ParasiticDevice *device) const;
  virtual ParasiticNode *node2(const ParasiticDevice *device) const;
  virtual ParasiticNode *otherNode(const ParasiticDevice *device,
				   ParasiticNode *node) const;
  // Reduce parasitic network to reduce_to model.
  virtual void reduceTo(Parasitic *parasitic,
			const Net *net,
			ReduceParasiticsTo reduce_to,
			const OperatingConditions *op_cond,
			const Corner *corner,
			const MinMax *cnst_min_max,
			const ParasiticAnalysisPt *ap);
  virtual void reduceToPiElmore(Parasitic *parasitic,
				const Net *net,
				const OperatingConditions *op_cond,
				const Corner *corner,
				const MinMax *cnst_min_max,
				const ParasiticAnalysisPt *ap);
  // Reduce parasitic network to pi elmore model for drvr_pin.
  virtual void reduceToPiElmore(Parasitic *parasitic,
				const Pin *drvr_pin,
				const OperatingConditions *op_cond,
				const Corner *corner,
				const MinMax *cnst_min_max,
				const ParasiticAnalysisPt *ap);
  virtual void reduceToPiPoleResidue2(Parasitic *parasitic,
				      const Net *net,
				      const OperatingConditions *op_cond,
				      const Corner *corner,
				      const MinMax *cnst_min_max,
				      const ParasiticAnalysisPt *ap);
  virtual void reduceToPiPoleResidue2(Parasitic *parasitic,
				      const Pin *drvr_pin,
				      const OperatingConditions *op_cond,
				      const Corner *corner,
				      const MinMax *cnst_min_max,
				      const ParasiticAnalysisPt *ap);
  virtual Parasitic *
  estimatePiElmore(const Pin *drvr_pin,
		   const RiseFall *rf,
		   const Wireload *wireload,
		   float fanout,
		   float net_pin_cap,
		   const OperatingConditions *op_cond,
		   const Corner *corner,
		   const MinMax *min_max,
		   const ParasiticAnalysisPt *ap);

  virtual void disconnectPinBefore(const Pin *pin);
  virtual void loadPinCapacitanceChanged(const Pin *pin);

private:
  DISALLOW_COPY_AND_ASSIGN(NullParasitics);
};

} // namespace
