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

#include "Parasitics.hh"

namespace sta {

class ConcretePoleResidue;
class ConcreteParasiticDevice;
class ConcreteParasiticPinNode;
class ConcreteParasiticSubNode;
class ConcreteParasiticNode;

typedef Map<const Pin*, float> ConcreteElmoreLoadMap;
typedef ConcreteElmoreLoadMap::Iterator ConcretePiElmoreLoadIterator;
typedef Map<const Pin*, ConcretePoleResidue*> ConcretePoleResidueMap;
typedef std::pair<const Net*, int> NetId;
struct NetIdLess
{
  bool operator()(const NetId *net_id1,
		  const NetId *net_id2) const;
};
typedef Map<NetId*, ConcreteParasiticSubNode*,
	    NetIdLess > ConcreteParasiticSubNodeMap;
typedef Map<const Pin*,
	    ConcreteParasiticPinNode*> ConcreteParasiticPinNodeMap;
typedef Vector<ConcreteParasiticDevice*> ConcreteParasiticDeviceSeq;
typedef Set<ConcreteParasiticDevice*> ConcreteParasiticDeviceSet;
typedef Vector<ConcreteParasiticNode*> ConcreteParasiticNodeSeq;

// Empty base class definitions so casts are not required on returned
// objects.
class Parasitic {};
class ParasiticNode {};
class ParasiticDevice {};
class ParasiticNetwork {};

// Base class for parasitics.
class ConcreteParasitic : public Parasitic
{
public:
  virtual ~ConcreteParasitic();
  virtual float capacitance() const = 0;
  virtual bool isPiElmore() const;
  virtual bool isPiModel() const;
  virtual bool isPiPoleResidue() const;
  virtual bool isPoleResidue() const;
  virtual bool isParasiticNetwork() const;
  virtual void piModel(float &c2,
		       float &rpi,
		       float &c1) const;
  virtual void setPiModel(float c2,
			  float rpi,
			  float c1);
  virtual bool isReducedParasiticNetwork() const;
  virtual void setIsReduced(bool reduced);
  virtual void findElmore(const Pin *load_pin,
			  float &elmore,
			  bool &exists) const;
  virtual void setElmore(const Pin *load_pin,
			 float elmore);
  virtual Parasitic *findPoleResidue(const Pin *load_pin) const;
  virtual void setPoleResidue(const Pin *load_pin,
			      ComplexFloatSeq *poles,
			      ComplexFloatSeq *residues);
  virtual ParasiticDeviceIterator *deviceIterator();
  virtual ParasiticNodeIterator *nodeIterator();
};

class ConcreteElmore
{
public:
  void findElmore(const Pin *load_pin,
		  float &elmore,
		  bool &exists) const;
  void deleteLoad(const Pin *load_pin);
  void setElmore(const Pin *load_pin,
		 float elmore);

protected:
  ConcreteElmore();
  virtual ~ConcreteElmore();

private:
  ConcreteElmoreLoadMap *loads_;
};

// Pi model for a driver pin.
class ConcretePi
{
public:
  ConcretePi(float c2,
	     float rpi,
	     float c1);
  float capacitance() const;
  void piModel(float &c2,
	       float &rpi,
	       float &c1) const;
  void setPiModel(float c2,
		  float rpi,
		  float c1);
  bool isReducedParasiticNetwork() const { return is_reduced_; }
  void setIsReduced(bool reduced);

protected:
  float c2_;
  float rpi_;
  float c1_;
  bool is_reduced_;
};

// Pi model for a driver pin and the elmore delay to each load.
class ConcretePiElmore : public ConcretePi,
			 public ConcreteElmore,
			 public ConcreteParasitic
{
public:
  ConcretePiElmore(float c2,
		   float rpi,
		   float c1);
  virtual bool isPiElmore() const { return true; }
  virtual bool isPiModel() const { return true; }
  virtual float capacitance() const;
  virtual void piModel(float &c2, float &rpi, float &c1) const;
  virtual void setPiModel(float c2, float rpi, float c1);
  virtual bool isReducedParasiticNetwork() const;
  virtual void setIsReduced(bool reduced);
  virtual void findElmore(const Pin *load_pin, float &elmore,
			  bool &exists) const;
  virtual void setElmore(const Pin *load_pin, float elmore);
};

// PiElmore from wireload model estimate.
class ConcretePiElmoreEstimated : public ConcretePi,
				  public ConcreteParasitic
{
public:
  ConcretePiElmoreEstimated(float c2,
			    float rpi,
			    float c1,
			    float elmore_res,
			    float elmore_cap,
			    bool elmore_use_load_cap,
			    const RiseFall *rf,
			    const OperatingConditions *op_cond,
			    const Corner *corner,
			    const MinMax *min_max,
			    Sdc *sdc);
  virtual float capacitance() const;
  virtual bool isPiElmore() const { return true; }
  virtual bool isPiModel() const { return true; }
  virtual void piModel(float &c2, float &rpi, float &c1) const;
  virtual void findElmore(const Pin *load_pin, float &elmore,
			  bool &exists) const;
  virtual void setElmore(const Pin *load_pin, float elmore);

private:
  float elmore_res_;
  float elmore_cap_;
  bool elmore_use_load_cap_;
  const RiseFall *rf_;
  const OperatingConditions *op_cond_;
  const Corner *corner_;
  const MinMax *min_max_;
  Sdc *sdc_;
};

class ConcretePoleResidue : public ConcreteParasitic
{
public:
  ConcretePoleResidue(ComplexFloatSeq *poles,
		      ComplexFloatSeq *residues);
  virtual ~ConcretePoleResidue();
  virtual bool isPoleResidue() const { return true; }
  size_t poleResidueCount() const;
  void poleResidue(int index, ComplexFloat &pole, ComplexFloat &residue) const;
  void setPoleResidue(ComplexFloatSeq *poles, ComplexFloatSeq *residues);
  float capacitance() const { return 0.0; }

