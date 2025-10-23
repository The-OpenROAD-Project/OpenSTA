// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#include "SpefReader.hh"

#include "Zlib.hh"
#include "Stats.hh"
#include "Report.hh"
#include "Debug.hh"
#include "StringUtil.hh"
#include "Map.hh"
#include "Transition.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "Corner.hh"
#include "ArcDelayCalc.hh"
#include "SpefReaderPvt.hh"
#include "SpefNamespace.hh"
#include "parasitics/SpefScanner.hh"

namespace sta {

using std::string;

bool
readSpefFile(const char *filename,
	     Instance *instance,
	     ParasiticAnalysisPt *ap,
	     bool pin_cap_included,
	     bool keep_coupling_caps,
	     float coupling_cap_factor,
	     bool reduce,
	     const Corner *corner,
	     const MinMaxAll *min_max,
             StaState *sta)
{
  SpefReader reader(filename, instance, ap,
                    pin_cap_included, keep_coupling_caps, coupling_cap_factor,
                    reduce, corner, min_max, sta);
  bool success = reader.read();
  return success;
}

SpefReader::SpefReader(const char *filename,
		       Instance *instance,
		       ParasiticAnalysisPt *ap,
		       bool pin_cap_included,
		       bool keep_coupling_caps,
		       float coupling_cap_factor,
		       bool reduce,
		       const Corner *corner,
		       const MinMaxAll *min_max,
		       StaState *sta) :
  StaState(sta),
  filename_(filename),
  instance_(instance),
  ap_(ap),
  pin_cap_included_(pin_cap_included),
  keep_coupling_caps_(keep_coupling_caps),
  reduce_(reduce),
  corner_(corner),
  min_max_(min_max),
  // defaults
  divider_('\0'),
  delimiter_('\0'),
  bus_brkt_left_('\0'),
  bus_brkt_right_('\0'),
  net_(nullptr),
  triple_index_(0),
  time_scale_(1.0),
  cap_scale_(1.0),
  res_scale_(1.0),
  induct_scale_(1.0),
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
}

