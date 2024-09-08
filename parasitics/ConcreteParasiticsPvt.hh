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

#include <map>
#include <set>

#include "Parasitics.hh"

namespace sta {

class ConcretePoleResidue;
class ConcreteParasiticDevice;
class ConcreteParasiticNode;

typedef std::pair<const Net*, int> NetIdPair;
class NetIdPairLess
{
public:
  NetIdPairLess(const Network *network);
  bool operator()(const NetIdPair &net_id1,
		  const NetIdPair &net_id2) const;

private:
  const NetIdLess net_less_;
};

typedef std::map<const Pin*, float> ConcreteElmoreLoadMap;
typedef std::map<const Pin*, ConcretePoleResidue> ConcretePoleResidueMap;
typedef std::map<NetIdPair,ConcreteParasiticNode*,
                 NetIdPairLess> ConcreteParasiticSubNodeMap;
typedef std::map<const Pin*,ConcreteParasiticNode*,PinIdLess> ConcreteParasiticPinNodeMap;
typedef std::set<ParasiticNode*> ParasiticNodeSet;
typedef std::set<ParasiticResistor*> ParasiticResistorSet;
typedef std::vector<ParasiticResistor*> ParasiticResistorSeq;

// Empty base class definitions so casts are not required on returned
// objects.
class Parasitic {};
class ParasiticNode {};
class ParasiticResistor {};
class ParasiticCapacitor {};
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
  virtual PinSet unannotatedLoads(const Pin *drvr_pin,
                                  const Parasitics *parasitics) const = 0;
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
			 public ConcreteParasitic
{
public:
  ConcretePiElmore(float c2,
		   float rpi,
		   float c1);
  bool isPiElmore() const override { return true; }
  bool isPiModel() const override { return true; }
  float capacitance() const override;
  void piModel(float &c2,
               float &rpi,
               float &c1) const override;
  void setPiModel(float c2,
                  float rpi,
                  float c1) override;
  bool isReducedParasiticNetwork() const override;
  void setIsReduced(bool reduced) override;
  void findElmore(const Pin *load_pin,
                  float &elmore,
                  bool &exists) const override;
  void setElmore(const Pin *load_pin,
                 float elmore) override;
  PinSet unannotatedLoads(const Pin *drvr_pin,
                          const Parasitics *parasitics) const override;
  void deleteLoad(const Pin *load_pin);

private:
  ConcreteElmoreLoadMap loads_;
};

class ConcretePoleResidue : public ConcreteParasitic
{
public:
  ConcretePoleResidue();
  virtual ~ConcretePoleResidue();
  virtual bool isPoleResidue() const override { return true; }
  float capacitance() const override { return 0.0; }
  PinSet unannotatedLoads(const Pin *drvr_pin,
                          const Parasitics *parasitics) const override;