  using ConcreteParasitic::setPoleResidue;

private:
  ComplexFloatSeq *poles_;
  ComplexFloatSeq *residues_;
};

// Pi model for a driver pin and the pole/residues to each load.
class ConcretePiPoleResidue : public ConcretePi,
			      public ConcreteParasitic
{
public:
  ConcretePiPoleResidue(float c2,
			float rpi,
			float c1);
  virtual ~ConcretePiPoleResidue();
  virtual bool isPiPoleResidue() const { return true; }
  virtual bool isPiModel() const { return true; }
  virtual float capacitance() const;
  virtual void piModel(float &c2,
		       float &rpi,
		       float &c1) const;
  virtual void setPiModel(float c2,
			  float rpi,
			  float c1);
  virtual bool isReducedParasiticNetwork() const;
  virtual void setIsReduced(bool reduced);
  virtual Parasitic *findPoleResidue(const Pin *load_pin) const;
  virtual void setPoleResidue(const Pin *load_pin,
			      ComplexFloatSeq *poles,
			      ComplexFloatSeq *residues);
  void deleteLoad(const Pin *load_pin);

private:
  ConcretePoleResidueMap *load_pole_residue_;
};

class ConcreteParasiticNode : public ParasiticNode
{
public:
  virtual ~ConcreteParasiticNode() {}
  float capacitance() const;
  virtual const char *name(const Network *network) const = 0;
  virtual bool isPinNode() const { return false; }
  ConcreteParasiticDeviceSeq *devices() { return &devices_; }
  void incrCapacitance(float cap);
  void addDevice(ConcreteParasiticDevice *device);

protected:
  ConcreteParasiticNode();

  float cap_;
  ConcreteParasiticDeviceSeq devices_;

  friend class ConcreteParasiticNetwork;
};

class ConcreteParasiticSubNode : public ConcreteParasiticNode
{
public:
  ConcreteParasiticSubNode(const Net *net,
			   int id);
  virtual const char *name(const Network *network) const;

private:
  const Net *net_;
  int id_;
};

class ConcreteParasiticPinNode : public ConcreteParasiticNode
{
public:
  ConcreteParasiticPinNode(const Pin *pin);
  const Pin *pin() const { return pin_; }
  virtual bool isPinNode() const { return true; }
  virtual const char *name(const Network *network) const;

private:
  const Pin *pin_;
};

class ConcreteParasiticDevice : public ParasiticDevice
{
public:
  ConcreteParasiticDevice(const char *name,
			  ConcreteParasiticNode *node,
			  float value);
  virtual ~ConcreteParasiticDevice();
  virtual bool isResistor() const { return false; }
  virtual bool isCouplingCap() const { return false; }
  const char *name() const { return name_; }
  float value() const { return value_; }
  ConcreteParasiticNode *node1() const { return node_; }
  virtual ConcreteParasiticNode *node2() const = 0;
  virtual ParasiticNode *otherNode(ParasiticNode *node) const = 0;
  virtual void replaceNode(ConcreteParasiticNode *from_node,
			   ConcreteParasiticNode *to_node) = 0;

protected:
  const char *name_;
  ConcreteParasiticNode *node_;
  float value_;

