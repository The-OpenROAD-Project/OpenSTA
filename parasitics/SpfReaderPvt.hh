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

#ifndef STA_SPF_READER_PVT_H
#define STA_SPF_READER_PVT_H

#include "Zlib.hh"
#include "Map.hh"
#include "SpfSpefReader.hh"

#define YY_INPUT(buf,result,max_size) \
  sta::spf_reader->getChars(buf, result, max_size)

int
SpfParse_error(const char *msg);

namespace sta {

class Parasitics;
class Parasitic;
class ParasiticNode;
class ParasiticNetwork;

typedef Map<const char *, const Pin*, CharPtrLess> SpfPinMap;

class SpfReader : public SpfSpefReader
{
public:
  SpfReader(const char *filename,
	    gzFile stream,
	    int line,
	    bool rspf,
	    Instance *instance,
	    ParasiticAnalysisPt *ap,
	    bool increment,
	    bool pin_cap_included,
	    bool keep_coupling_caps,
	    float coupling_cap_factor,
	    ReduceParasiticsTo reduce_to,
	    bool delete_after_reduce,
	    const OperatingConditions *op_cond,
	    const Corner *corner,
	    const MinMax *cnst_min_max,
	    bool quiet,
	    Report *report,
	    Network *network,
	    Parasitics *parasitics);
  virtual ~SpfReader();
  void read();
  void setGroundNet(const char *ground_net);
  void netBegin(const char *net_name);
  void rspfDrvrBegin(const char *drvr_pin_name);
  void rspfDrvrBegin(const char *drvr_pin_name,
		     const char *inst_name,
		     const char *port_name);
  void subnodeDef(const char *subnode_name);
  void resistor(const char *name,
		const char *node1,
		const char *node2,
		float res);
  void capacitor(const char *name,
		 const char *node1,
		 const char *node2,
		 float cap);
  void rspfLoadBegin(const char *load_pin_name);
  void rspfLoadBegin(const char *load_pin_name,
		     const char *inst_name,
		     const char *port_name);
  void rspfDrvrFinish();
  void rspfLoadFinish();
  void rspfNetFinish();

  void dspfPinDef(const char *pin_name,
		  const char *pin_type);
  void dspfInstPinDef(const char *pin_name,
		      const char *inst_name,
		      const char *port_name,
		      const char *pin_type);
  void dspfNetFinish();

  float unitScale(char unit);

private:
  void rspfDrvrBegin();
  void instNotFound(const char *inst_name);
  void pinNotFound(const char *pin_name);
  void netNotFound(const char *net_name);
  void instPinNotFound(const char *inst_name,
		       const char *port_name);
  void clearPinMap();
  void rspfDrvrRes(const char *node1,
		   const char *node2,
		   float res);
  void rspfSubnode(const Pin *subnode_pin,
		   const char *subnode_name);
  void rspfLoadRes(const char *node1,
		   const char *node2,
		   float res);
  void dspfResistor(const char *name,
		    const char *node1,
		    const char *node2,
		    float res);
  void rspfDrvrCap(const char *node1,
		   const char *node2,
		   float cap);
  void rspfDrvrCap1(const char *pin_name,
		    const Pin *pin,
		    float cap);
  void rspfLoadCap(const char *node1,
		   const char *node2,
		   float cap);
  void dspfCapacitor(const char *name,
		     const char *node1,
		     const char *node2,
		     float cap);
  ParasiticNode *ensureDspfNode(const char *node_name);

  bool reduce_dspf_;
  bool delete_reduced_dspf_;
  bool is_rspf_;
  Parasitic *parasitic_;
  Pin *rspf_drvr_pin_;
  Pin *rspf_load_pin_;
  SpfPinMap pin_node_map_;
  float rspf_c1_;
  float rspf_c2_;
  float rspf_rpi_;
  float rspf_c3_;
  float rspf_r3_;
  Parasitic *dspf_;
  const char *gnd_net_name_;

  // spf_gnd_net is inserted into spf_node_map to save string compares.
  static const Pin *gnd_net_;
  static const Pin *rspf_subnode_;
};

extern SpfReader *spf_reader;

} // namespace
#endif
