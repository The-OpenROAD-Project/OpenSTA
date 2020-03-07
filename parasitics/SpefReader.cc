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

#include <limits>
#include "Machine.hh"
#include "Zlib.hh"
#include "Report.hh"
#include "Debug.hh"
#include "StringUtil.hh"
#include "Map.hh"
#include "PortDirection.hh"
#include "Transition.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "SpefReaderPvt.hh"
#include "SpefNamespace.hh"
#include "SpefReader.hh"

int
SpefParse_parse();
void
spefResetScanner();

namespace sta {

SpefReader *spef_reader;

bool
readSpefFile(const char *filename,
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
  bool success = false;
  // Use zlib to uncompress gzip'd files automagically.
  gzFile stream = gzopen(filename, "rb");
  if (stream) {
    SpefReader reader(filename, stream, instance, ap, increment,
		      pin_cap_included, keep_coupling_caps, coupling_cap_factor,
		      reduce_to, delete_after_reduce, op_cond, corner, 
		      cnst_min_max, quiet, report, network, parasitics);
    spef_reader = &reader;
    ::spefResetScanner();
    // yyparse returns 0 on success.
    success = (::SpefParse_parse() == 0);
    gzclose(stream);
  }
  else
    throw FileNotReadable(filename);
  if (success && save)
    parasitics->save();
  return success;
}

SpefReader::SpefReader(const char *filename,
		       gzFile stream,
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
		       Parasitics *parasitics) :
  filename_(filename),
  instance_(instance),
  ap_(ap),
  increment_(increment),
  pin_cap_included_(pin_cap_included),
  keep_coupling_caps_(keep_coupling_caps),
  reduce_to_(reduce_to),
  delete_after_reduce_(delete_after_reduce),
  op_cond_(op_cond),
  corner_(corner),
  cnst_min_max_(cnst_min_max),
  keep_device_names_(false),
  quiet_(quiet),
  stream_(stream),
  line_(1),
  // defaults
  divider_('\0'),
  delimiter_('\0'),
  bus_brkt_left_('\0'),
  bus_brkt_right_('\0'),
  net_(nullptr),
  report_(report),
  network_(network),
  parasitics_(parasitics),
  triple_index_(0),
  design_flow_(nullptr),
  parasitic_(nullptr)
{
  ap->setCouplingCapFactor(coupling_cap_factor);
}

SpefReader::~SpefReader()
{
  if (design_flow_) {
    deleteContents(design_flow_);
    delete design_flow_;
    design_flow_ = nullptr;
  }

  SpefNameMap::Iterator map_iter(name_map_);
  while (map_iter.hasNext()) {
    int index;
    char *name;
    map_iter.next(index, name);
    stringDelete(name);
  }
}

void
SpefReader::setDivider(char divider)
{
  divider_ = divider;
}

void
SpefReader::setDelimiter(char delimiter)
{
  delimiter_ = delimiter;
}

void
SpefReader::setBusBrackets(char left, char right)
{
  if (!((left == '[' && right == ']')
	|| (left == '{' && right == '}')
	|| (left == '(' && right == ')')
	|| (left == '<' && right == '>')
	|| (left == ':' && right == '\0')
	|| (left == '.' && right == '\0')))
    warn("illegal bus delimiters.\n");
  bus_brkt_left_ = left;
  bus_brkt_right_ = right;
}

Instance *
SpefReader::findInstanceRelative(const char *name)
{
  return network_->findInstanceRelative(instance_, name);
}

Net *
SpefReader::findNetRelative(const char *name)
{
  return network_->findNetRelative(instance_, name);
}

Pin *
SpefReader::findPinRelative(const char *name)
{
  return network_->findPinRelative(instance_, name);
}

Pin *
SpefReader::findPortPinRelative(const char *name)
{
  return network_->findPin(instance_, name);
}

void
SpefReader::getChars(char *buf,
		     int &result,
		     size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_nullptr
  else
    result = static_cast<int>(strlen(buf));
}

void
SpefReader::getChars(char *buf,
		     size_t &result,
		     size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_nullptr
  else
    result = strlen(buf);
}

char *
SpefReader::translated(const char *token)
{
  return spefToSta(token, divider_, network_->pathDivider(),
		   network_->pathEscape());
}

void
SpefReader::incrLine()
{
  line_++;
}

void
SpefReader::warn(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(filename_, line_, fmt, args);
  va_end(args);
}

void
SpefReader::setTimeScale(float scale,
			 const char *units)
{
  if (stringEq(units, "NS"))
    time_scale_ = scale * 1E-9F;
  else if (stringEq(units, "PS"))
    time_scale_ = scale * 1E-12F;
  else
    warn("unknown units %s.\n", units);
  stringDelete(units);
}

void
SpefReader::setCapScale(float scale,
			const char *units)
{
  if (stringEq(units, "PF"))
    cap_scale_ = scale * 1E-12F;
  else if (stringEq(units, "FF"))
    cap_scale_ = scale * 1E-15F;
  else
    warn("unknown units %s.\n", units);
  stringDelete(units);
}

void
SpefReader::setResScale(float scale,
			const char *units)
{
  if (stringEq(units, "OHM"))
    res_scale_ = scale;
  else if (stringEq(units, "KOHM"))
    res_scale_ = scale * 1E+3F;
  else
    warn("unknown units %s.\n", units);
  stringDelete(units);
}

void
SpefReader::setInductScale(float scale,
			   const char *units)
{
  if (stringEq(units, "HENRY"))
    induct_scale_ = scale;
  else if (stringEq(units, "MH"))
    induct_scale_ = scale * 1E-3F;
  else if (stringEq(units, "UH"))
    induct_scale_ = scale * 1E-6F;
  else
    warn("unknown units %s.\n", units);
  stringDelete(units);
}

void
SpefReader::makeNameMapEntry(char *index,
			     char *name)
{
  int i = atoi(index + 1);
  name_map_[i] = name;
}

char *
SpefReader::nameMapLookup(char *name)
{
  if (name && name[0] == '*') {
    char *mapped_name;
    bool exists;
    int index = atoi(name + 1);
    name_map_.findKey(index, mapped_name, exists);
    if (exists)
      return mapped_name;
    else {
      warn("no name map entry for %d.\n", index);
      return 0;
    }
  }
  else
    return name;
}

PortDirection *
SpefReader::portDirection(char *spef_dir)
{
  PortDirection *direction = PortDirection::unknown();
  if (stringEq(spef_dir, "I"))
    direction = PortDirection::input();
  else if (stringEq(spef_dir, "O"))
    direction = PortDirection::output();
  else if (stringEq(spef_dir, "B"))
    direction = PortDirection::bidirect();
  else
    warn("unknown port direction %s.\n", spef_dir);
  return direction;
}

void
SpefReader::setDesignFlow(StringSeq *flow)
{
  design_flow_ = flow;
}

Pin *
SpefReader::findPin(char *name)
{
  Pin *pin = nullptr;
  if (name) {
    char *delim = strrchr(name, delimiter_);
    if (delim) {
      *delim = '\0';
      name = nameMapLookup(name);
      Instance *inst = findInstanceRelative(name);
      // Replace delimiter for error messages.
      *delim = delimiter_;
      const char *port_name = delim + 1;
      if (inst) {
	pin = network_->findPin(inst, port_name);
	if (pin == nullptr)
	  warn("pin %s not found.\n", name);
      }
      else
	warn("instance %s not found.\n", name);
    }
    else {
      pin = findPortPinRelative(name);
      if (pin == nullptr)
	warn("pin %s not found.\n", name);
    }
  }
  return pin;
}

Net *
SpefReader::findNet(char *name)
{
  Net *net = nullptr;
  name = nameMapLookup(name);
  if (name) {
    net = findNetRelative(name);
    if (net == nullptr)
      warn("net %s not found.\n", name);
  }
  return net;
}

void
SpefReader::rspfBegin(Net *net,
		      SpefTriple *total_cap)
{
  if (net && !increment_)
    parasitics_->deleteParasitics(net, ap_);
  // Net total capacitance is ignored.
  delete total_cap;
}

void
SpefReader::rspfFinish()
{
}

void
SpefReader::rspfDrvrBegin(Pin *drvr_pin,
			  SpefRspfPi *pi)
{
  if (drvr_pin) {
    // Incremental parasitics do not overwrite existing parasitics.
    if (!(increment_ &&
	  parasitics_->findPiElmore(drvr_pin, RiseFall::rise(), ap_))) {
      float c2 = pi->c2()->value(triple_index_) * cap_scale_;
      float rpi = pi->r1()->value(triple_index_) * res_scale_;
      float c1 = pi->c1()->value(triple_index_) * cap_scale_;
      // Delete pi model and elmore delays.
      parasitics_->deleteParasitics(drvr_pin, ap_);
      // Only one parasitic, save it under rise transition.
      parasitic_ = parasitics_->makePiElmore(drvr_pin,
					     RiseFall::rise(),
					     ap_, c2, rpi, c1);
    }
  }
  delete pi;
}

void
SpefReader::rspfLoad(Pin *load_pin,
		     SpefTriple *rc)
{
  if (parasitic_ && load_pin) {
    float elmore = rc->value(triple_index_) * time_scale_;
    parasitics_->setElmore(parasitic_, load_pin, elmore);
  }
  delete rc;
}

void
SpefReader::rspfDrvrFinish()
{
  parasitic_ = nullptr;
}

// Net cap (total_cap) is ignored.
void
SpefReader::dspfBegin(Net *net,
		      SpefTriple *total_cap)
{
  if (net) {
    // Incremental parasitics do not overwrite existing parasitics.
    if (increment_
	&& parasitics_->findParasiticNetwork(net, ap_))
      parasitic_ = nullptr;
    else
      parasitic_ = parasitics_->makeParasiticNetwork(net, pin_cap_included_,
						     ap_);
    net_ = net;
  }
  else {
    parasitic_ = nullptr;
    net_ = nullptr;
  }
  delete total_cap;
}

void
SpefReader::dspfFinish()
{
  if (parasitic_) {
    // Checking "should" be done by report_annotated_parasitics.
    if (!quiet_)
      parasitics_->check(parasitic_);
    if (reduce_to_ != ReduceParasiticsTo::none) {
      parasitics_->reduceTo(parasitic_, net_, reduce_to_, op_cond_,
			    corner_, cnst_min_max_, ap_);
      if (delete_after_reduce_)
	parasitics_->deleteParasiticNetwork(net_, ap_);
    }
  }
  parasitic_ = nullptr;
  net_ = nullptr;
}

// Caller is only interested in nodes on net_.
ParasiticNode *
SpefReader::findParasiticNode(char *name)
{
  ParasiticNode *node;
  Net *ext_net;
  int ext_node_id;
  Pin *ext_pin;
  findParasiticNode(name, node, ext_net, ext_node_id, ext_pin);
  if (node == nullptr
      && (ext_net || ext_pin))
    warn("%s not connected to net %s.\n", name, network_->pathName(net_));
  return node;
}

void
SpefReader::findParasiticNode(char *name,
			      ParasiticNode *&node,
			      Net *&ext_net,
			      int &ext_node_id,
			      Pin *&ext_pin)
{
  node = nullptr;
  ext_net = nullptr;
  ext_node_id = 0;
  ext_pin = nullptr;
  if (name) {
    if (parasitic_) {
      char *delim = strrchr(name, delimiter_);
      if (delim) {
	*delim = '\0';
	char *name2 = delim + 1;
	name = nameMapLookup(name);
	Instance *inst = findInstanceRelative(name);
	if (inst) {
	  // <instance>:<port>
	  Pin *pin = network_->findPin(inst, name2);
	  if (pin) {
	    if (network_->isConnected(net_, pin))
	      node = parasitics_->ensureParasiticNode(parasitic_, pin);
	    else
	      ext_pin = pin;
	  }
	  else {
	    // Replace delimiter for error message.
	    *delim = delimiter_;
	    warn("pin %s not found.\n", name);
	  }
	}
	else {
	  Net *net = findNet(name);
	  // Replace delimiter for error messages.
	  *delim = delimiter_;
	  if (net) {
	    // <net>:<subnode_id>
	    const char *id_str = delim + 1;
	    if (isDigits(id_str)) {
	      int id = atoi(id_str);
	      if (network_->isConnected(net, net_))
		node = parasitics_->ensureParasiticNode(parasitic_, net, id);
	      else {
		ext_net = net;
		ext_node_id = id;
	      }
	    }
	    else
	      warn("node %s not a pin or net:number\n", name);
	  }
	}
      }
      else {
	// <top_level_port>
	name = nameMapLookup(name);
	Pin *pin = findPortPinRelative(name);
	if (pin) {
	  if (network_->isConnected(net_, pin))
	    node = parasitics_->ensureParasiticNode(parasitic_, pin);
	  else
	    ext_pin = pin;
	}
	else
	  warn("pin %s not found.\n", name);
      }
    }
  }
}

void
SpefReader::makeCapacitor(int, char *node_name,
			  SpefTriple *cap)
{
  ParasiticNode *node = findParasiticNode(node_name);
  if (node) {
    float cap1 = cap->value(triple_index_) * cap_scale_;
    parasitics_->incrCap(node, cap1, ap_);
  }
  delete cap;
  stringDelete(node_name);
}

void
SpefReader::makeCapacitor(int id,
			  char *node_name1,
			  char *node_name2,
			  SpefTriple *cap)
{
  float cap1 = cap->value(triple_index_) * cap_scale_;
  if (keep_coupling_caps_)
    makeCouplingCap(id, node_name1, node_name2, cap1);
  else {
    ParasiticNode *node1, *node2;
    Net *ext_net1, *ext_net2;
    int ext_node_id1, ext_node_id2;
    Pin *ext_pin1, *ext_pin2;
    findParasiticNode(node_name1, node1, ext_net1, ext_node_id1, ext_pin1);
    findParasiticNode(node_name2, node2, ext_net2, ext_node_id2, ext_pin2);
    float scaled_cap = cap1 * ap_->couplingCapFactor();
    if (node1)
      parasitics_->incrCap(node1, scaled_cap, ap_);
    if (node2)
      parasitics_->incrCap(node2, scaled_cap, ap_);
  }
  delete cap;
  stringDelete(node_name1);
  stringDelete(node_name2);
}

void
SpefReader::makeCouplingCap(int id,
			    char *node_name1,
			    char *node_name2,
			    float cap)
{
  const char *name = nullptr;
  const char *name_tmp = nullptr;
  if (keep_device_names_)
    // Prepend device type because OA uses one namespace for all devices.
    name = name_tmp = stringPrint("C%d", id);

  ParasiticNode *node1, *node2;
  Net *ext_net1, *ext_net2;
  int ext_node_id1, ext_node_id2;
  Pin *ext_pin1, *ext_pin2;
  findParasiticNode(node_name1, node1, ext_net1, ext_node_id1, ext_pin1);
  findParasiticNode(node_name2, node2, ext_net2, ext_node_id2, ext_pin2);
  if (node1 && node2)
    parasitics_->makeCouplingCap(name, node1, node2, cap, ap_);
  if (node1 && node2 == nullptr) {
    if (ext_net2)
      parasitics_->makeCouplingCap(name, node1, ext_net2, ext_node_id2,
				   cap, ap_);
    else if (ext_pin2)
      parasitics_->makeCouplingCap(name, node1, ext_pin2, cap, ap_);
  }
  else if (node1 == nullptr && node2) {
    if (ext_net1)
      parasitics_->makeCouplingCap(name, node2, ext_net1, ext_node_id1,
				   cap, ap_);
    else if (ext_pin1)
      parasitics_->makeCouplingCap(name, node2, ext_pin1, cap, ap_);
  }
  stringDelete(name_tmp);
}

void
SpefReader::makeResistor(int id,
			 char *node_name1,
			 char *node_name2,
			 SpefTriple *res)
{
  ParasiticNode *node1 = findParasiticNode(node_name1);
  ParasiticNode *node2 = findParasiticNode(node_name2);
  if (node1 && node2) {
    float res1 = res->value(triple_index_) * res_scale_;
    const char *name = nullptr;
    const char *name_tmp = nullptr;
    if (keep_device_names_)
      // Prepend device type because OA uses one namespace for all devices.
      name = name_tmp = stringPrint("R%d", id);
    parasitics_->makeResistor(name, node1, node2, res1, ap_);
    stringDelete(name_tmp);
  }
  delete res;
  stringDelete(node_name1);
  stringDelete(node_name2);
}

////////////////////////////////////////////////////////////////

SpefRspfPi::SpefRspfPi(SpefTriple *c2,
		       SpefTriple *r1,
		       SpefTriple *c1) :
  c2_(c2),
  r1_(r1),
  c1_(c1)
{
}

SpefRspfPi::~SpefRspfPi()
{
  delete c2_;
  delete r1_;
  delete c1_;
}

////////////////////////////////////////////////////////////////

SpefTriple::SpefTriple(float value) :
  is_triple_(false)
{
  values_[0] = value;
}

SpefTriple::SpefTriple(float value1,
		       float value2,
		       float value3) :
  is_triple_(true)
{
  values_[0] = value1;
  values_[1] = value2;
  values_[2] = value3;
}

float
SpefTriple::value(int index) const
{
  if (is_triple_)
    return values_[index];
  else
    return values_[0];
}

} // namespace

////////////////////////////////////////////////////////////////
// Global namespace

void spefFlushBuffer();

int
SpefParse_error(const char *msg)
{
  sta::spef_reader->warn("%s.\n", msg);
  spefFlushBuffer();
  return 0;
}