  void setPoleResidue(ComplexFloatSeq *poles,
                      ComplexFloatSeq *residues);
  void poleResidue(int index,
                   ComplexFloat &pole,
                   ComplexFloat &residue) const;
  size_t poleResidueCount() const;
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
  virtual bool isPiPoleResidue() const override { return true; }
  virtual bool isPiModel() const override { return true; }
  virtual float capacitance() const override;
  virtual void piModel(float &c2,
		       float &rpi,
		       float &c1) const override;
  virtual void setPiModel(float c2,
			  float rpi,
			  float c1) override;
  virtual bool isReducedParasiticNetwork() const override;
  virtual void setIsReduced(bool reduced) override;
  virtual Parasitic *findPoleResidue(const Pin *load_pin) const override;
  virtual void setPoleResidue(const Pin *load_pin,
			      ComplexFloatSeq *poles,
			      ComplexFloatSeq *residues) override;
  virtual PinSet unannotatedLoads(const Pin *drvr_pin,
                                  const Parasitics *parasitics) const override;
  void deleteLoad(const Pin *load_pin);

private:
  ConcretePoleResidueMap load_pole_residue_;
};

class ConcreteParasiticNetwork : public ParasiticNetwork,
				 public ConcreteParasitic
{
public:
  ConcreteParasiticNetwork(const Net *net,
                           bool includes_pin_caps,
                           const Network *network);
  virtual ~ConcreteParasiticNetwork();
  virtual bool isParasiticNetwork() const { return true; }
  const Net *net() const { return net_; }
  bool includesPinCaps() const { return includes_pin_caps_; }
  ConcreteParasiticNode *findParasiticNode(const Net *net,
                                           int id,
                                           const Network *network) const;
  ConcreteParasiticNode *ensureParasiticNode(const Net *net,
					     int id,
                                             const Network *network);
  ConcreteParasiticNode *findParasiticNode(const Pin *pin) const;
  ConcreteParasiticNode *ensureParasiticNode(const Pin *pin,
                                             const Network *network);
  virtual float capacitance() const;
  ParasiticNodeSeq nodes() const;
  void disconnectPin(const Pin *pin,
		     const Net *net,
                     const Network *network);
  ParasiticResistorSeq resistors() const { return resistors_; }
  void addResistor(ParasiticResistor *resistor);
  ParasiticCapacitorSeq capacitors() const { return capacitors_; }
  void addCapacitor(ParasiticCapacitor *capacitor);
  virtual PinSet unannotatedLoads(const Pin *drvr_pin,
                                  const Parasitics *parasitics) const;

private:
  void unannotatedLoads(ParasiticNode *node,
                        ParasiticResistor *from_res,
                        PinSet &loads,
                        ParasiticNodeSet &visited_nodes,
                        ParasiticResistorSet &loop_resistors,
                        ParasiticNodeResistorMap &resistor_map,
                        const Parasitics *parasitics) const;

  void deleteNodes();
  void deleteDevices();

  const Net *net_;
  ConcreteParasiticSubNodeMap sub_nodes_;
  ConcreteParasiticPinNodeMap pin_nodes_;
  ParasiticResistorSeq resistors_;
  ParasiticCapacitorSeq capacitors_;
  unsigned max_node_id_:31;
  bool includes_pin_caps_:1;
};

class ConcreteParasiticNode : public ParasiticNode
{
public:
  ConcreteParasiticNode(const Net *net,
                        int id,
                        bool is_external);
  ConcreteParasiticNode(const Pin *pin,
                        bool is_external);
  float capacitance() const { return cap_; }
  const char *name(const Network *network) const;
  const Net *net(const Network *network) const;
  unsigned id() const { return id_; }
  bool isExternal() const { return is_external_; }
  const Pin *pin() const;
  void incrCapacitance(float cap);

protected:
  ConcreteParasiticNode();

  union {
    const Net *net_;
    const Pin *pin_;
  } net_pin_;
  bool is_net_:1;
  bool is_external_:1;
  unsigned id_:30;
  float cap_;

  friend class ConcreteParasiticNetwork;
};

class ConcreteParasiticDevice
{
public:
  ConcreteParasiticDevice(size_t id,
                          float value,
                          ConcreteParasiticNode *node1,
                          ConcreteParasiticNode *node2);
  int id() const { return id_; }
  float value() const { return value_; }
  ConcreteParasiticNode *node1() const { return node1_; }
  ConcreteParasiticNode *node2() const { return node2_; }
  void replaceNode(ConcreteParasiticNode *from_node,
                   ConcreteParasiticNode *to_node);

protected:
  size_t id_;
  float value_;
  ConcreteParasiticNode *node1_;
  ConcreteParasiticNode *node2_;
};

class ConcreteParasiticResistor : public ParasiticResistor,
                                  public ConcreteParasiticDevice 
{
public:
  ConcreteParasiticResistor(size_t id,
                            float value,
                            ConcreteParasiticNode *node1,
                            ConcreteParasiticNode *node2);
};

class ConcreteParasiticCapacitor : public ParasiticCapacitor,
                                  public ConcreteParasiticDevice 
{
public:
  ConcreteParasiticCapacitor(size_t id,
                             float value,
                             ConcreteParasiticNode *node1,
                             ConcreteParasiticNode *node2);
};

} // namespace