  friend class ConcreteParasiticNetwork;
};

class ConcreteParasiticResistor : public ConcreteParasiticDevice
{
public:
  ConcreteParasiticResistor(const char *name,
			    ConcreteParasiticNode *node,
			    ConcreteParasiticNode *other_node,
			    float res);
  virtual bool isResistor() const { return true; }
  virtual ConcreteParasiticNode *node2() const { return other_node_; }
  virtual ParasiticNode *otherNode(ParasiticNode *node) const;
  virtual void replaceNode(ConcreteParasiticNode *from_node,
			   ConcreteParasiticNode *to_node);

private:
  ConcreteParasiticNode *other_node_;
};

// Base class for coupling capacitors.
class ConcreteCouplingCap : public ConcreteParasiticDevice
{
public:
  ConcreteCouplingCap(const char *name,
		      ConcreteParasiticNode *node,
		      float cap);
  virtual bool isCouplingCap() const { return true; }
  virtual ConcreteParasiticNode *node2() const { return nullptr; }
  virtual void replaceNode(ConcreteParasiticNode *from_node,
			   ConcreteParasiticNode *to_node);
};

class ConcreteCouplingCapInt : public ConcreteCouplingCap
{
public:
  ConcreteCouplingCapInt(const char *name,
			 ConcreteParasiticNode *node,
			 ConcreteParasiticNode *other_node,
			 float cap);
  virtual bool isCouplingCap() const { return true; }
  virtual ConcreteParasiticNode *node2() const { return other_node_; }
  virtual ParasiticNode *otherNode(ParasiticNode *node) const;
  virtual void replaceNode(ConcreteParasiticNode *from_node,
			   ConcreteParasiticNode *to_node);

private:
  ConcreteParasiticNode *other_node_;
};

class ConcreteCouplingCapExtNode : public ConcreteCouplingCap
{
public:
  ConcreteCouplingCapExtNode(const char *name,
			     ConcreteParasiticNode *node,
			     Net *other_node_net,
			     int other_node_id,
			     float cap);
  virtual bool isCouplingCap() const { return true; }
  virtual ParasiticNode *otherNode(ParasiticNode *node) const;
  virtual void replaceNode(ConcreteParasiticNode *from_node,
			   ConcreteParasiticNode *to_node);

private:
};

class ConcreteCouplingCapExtPin : public ConcreteCouplingCap
{
public:
  ConcreteCouplingCapExtPin(const char *name,
			    ConcreteParasiticNode *node,
			    Pin *other_node_pin,
			    float cap);
  virtual bool isCouplingCap() const { return true; }
  virtual ParasiticNode *otherNode(ParasiticNode *node) const;
  virtual void replaceNode(ConcreteParasiticNode *from_node,
			   ConcreteParasiticNode *to_node);

private:
};

class ConcreteParasiticDeviceSetIterator : public ParasiticDeviceIterator
{
public:
  ConcreteParasiticDeviceSetIterator(ConcreteParasiticDeviceSet *devices);
  virtual ~ConcreteParasiticDeviceSetIterator();
  bool hasNext() { return iter_.hasNext(); }
  ParasiticDevice *next() { return iter_.next(); }

private:
  ConcreteParasiticDeviceSet::ConstIterator iter_;
};

class ConcreteParasiticDeviceSeqIterator : public ParasiticDeviceIterator
{
public:
  ConcreteParasiticDeviceSeqIterator(ConcreteParasiticDeviceSeq *devices);
  bool hasNext() { return iter_.hasNext(); }
  ParasiticDevice *next() { return iter_.next(); }

private:
  ConcreteParasiticDeviceSeq::ConstIterator iter_;
};

class ConcreteParasiticNodeSeqIterator : public ParasiticNodeIterator
{
public:
  ConcreteParasiticNodeSeqIterator(ConcreteParasiticNodeSeq *devices);
  virtual ~ConcreteParasiticNodeSeqIterator();
  bool hasNext() { return iter_.hasNext(); }
  ParasiticNode *next() { return iter_.next(); }

private:
  ConcreteParasiticNodeSeq::ConstIterator iter_;
};

class ConcreteParasiticNetwork : public ParasiticNetwork,
				 public ConcreteParasitic
{
public:
  ConcreteParasiticNetwork(bool includes_pin_caps);
  virtual ~ConcreteParasiticNetwork();
  virtual bool isParasiticNetwork() const { return true; }
  bool includesPinCaps() const { return includes_pin_caps_; }
  ConcreteParasiticNode *ensureParasiticNode(const Net *net,
					     int id);
  ConcreteParasiticNode *findNode(const Pin *pin);
  ConcreteParasiticNode *ensureParasiticNode(const Pin *pin);
  virtual float capacitance() const;
  ConcreteParasiticPinNodeMap *pinNodes() { return &pin_nodes_; }
  ConcreteParasiticSubNodeMap *subNodes() { return &sub_nodes_; }
  void disconnectPin(const Pin *pin,
		     Net *net);
  virtual ParasiticDeviceIterator *deviceIterator();
  virtual ParasiticNodeIterator *nodeIterator();
  virtual void devices(// Return value.
		       ConcreteParasiticDeviceSet *devices);

private:
  void deleteNodes();
  void deleteDevices();

  ConcreteParasiticSubNodeMap sub_nodes_;
  ConcreteParasiticPinNodeMap pin_nodes_;
  unsigned max_node_id_:31;
  bool includes_pin_caps_:1;
};

} // namespace
