// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
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

#ifndef STA_SPEF_READER_PVT_H
#define STA_SPEF_READER_PVT_H

#include "Zlib.hh"
#include "Map.hh"
#include "StringSeq.hh"
#include "NetworkClass.hh"
#include "SpfSpefReader.hh"

// Global namespace.
#define YY_INPUT(buf,result,max_size) \
  sta::spef_reader->getChars(buf, result, max_size)

int
SpefParse_error(const char *msg);

////////////////////////////////////////////////////////////////

namespace sta {

class Parasitic;
class ParasiticNode;
class ParasiticNetwork;
class SpefRspfPi;
class SpefTriple;
class Corner;

typedef Map<int,char*,std::less<int> > SpefNameMap;

class SpefReader : public SpfSpefReader
{
public:
  SpefReader(const char *filename,
	     gzFile stream,
	     int line,
	     Instance *instance,
	     ParasiticAnalysisPt *ap,
	     bool increment,
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
  virtual ~SpefReader();
  void setBusBrackets(char left,
		      char right);
  void setTimeScale(float scale,
		    const char *units);
  void setCapScale(float scale,
		   const char *units);
  void setResScale(float scale,
		   const char *units);
  void setInductScale(float scale,
		      const char *units);
  void makeNameMapEntry(char *index,
			char *name);
  char *nameMapLookup(char *index);
  void setDesignFlow(StringSeq *flow_keys);
  Pin *findPin(char *name);
  Net *findNet(char *name);
  void rspfBegin(Net *net,
		 SpefTriple *total_cap);
  void rspfFinish();
  void rspfDrvrBegin(Pin *drvr_pin,
		     SpefRspfPi *pi);
  void rspfLoad(Pin *load_pin,
		SpefTriple *rc);
  void rspfDrvrFinish();
  void dspfBegin(Net *net,
		 SpefTriple *total_cap);
  void dspfFinish();
  void makeCapacitor(int id,
		     char *node_name,
		     SpefTriple *cap);
  void makeCapacitor(int id,
		     char *node_name1,
		     char *node_name2,
		     SpefTriple *cap);
  void makeResistor(int id,
		    char *node_name1,
		    char *node_name2,
		    SpefTriple *res);
  PortDirection *portDirection(char *spef_dir);

private:
  void makeCouplingCap(int id,
		       char *node_name1,
		       char *node_name2,
		       float cap);
  ParasiticNode *findParasiticNode(char *name);
  void findParasiticNode(char *name,
			 ParasiticNode *&node,
			 Net *&ext_net,
			 int &ext_node_id,
			 Pin *&ext_pin);

  int triple_index_;
  float time_scale_;
  float cap_scale_;
  float res_scale_;
  float induct_scale_;
  SpefNameMap name_map_;
  StringSeq *design_flow_;
  Parasitic *parasitic_;

  using SpfSpefReader::findPortPinRelative;
  using SpfSpefReader::findNetRelative;
  using SpfSpefReader::findInstanceRelative;
};

class SpefTriple
{
public:
  SpefTriple(float value);
  SpefTriple(float value1,
	     float value2,
	     float value3);
  float value(int index) const;
  bool isTriple() const { return is_triple_; }

private:
  float values_[3];
  bool is_triple_;
};

class SpefRspfPi
{
public:
  SpefRspfPi(SpefTriple *c2, SpefTriple *r1, SpefTriple *c1);
  ~SpefRspfPi();
  SpefTriple *c2() { return c2_; }
  SpefTriple *r1() { return r1_; }
  SpefTriple *c1() { return c1_; }

private:
  SpefTriple *c2_;
  SpefTriple *r1_;
  SpefTriple *c1_;
};

extern SpefReader *spef_reader;

} // namespace
#endif
