// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#ifndef STA_CONCRETE_PARASITICS_H
#define STA_CONCRETE_PARASITICS_H

#include <mutex>
#include "Map.hh"
#include "Set.hh"
#include "MinMax.hh"
#include "EstimateParasitics.hh"
#include "Parasitics.hh"

namespace sta {

class ConcreteParasiticNode;
class ConcreteParasiticDevice;
class ConcreteLumpedElmore;
class ConcretePiElmore;
class ConcretePiPoleResidue;
class ConcreteParasiticNetwork;

typedef Map<const Pin*, ConcreteLumpedElmore*> ConcreteLumpedElmoreMap;
typedef Vector<ConcreteLumpedElmoreMap*> ConcreteLumpedElmoreMapSeq;
typedef Map<const Pin*, ConcretePiElmore*> ConcretePiElmoreMap;
typedef Vector<ConcretePiElmoreMap*> ConcretePiElmoreMapSeq;
typedef Map<const Pin*, ConcretePiPoleResidue*> ConcretePiPoleResidueMap;
typedef Vector<ConcretePiPoleResidueMap*> ConcretePiPoleResidueMapSeq;
typedef Map<const Net*, ConcreteParasiticNetwork*> ConcreteParasiticNetworkMap;
typedef Vector<ConcreteParasiticNetworkMap*> ConcreteParasiticNetworkMapSeq;

// This class acts as a BUILDER for all parasitics.
class ConcreteParasitics : public Parasitics, public EstimateParasitics
{
public:
  ConcreteParasitics(StaState *sta);
  virtual ~ConcreteParasitics();
  bool haveParasitics();
  virtual void clear();
  virtual void makeParasiticAnalysisPtAfter();

  virtual void finish(Parasitic *parasitic);
  virtual void save();
  virtual void deleteParasitics();
  virtual void deleteParasitics(const Net *net,
				const ParasiticAnalysisPt *ap);
  virtual void deleteParasitics(const Pin *drvr_pin,
				const ParasiticAnalysisPt *ap);
  virtual void deleteParasitic(const Pin *drvr_pin,
			       const TransRiseFall *tr,
			       const ParasiticAnalysisPt *ap,
			       Parasitic *parasitic);
  virtual bool isReducedParasiticNetwork(Parasitic *parasitic) const;
  virtual void setIsReducedParasiticNetwork(Parasitic *parasitic,
					    bool is_reduced);

  virtual float capacitance(Parasitic *parasitic) const;

