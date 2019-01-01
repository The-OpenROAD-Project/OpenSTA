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

#include "Machine.hh"
#include "Zlib.hh"
#include "Debug.hh"
#include "StringUtil.hh"
#include "Map.hh"
#include "Network.hh"
#include "Transition.hh"
#include "Parasitics.hh"
#include "SpfReaderPvt.hh"
#include "SpfReader.hh"

int
SpfParse_parse();
void
spfResetScanner();

namespace sta {

SpfReader *spf_reader;
const Pin *SpfReader::gnd_net_ = reinterpret_cast<Pin*>(1);
const Pin *SpfReader::rspf_subnode_ = reinterpret_cast<Pin*>(2);

bool
readSpfFile(const char *filename,
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
	    bool save,
	    bool quiet,
	    Report *report,
	    Network *network,
	    Parasitics *parasitics)
{
  SpfReader reader(filename, stream, line, rspf, instance, ap,
		   increment, pin_cap_included, keep_coupling_caps,
		   coupling_cap_factor, reduce_to, delete_after_reduce,
		   op_cond, corner, cnst_min_max, quiet,
		   report, network, parasitics);
  spf_reader = &reader;
  ::spfResetScanner();
  // yyparse returns 0 on success.
  bool success = (::SpfParse_parse() == 0);
  if (success && save)
    parasitics->save();
  return success;
}

SpfReader::SpfReader(const char *filename,
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
		     Parasitics *parasitics):
  SpfSpefReader(filename, stream, line, instance, ap, increment,
		pin_cap_included, keep_coupling_caps, coupling_cap_factor,
		reduce_to, delete_after_reduce, op_cond, corner, cnst_min_max,
		quiet, report, network, parasitics),
  is_rspf_(rspf),
  gnd_net_name_(NULL)
{
}

SpfReader::~SpfReader()
{
  clearPinMap();
  // gnd_net remains in the pin map, so delete it here.
  if (gnd_net_name_)
    stringDelete(gnd_net_name_);
  pin_node_map_.clear();
}

void
SpfReader::setGroundNet(const char *ground_net)
{
  gnd_net_name_ = ground_net;
  pin_node_map_[ground_net] = gnd_net_;
}

// Some SPF writers use DRIVER/LOAD stmts to define an alias for the
// pin name so make a table to map from the name to the node/pin.
void
SpfReader::rspfDrvrBegin(const char *drvr_pin_name)
{
  rspf_drvr_pin_ = findPinRelative(drvr_pin_name);
  if (rspf_drvr_pin_)
    pin_node_map_[drvr_pin_name] = rspf_drvr_pin_;
  else {
    pinNotFound(drvr_pin_name);
    stringDelete(drvr_pin_name);
  }
  rspfDrvrBegin();
}

void
SpfReader::rspfDrvrBegin(const char *drvr_pin_name,
			 const char *inst_name,
			 const char *port_name)
{
  rspf_drvr_pin_ = 0;
  parasitic_ = 0;
  Instance *inst = findInstanceRelative(inst_name);
  if (inst) {
    rspf_drvr_pin_ = network_->findPinRelative(inst, port_name);
    if (rspf_drvr_pin_)
      pin_node_map_[drvr_pin_name] = rspf_drvr_pin_;
    else {
      instPinNotFound(inst_name, port_name);
      stringDelete(drvr_pin_name);
    }
  }
  else {
    instNotFound(inst_name);
    stringDelete(drvr_pin_name);
  }
  rspfDrvrBegin();
  stringDelete(inst_name);
  stringDelete(port_name);
}

void
SpfReader::rspfDrvrBegin()
{
  rspf_c1_ = 0.0;
  rspf_c2_ = 0.0;
  rspf_rpi_ = 0.0;

  rspf_load_pin_ = 0;
  rspf_c3_ = 0.0;
  rspf_r3_ = 0.0;
}

void
SpfReader::rspfLoadBegin(const char *load_pin_name)
{
  rspf_load_pin_ = findPinRelative(load_pin_name);
  if (rspf_load_pin_)
    pin_node_map_[load_pin_name] = rspf_load_pin_;
  else {
    pinNotFound(load_pin_name);
    stringDelete(load_pin_name);
  }
}

void
SpfReader::rspfLoadBegin(const char *load_pin_name,
			 const char *inst_name,
			 const char *port_name)
{
  Instance *inst = findInstanceRelative(inst_name);
  if (inst) {
    rspf_load_pin_ = network_->findPinRelative(inst, port_name);
    if (rspf_load_pin_)
      pin_node_map_[load_pin_name] = rspf_load_pin_;
    else {
      instPinNotFound(inst_name, port_name);
      stringDelete(load_pin_name);
    }
  }
  else {
    instNotFound(inst_name);
    rspf_load_pin_ = 0;
    stringDelete(load_pin_name);
  }
  stringDelete(inst_name);
  stringDelete(port_name);
}

// Note that some SPF writers do not include subnode definitions.
void
SpfReader::subnodeDef(const char *subnode_name)
{
  if (is_rspf_)
    pin_node_map_[subnode_name] = rspf_subnode_;
  else
    stringDelete(subnode_name);
}

void
SpfReader::resistor(const char *name,
		    const char *node1,
		    const char *node2,
		    float res)
{
  if (is_rspf_) {
    if (rspf_drvr_pin_) {
      if (rspf_load_pin_ == NULL)
	rspfDrvrRes(node1, node2, res);
      else
	rspfLoadRes(node1, node2, res);
    }
    stringDelete(name);
  }
  else
    dspfResistor(name, node1, node2, res);
}

void
SpfReader::rspfDrvrRes(const char *node1,
		       const char *node2,
		       float res)
{
  const Pin *pin1 = pin_node_map_.findKey(node1);
  const Pin *pin2 = pin_node_map_.findKey(node2);
  // Ignore gnd'd resistors (r1).
  if (!((pin1 && pin1 == gnd_net_)
	|| (pin2 && pin2 == gnd_net_))) {
    if (pin1 && pin1 == rspf_drvr_pin_) {
      rspfSubnode(pin2, node2);
      rspf_rpi_ = res;
      stringDelete(node1);
    }
    else if (pin2 && pin2 == rspf_drvr_pin_) {
      rspfSubnode(pin1, node1);
      rspf_rpi_ = res;
      stringDelete(node2);
    }
    else
      warn("rspf resistor not connected to driver pin.\n");
  }
}

void
SpfReader::rspfSubnode(const Pin *subnode_pin,
		       const char *subnode_name)
{
  // Subnode does not have to be declared before use.
  if (subnode_pin != rspf_subnode_)
    // Define the driver subnode name.
    pin_node_map_[subnode_name] = rspf_subnode_;
  else
    stringDelete(subnode_name);
}

void
SpfReader::rspfLoadRes(const char *node1,
		       const char *node2,
		       float res)
{
  rspf_r3_ = res;
  stringDelete(node1);
  stringDelete(node2);
}

void
SpfReader::capacitor(const char *name,
		     const char *node1,
		     const char *node2,
		     float cap)
{
  if (is_rspf_) {
    if (rspf_drvr_pin_) {
      if (rspf_load_pin_ == NULL)
	rspfDrvrCap(node1, node2, cap);
      else
	rspfLoadCap(node1, node2, cap);
    }
    stringDelete(name);
  }
  else
    dspfCapacitor(name, node1, node2, cap);
}

void
SpfReader::rspfDrvrCap(const char *node1,
		       const char *node2,
		       float cap)
{
  const Pin *pin1 = pin_node_map_.findKey(node1);
  const Pin *pin2 = pin_node_map_.findKey(node2);
  if (pin1 && pin1 == gnd_net_) {
    rspfDrvrCap1(node2, pin2, cap);
    stringDelete(node1);
  }
  else if (pin2 && pin2 == gnd_net_) {
    rspfDrvrCap1(node1, pin1, cap);
    stringDelete(node2);
  }
  else
    warn("capacitor is not grounded.\n");
}

void
SpfReader::rspfDrvrCap1(const char *pin_name,
			const Pin *pin,
			float cap)
{
  if (pin && pin == rspf_drvr_pin_) {
    rspf_c2_ = cap;
    stringDelete(pin_name);
  }
  else {
    rspfSubnode(pin, pin_name);
    rspf_c1_ = cap;
  }
}

void
SpfReader::rspfLoadCap(const char *node1,
		       const char *node2,
		       float cap)
{
  rspf_c3_ = cap;
  stringDelete(node1);
  stringDelete(node2);
}

void
SpfReader::rspfDrvrFinish()
{
  if (rspf_drvr_pin_
      // Incremental parasitics do not overwrite existing parasitics.
      && !(increment_ &&
	   parasitics_->hasPiElmore(rspf_drvr_pin_,
				    TransRiseFall::rise(), ap_))) {
    parasitics_->deletePiElmore(rspf_drvr_pin_, TransRiseFall::rise(), ap_);
    parasitics_->deletePiElmore(rspf_drvr_pin_, TransRiseFall::fall(), ap_);
    // Only one parasitic, save it under rise transition.
    parasitic_ = parasitics_->makePiElmore(rspf_drvr_pin_,
					   TransRiseFall::rise(), ap_,
					   rspf_c2_, rspf_rpi_, rspf_c1_);
  }
  rspf_c2_ = rspf_rpi_ = rspf_c1_ = 0.0;
}

void
SpfReader::rspfLoadFinish()
{
  if (parasitic_ && rspf_load_pin_)
    parasitics_->setElmore(parasitic_, rspf_load_pin_, rspf_r3_*rspf_c3_);
  rspf_load_pin_ = 0;
  rspf_r3_ = rspf_c3_ = 0.0;
}

void
SpfReader::rspfNetFinish()
{
  rspf_drvr_pin_ = 0;
  parasitic_ = 0;
  clearPinMap();
}

void
SpfReader::clearPinMap()
{
  // Delete the pin names in the pin map.
  // Note that erasing the map elements while iterating fails on slowaris,
  // so delete all the strings, clear the map and put gnd back in.
  SpfPinMap::Iterator node_iter(pin_node_map_);
  while (node_iter.hasNext()) {
    const char *pin_name;
    const Pin *pin;
    node_iter.next(pin_name, pin);
    if (pin != gnd_net_)
      stringDelete(pin_name);
  }
  pin_node_map_.clear();
  pin_node_map_[gnd_net_name_] = gnd_net_;
}

////////////////////////////////////////////////////////////////

void
SpfReader::netBegin(const char *net_name)
{
  if (!is_rspf_) {
    net_ = findNetRelative(net_name);
    if (net_) {
      if (increment_
	  && parasitics_->hasParasiticNetwork(net_, ap_))
	// Do not overwrite existing parasitic.
	dspf_ = 0;
      else {
	parasitics_->deleteParasitics(net_, ap_);
	dspf_ = parasitics_->makeParasiticNetwork(net_,pin_cap_included_,ap_);
      }
    }
    else {
      netNotFound(net_name);
      dspf_ = 0;
    }
  }
  stringDelete(net_name);
}

void
SpfReader::dspfPinDef(const char *pin_name,
		      const char *pin_type)
{
  Pin *pin = findPortPinRelative(pin_name);
  if (pin)
    pin_node_map_[pin_name] = pin;
  else {
    pinNotFound(pin_name);
    stringDelete(pin_name);
  }
  stringDelete(pin_type);
}

void
SpfReader::dspfInstPinDef(const char *pin_name,
			  const char *inst_name,
			  const char *port_name,
			  const char *pin_type)
{
  Instance *inst = findInstanceRelative(inst_name);
  if (inst) {
    Pin *pin = network_->findPinRelative(inst, port_name);
    if (pin)
      pin_node_map_[pin_name] = pin;
    else {
      instPinNotFound(inst_name, port_name);
      stringDelete(pin_name);
    }
  }
  else {
    instNotFound(inst_name);
    stringDelete(pin_name);
  }
  stringDelete(inst_name);
  stringDelete(port_name);
  stringDelete(pin_type);
}

void
SpfReader::dspfResistor(const char *name,
			const char *node1,
			const char *node2,
			float res)
{
  if (dspf_) {
    ParasiticNode *pnode1 = ensureDspfNode(node1);
    ParasiticNode *pnode2 = ensureDspfNode(node2);
    if (!keep_device_names_) {
      stringDelete(name);
      name = 0;
    }
    if (pnode1 && pnode2)
      parasitics_->makeResistor(name, pnode1, pnode2, res, ap_);
  }
  stringDelete(node1);
  stringDelete(node2);
}

ParasiticNode *
SpfReader::ensureDspfNode(const char *node_name)
{
  const Pin *pin = pin_node_map_.findKey(node_name);
  if (pin)
    return parasitics_->ensureParasiticNode(dspf_, pin);
  else {
    const char *delim = strrchr(node_name, delimiter_);
    if (delim) {
      const char *id_str = delim + 1;
      if (isDigits(id_str)) {
	int id = atoi(id_str);
	return parasitics_->ensureParasiticNode(dspf_, net_, id);
      }
      // Fall thru.
    }
    warn("node %s is not a sub-node or external pin connection\n", node_name);
    return 0;
  }
}

void
SpfReader::dspfCapacitor(const char *name,
			 const char *node1,
			 const char *node2,
			 float cap)
{
  if (dspf_) {
    if (stringEq(node1, gnd_net_name_)) {
      ParasiticNode *pnode = ensureDspfNode(node2);
      if (pnode)
	parasitics_->incrCap(pnode, cap, ap_);
      stringDelete(name);
    }
    else if (stringEq(node2, gnd_net_name_)) {
      ParasiticNode *pnode = ensureDspfNode(node1);
      if (pnode)
	parasitics_->incrCap(pnode, cap, ap_);
      stringDelete(name);
    }
    else {
      // Coupling capacitor.
      ParasiticNode *pnode1 = ensureDspfNode(node1);
      ParasiticNode *pnode2 = ensureDspfNode(node2);
      if (keep_coupling_caps_
	  && pnode1 && pnode2) {
	if (!keep_device_names_) {
	  stringDelete(name);
	  name = 0;
	}
	parasitics_->makeCouplingCap(name, pnode1, pnode2, cap, ap_);
      }
      else {
	float scaled_cap = cap * ap_->couplingCapFactor();
	if (pnode1)
	  parasitics_->incrCap(pnode1, scaled_cap, ap_);
	if (pnode2)
	  parasitics_->incrCap(pnode2, scaled_cap, ap_);
	stringDelete(name);
      }
    }
  }
  stringDelete(node1);
  stringDelete(node2);
}

void
SpfReader::dspfNetFinish()
{
  if (dspf_) {
    if (!quiet_)
      parasitics_->check(dspf_);
    if (reduce_to_ != reduce_parasitics_to_none) {
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	parasitics_->reduceTo(dspf_, net_, reduce_to_, tr, op_cond_,
			      corner_, cnst_min_max_, ap_);
      }
      if (delete_after_reduce_)
	parasitics_->deleteParasiticNetwork(net_, ap_);
    }
  }
  clearPinMap();
  net_ = 0;
  dspf_ = 0;
}

////////////////////////////////////////////////////////////////

float
SpfReader::unitScale(char unit)
{
  switch (unit) {
  case 'K':
    return 1E+3F;
  case 'M':
    return 1E+6F;
  case 'U':
    return 1E-6F;
  case 'N':
    return 1E-9F;
  case 'P':
    return 1E-12F;
  case 'F':
    return 1E-15F;
  default:
    warn("unknown unit suffix %c.\n", unit);
  }
  return 1.0F;
}

void
SpfReader::pinNotFound(const char *pin_name)
{
  warn("pin %s not found.\n", pin_name);
}

void
SpfReader::netNotFound(const char *net_name)
{
  warn("net %s not found.\n", net_name);
}

void
SpfReader::instNotFound(const char *inst_name)
{
  warn("instance %s not found.\n", inst_name);
}

void
SpfReader::instPinNotFound(const char *inst_name,
			   const char *port_name)
{
  warn("instance %s pin %s not found.\n", inst_name, port_name);
}

} // namespace

// Global namespace

void spfFlushBuffer();

int
SpfParse_error(const char *msg)
{
  sta::spf_reader->warn("%s.\n", msg);
  spfFlushBuffer();
  return 0;
}