bool
SpefReader::read()
{
  bool success;
  gzstream::igzstream stream(filename_);
  if (stream.is_open()) {
    Stats stats(debug_, report_);
    SpefScanner scanner(&stream, filename_, this, report_);
    scanner_ = &scanner;
    SpefParse parser(&scanner, this);
    //parser.set_debug_level(1);
    // yyparse returns 0 on success.
    success = (parser.parse() == 0);
    stats.report("Read spef");
  }
  else
    throw FileNotReadable(filename_);
  return success;
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
SpefReader::setBusBrackets(char left,
                           char right)
{
  if (!((left == '[' && right == ']')
	|| (left == '{' && right == '}')
	|| (left == '(' && right == ')')
	|| (left == '<' && right == '>')
	|| (left == ':' && right == '\0')
	|| (left == '.' && right == '\0')))
    warn(1640, "illegal bus delimiters.");
  bus_brkt_left_ = left;
  bus_brkt_right_ = right;
}

Instance *
SpefReader::findInstanceRelative(const char *name)
{
  return sdc_network_->findInstanceRelative(instance_, name);
}

Net *
SpefReader::findNetRelative(const char *name)
{
  Net *net = network_->findNetRelative(instance_, name);
  // Relax spef escaping requirement because some commercial tools
  // don't follow the rules.
  if (net == nullptr)
    net = sdc_network_->findNetRelative(instance_, name);
  return net;
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

char *
SpefReader::translated(const char *token)
{
  return spefToSta(token, divider_, network_->pathDivider(),
		   network_->pathEscape());
}

void
SpefReader::warn(int id, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(id, filename_, scanner_->line(), fmt, args);
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
    warn(1641, "unknown units %s.", units);
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
    warn(1642, "unknown units %s.", units);
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
    warn(1643, "unknown units %s.", units);
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
    warn(1644, "unknown units %s.", units);
  stringDelete(units);
}

void
SpefReader::makeNameMapEntry(const char *index,
			     const char *name)
{
  int i = atoi(index + 1);
  name_map_[i] = name;
  stringDelete(index);
  stringDelete(name);
}

const char *
SpefReader::nameMapLookup(const char *name)
{
  if (name && name[0] == '*') {
    int index = atoi(name + 1);
    const auto &itr = name_map_.find(index);
    if (itr != name_map_.end())
      return itr->second.c_str();
    else {
      warn(1645, "no name map entry for %d.", index);
      return nullptr;
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
    warn(1646, "unknown port direction %s.", spef_dir);
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
      const char *name1 = nameMapLookup(name);
      if (name1) {
        Instance *inst = findInstanceRelative(name1);
        // Replace delimiter for error messages.
        *delim = delimiter_;
        const char *port_name = delim + 1;
        if (inst) {
          pin = network_->findPin(inst, port_name);
          if (pin == nullptr)
            warn(1647, "pin %s not found.", name1);
        }
        else
          warn(1648, "instance %s not found.", name1);
      }
    }
    else {
      pin = findPortPinRelative(name);
      if (pin == nullptr)
	warn(1649, "pin %s not found.", name);
    }
  }
  return pin;
}

Net *
SpefReader::findNet(const char *name)
{
  Net *net = nullptr;
  const char *name1 = nameMapLookup(name);
  if (name1) {
    net = findNetRelative(name1);
    if (net == nullptr)
      warn(1650, "net %s not found.", name1);
  }
  return net;
}

void
SpefReader::rspfBegin(Net *net,
		      SpefTriple *total_cap)
{
  if (net)
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
    float c2 = pi->c2()->value(triple_index_) * cap_scale_;
    float rpi = pi->r1()->value(triple_index_) * res_scale_;
    float c1 = pi->c1()->value(triple_index_) * cap_scale_;
    // Only one parasitic, save it under rise transition.
    parasitic_ = parasitics_->makePiElmore(drvr_pin, RiseFall::rise(), ap_,
                                           c2, rpi, c1);
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
    if (network_->isTopInstance(instance_)) {
      parasitics_->deleteReducedParasitics(net, ap_);
      parasitic_ = parasitics_->makeParasiticNetwork(net, pin_cap_included_, ap_);
    }
    else {
      Net *parasitic_owner = net;
      NetTermIterator *term_iter = network_->termIterator(net);
      if (term_iter->hasNext()) {
        Term *term = term_iter->next();
        Pin *hpin = network_->pin(term);
        parasitic_owner = network_->net(hpin);
      }
      delete term_iter;
      parasitic_ = parasitics_->findParasiticNetwork(parasitic_owner, ap_);
      if (parasitic_ == nullptr)
        parasitic_ = parasitics_->makeParasiticNetwork(parasitic_owner,
                                                       pin_cap_included_, ap_);
    }
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
  if (parasitic_ && reduce_) {
    arc_delay_calc_->reduceParasitic(parasitic_, net_, corner_, min_max_);
    parasitics_->deleteParasiticNetwork(net_, ap_);
  }
  parasitic_ = nullptr;
  net_ = nullptr;
}

ParasiticNode *
SpefReader::findParasiticNode(char *name,
			      bool local_only)
{
  if (name && parasitic_) {
    char *delim = strrchr(name, delimiter_);
    if (delim) {
      *delim = '\0';
      char *name2 = delim + 1;
      const char *name1 = nameMapLookup(name);
      if (name1) {
        Instance *inst = findInstanceRelative(name1);
        if (inst) {
          // <instance>:<port>
          Pin *pin = network_->findPin(inst, name2);
          if (pin) {
            if (local_only
                && !network_->isConnected(net_, pin))
              warn(1651, "%s not connected to net %s.",
		   name1, sdc_network_->pathName(net_));
            return parasitics_->ensureParasiticNode(parasitic_, pin, network_);
          }
          else {
            // Replace delimiter for error message.
            *delim = delimiter_;
            warn(1652, "pin %s not found.", name1);
          }
        }
        else {
          Net *net = findNet(name1);
          // Replace delimiter for error messages.
          *delim = delimiter_;
          if (net) {
            // <net>:<subnode_id>
            const char *id_str = delim + 1;
            if (isDigits(id_str)) {
              int id = atoi(id_str);
              if (local_only
                  && !network_->isConnected(net, net_))
                warn(1653, "%s not connected to net %s.",
                     name1,
                     network_->pathName(net_));
              return parasitics_->ensureParasiticNode(parasitic_, net, id, network_);
            }
            else
              warn(1654, "node %s not a pin or net:number", name1);
          }
        }
      }
    }
    else {
      // <top_level_port>
      const char *name1 = nameMapLookup(name);
      if (name1) {
        Pin *pin = findPortPinRelative(name1);
        if (pin) {
          if (local_only
              && !network_->isConnected(net_, pin))
            warn(1655, "%s not connected to net %s.", name1, network_->pathName(net_));
          return parasitics_->ensureParasiticNode(parasitic_, pin, network_);
        }
        else
          warn(1656, "pin %s not found.", name1);
      }
      else
        warn(1657, "pin %s not found.", name);
    }
  }
  return nullptr;
}

void
SpefReader::makeCapacitor(int, char *node_name,
			  SpefTriple *cap)
{
  ParasiticNode *node = findParasiticNode(node_name, true);
  if (node) {
    float cap1 = cap->value(triple_index_) * cap_scale_;
    parasitics_->incrCap(node, cap1);
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
  ParasiticNode *node1 = findParasiticNode(node_name1, false);
  ParasiticNode *node2 = findParasiticNode(node_name2, false);
  float cap1 = cap->value(triple_index_) * cap_scale_;
  if (cap1 > 0.0) {
    if (keep_coupling_caps_)
      parasitics_->makeCapacitor(parasitic_, id, cap1, node1, node2);
    else {
      float scaled_cap = cap1 * ap_->couplingCapFactor();
      if (node1 && parasitics_->net(node1, network_) == net_)
        parasitics_->incrCap(node1, scaled_cap);
      if (node2 && parasitics_->net(node2, network_) == net_)
        parasitics_->incrCap(node2, scaled_cap);
    }
  }
  delete cap;
  stringDelete(node_name1);
  stringDelete(node_name2);
}

void
SpefReader::makeResistor(int id,
			 char *node_name1,
			 char *node_name2,
			 SpefTriple *res)
{
  ParasiticNode *node1 = findParasiticNode(node_name1, true);
  ParasiticNode *node2 = findParasiticNode(node_name2, true);
  if (node1 && node2) {
    float res1 = res->value(triple_index_) * res_scale_;
    parasitics_->makeResistor(parasitic_, id, res1, node1, node2);
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

////////////////////////////////////////////////////////////////

SpefScanner::SpefScanner(std::istream *stream,
                         const string &filename,
                         SpefReader *reader,
                         Report *report) :
  yyFlexLexer(stream),
  filename_(filename),
  reader_(reader),
  report_(report)
{
}

void
SpefScanner::error(const char *msg)
{
  report_->fileError(1867, filename_.c_str(), lineno(), "%s", msg);
}

} // namespace