  virtual bool isLumpedElmore(Parasitic *parasitic) const;
  virtual bool hasLumpedElmore(const Pin *drvr_pin,
			       const TransRiseFall *tr,
			       const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *findLumpedElmore(const Pin *drvr_pin,
				      const TransRiseFall *tr,
				      const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *makeLumpedElmore(const Pin *drvr_pin,
				      float cap,
				      const TransRiseFall *tr,
				      const ParasiticAnalysisPt *ap);
  virtual void deleteLumpedElmore(const Pin *drvr_pin,
				  const TransRiseFall *tr,
				  const ParasiticAnalysisPt *ap);

  virtual bool isPiElmore(Parasitic *parasitic) const;
  virtual bool hasPiElmore(const Pin *drvr_pin,
			   const TransRiseFall *tr,
			   const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *findPiElmore(const Pin *drvr_pin,
				  const TransRiseFall *tr,
				  const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *makePiElmore(const Pin *drvr_pin,
				  const TransRiseFall *tr,
				  const ParasiticAnalysisPt *ap,
				  float c2, float rpi, float c1);
  virtual void deletePiElmore(const Pin *drvr_pin,
			      const TransRiseFall *tr,
			      const ParasiticAnalysisPt *ap);

  virtual bool isPiModel(Parasitic *parasitic) const;
  virtual void piModel(Parasitic *parasitic, float &c2, float &rpi,
		       float &c1) const;
  virtual void setPiModel(Parasitic *parasitic, float c2, float rpi, float c1);

  virtual void findElmore(Parasitic *parasitic, const Pin *load_pin,
			  float &elmore, bool &exists) const;
  virtual void setElmore(Parasitic *parasitic, const Pin *load_pin,
			 float elmore);

  virtual bool isPiPoleResidue(Parasitic* parasitic) const;
  virtual bool hasPiPoleResidue(const Pin *drvr_pin,
				const TransRiseFall *tr,
				const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *findPiPoleResidue(const Pin *drvr_pin,
				       const TransRiseFall *tr,
				       const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *findPoleResidue(const Parasitic *parasitic,
				     const Pin *load_pin) const;
  virtual Parasitic *makePiPoleResidue(const Pin *drvr_pin,
				       const TransRiseFall *tr,
				       const ParasiticAnalysisPt *ap,
				       float c2, float rpi, float c1);
  virtual void setPoleResidue(Parasitic *parasitic, const Pin *load_pin,
			      ComplexFloatSeq *poles,
			      ComplexFloatSeq *residues);
  virtual bool isPoleResidue(const Parasitic* parasitic) const;
  virtual size_t poleResidueCount(const Parasitic *parasitic) const;
  virtual void poleResidue(const Parasitic *parasitic, int pole_index,
			   ComplexFloat &pole, ComplexFloat &residue) const;

  virtual bool isParasiticNetwork(Parasitic *parasitic) const;
  virtual bool hasParasiticNetwork(const Net *net,
				   const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *findParasiticNetwork(const Pin *pin,
					  const ParasiticAnalysisPt *ap) const;
  virtual Parasitic *makeParasiticNetwork(Net *net,
					  bool includes_pin_caps,
					  const ParasiticAnalysisPt *ap);
  virtual void deleteParasiticNetwork(const Net *net,
				      const ParasiticAnalysisPt *ap);
  virtual bool includesPinCaps(Parasitic *parasitic) const;
  virtual ParasiticNode *ensureParasiticNode(Parasitic *parasitic, Net *net,
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
			    ParasiticNode *node2,
			    float res, const ParasiticAnalysisPt *ap);
  virtual ParasiticDeviceIterator *deviceIterator(Parasitic *parasitic);
  virtual ParasiticNodeIterator *nodeIterator(Parasitic *parasitic);

  virtual const char *name(const ParasiticNode *node);
  virtual const Pin *connectionPin(const ParasiticNode *node) const;
  virtual ParasiticNode *findNode(Parasitic *parasitic, const Pin *pin) const;
  virtual float nodeGndCap(const ParasiticNode *node,
			   const ParasiticAnalysisPt *ap) const;
  virtual ParasiticDeviceIterator *
  deviceIterator(ParasiticNode *node) const;
  virtual bool isResistor(const ParasiticDevice *device) const;
  virtual bool isCouplingCap(const ParasiticDevice *device) const;
  virtual const char *name(const ParasiticDevice *device) const;
  virtual float value(const ParasiticDevice *device,
		      const ParasiticAnalysisPt *ap) const;
  virtual ParasiticNode *node1(const ParasiticDevice *device) const;
  virtual ParasiticNode *node2(const ParasiticDevice *device) const;
  virtual ParasiticNode *otherNode(const ParasiticDevice *device,
				   ParasiticNode *node) const;

  virtual Parasitic *estimatePiElmore(const Pin *drvr_pin,
				      const TransRiseFall *tr,
				      const Wireload *wireload,
				      float fanout,
				      float net_pin_cap,
				      const OperatingConditions *op_cond,
				      const Corner *corner,
				      const MinMax *min_max,
				      const ParasiticAnalysisPt *ap);
  virtual void disconnectPinBefore(const Pin *pin);
  virtual void reduceTo(Parasitic *parasitic,
			const Net *net,
			ReduceParasiticsTo reduce_to,
			const TransRiseFall *tr,
			const OperatingConditions *op_cond,
			const Corner *corner,
			const MinMax *cnst_min_max,
			const ParasiticAnalysisPt *ap);
  virtual void reduceToPiElmore(Parasitic *parasitic,
				const Net *net,
				const TransRiseFall *tr,
				const OperatingConditions *op_cond,
				const Corner *corner,
				const MinMax *cnst_min_max,
				const ParasiticAnalysisPt *ap);
  virtual Parasitic *reduceToPiElmore(Parasitic *parasitic,
				      const Pin *drvr_pin,
				      const TransRiseFall *tr,
				      const OperatingConditions *op_cond,
				      const Corner *corner,
				      const MinMax *cnst_min_max,
				      const ParasiticAnalysisPt *ap);
  virtual void reduceToPiPoleResidue2(Parasitic *parasitic,
				      const Net *net,
				      const TransRiseFall *tr,
				      const OperatingConditions *op_cond,
				      const Corner *corner,
				      const MinMax *cnst_min_max,
				      const ParasiticAnalysisPt *ap);
  virtual Parasitic *reduceToPiPoleResidue2(Parasitic *parasitic,
					    const Pin *drvr_pin,
					    const TransRiseFall *tr,
					    const OperatingConditions *op_cond,
					    const Corner *corner,
					    const MinMax *cnst_min_max,
					    const ParasiticAnalysisPt *ap);

protected:
  int mapSize();
  int parasiticAnalysisPtIndex(const ParasiticAnalysisPt *ap,
			       const TransRiseFall *tr) const;
  int parasiticNetworkAnalysisPtIndex(const ParasiticAnalysisPt *ap) const;
  Parasitic *ensureRspf(const Pin *drvr_pin);
  void deleteParasitics(int ap_index);
  void disconnectPinBefore(const Pin *drvr_pin, const Pin *pin, int ap_index);
  void makeAnalysisPtAfter();

  // Arrays of parasitic lookup maps indexed by analysis pt index.
  ConcreteLumpedElmoreMapSeq *lumped_elmore_maps_;
  ConcretePiElmoreMapSeq *pi_elmore_maps_;
  ConcretePiPoleResidueMapSeq *pi_pole_residue_maps_;
  ConcreteParasiticNetworkMapSeq *parasitic_network_maps_;
  mutable std::mutex lock_;

  using EstimateParasitics::estimatePiElmore;
  friend class ConcreteLumpedElmore;
  friend class ConcretePiElmore;
  friend class ConcreteParasiticNode;
  friend class ConcreteParasiticResistor;
  friend class ConcreteParasiticNetwork;
};

} // namespace
#endif
