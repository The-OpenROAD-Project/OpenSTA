// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include <string>
#include <string_view>
#include <utility>

#include "Zlib.hh"
#include "Stats.hh"
#include "Report.hh"
#include "Debug.hh"
#include "StringUtil.hh"
#include "Transition.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "Scene.hh"
#include "ArcDelayCalc.hh"
#include "SpefReaderPvt.hh"
#include "SpefNamespace.hh"
#include "parasitics/SpefScanner.hh"

namespace sta {

bool
readSpefFile(std::string_view filename,
             Instance *instance,
             bool pin_cap_included,
             bool keep_coupling_caps,
             float coupling_cap_factor,
             bool reduce,
             const Scene *scene,
             const MinMaxAll *min_max,
             Parasitics *parasitics,
             StaState *sta)
{
  SpefReader reader(filename, instance, pin_cap_included, keep_coupling_caps,
                    coupling_cap_factor, reduce, scene, min_max, parasitics, sta);
  bool success = reader.read();
  return success;
}

SpefReader::SpefReader(std::string_view filename,
                       Instance *instance,
                       bool pin_cap_included,
                       bool keep_coupling_caps,
                       float coupling_cap_factor,
                       bool reduce,
                       const Scene *scene,
                       const MinMaxAll *min_max,
                       Parasitics *parasitics,
                       StaState *sta) :
  StaState(sta),
  filename_(filename),
  instance_(instance),
  pin_cap_included_(pin_cap_included),
  keep_coupling_caps_(keep_coupling_caps),
  coupling_cap_factor_(coupling_cap_factor),
  reduce_(reduce),
  scene_(scene),
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
  parasitics_(parasitics),
  parasitic_(nullptr)
{
  parasitics->setCouplingCapFactor(coupling_cap_factor);
}

SpefReader::~SpefReader() {}

bool
SpefReader::read()
{
  bool success;
  gzstream::igzstream stream(std::string(filename_).c_str());
  if (stream.is_open()) {
    Stats stats(debug_, report_);
    SpefScanner scanner(&stream, filename_, this, report_);
    scanner_ = &scanner;
    SpefParse parser(&scanner, this);
    // parser.set_debug_level(1);
    //  yyparse returns 0 on success.
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
  if (!((left == '[' && right == ']') || (left == '{' && right == '}')
        || (left == '(' && right == ')') || (left == '<' && right == '>')
        || (left == ':' && right == '\0') || (left == '.' && right == '\0')))
    warn(1640, "illegal bus delimiters.");
  bus_brkt_left_ = left;
  bus_brkt_right_ = right;
}

Instance *
SpefReader::findInstanceRelative(std::string_view name)
{
  return sdc_network_->findInstanceRelative(instance_, name);
}

Net *
SpefReader::findNetRelative(std::string_view name)
{
  Net *net = network_->findNetRelative(instance_, name);
  // Relax spef escaping requirement because some commercial tools
  // don't follow the rules.
  if (net == nullptr)
    net = sdc_network_->findNetRelative(instance_, name);
  return net;
}

Pin *
SpefReader::findPinRelative(std::string_view name)
{
  return network_->findPinRelative(instance_, name);
}

Pin *
SpefReader::findPortPinRelative(std::string_view name)
{
  return network_->findPin(instance_, name);
}

std::string
SpefReader::translated(std::string_view spef_name)
{
  return spefToSta(spef_name, divider_, network_->pathDivider(),
                   network_->pathEscape());
}

int
SpefReader::warnLine() const
{
  return scanner_->line();
}


void
SpefReader::setTimeScale(float scale,
                         std::string_view units)
{
  if (stringEqual(units, "NS"))
    time_scale_ = scale * 1E-9F;
  else if (stringEqual(units, "PS"))
    time_scale_ = scale * 1E-12F;
  else
    warn(1641, "unknown units {}.", units);
}

void
SpefReader::setCapScale(float scale,
                        std::string_view units)
{
  if (stringEqual(units, "PF"))
    cap_scale_ = scale * 1E-12F;
  else if (stringEqual(units, "FF"))
    cap_scale_ = scale * 1E-15F;
  else
    warn(1642, "unknown units {}.", units);
}

void
SpefReader::setResScale(float scale,
                        std::string_view units)
{
  if (stringEqual(units, "OHM"))
    res_scale_ = scale;
  else if (stringEqual(units, "KOHM"))
    res_scale_ = scale * 1E+3F;
  else
    warn(1643, "unknown units {}.", units);
}

void
SpefReader::setInductScale(float scale,
                           std::string_view units)
{
  if (stringEqual(units, "HENRY"))
    induct_scale_ = scale;
  else if (stringEqual(units, "MH"))
    induct_scale_ = scale * 1E-3F;
  else if (stringEqual(units, "UH"))
    induct_scale_ = scale * 1E-6F;
  else
    warn(1644, "unknown units {}.", units);
}

void
SpefReader::makeNameMapEntry(std::string_view index,
                             std::string_view name)
{
  int i = std::stoi(std::string(index.substr(1)));
  name_map_[i] = name;
}

std::string_view
SpefReader::nameMapLookup(std::string_view name)
{
  if (!name.empty() && name[0] == '*') {
    std::string index_str(name.substr(1));
    int index = std::stoi(index_str);
    const auto &itr = name_map_.find(index);
    if (itr != name_map_.end())
      return itr->second;
    else {
      warn(1645, "no name map entry for {}.", index);
      return "";
    }
  }
  else
    return name;
}

PortDirection *
SpefReader::portDirection(std::string_view spef_dir)
{
  PortDirection *direction = PortDirection::unknown();
  if (spef_dir == "I")
    direction = PortDirection::input();
  else if (spef_dir == "O")
    direction = PortDirection::output();
  else if (spef_dir == "B")
    direction = PortDirection::bidirect();
  else
    warn(1646, "unknown port direction {}.", spef_dir);
  return direction;
}

void
SpefReader::setDesignFlow(StringSeq *flow)
{
  design_flow_ = *flow;
  delete flow;
}

Pin *
SpefReader::findPin(std::string_view name)
{
  Pin *pin = nullptr;
  if (!name.empty()) {
    size_t delim = name.rfind(delimiter_);
    if (delim != std::string::npos) {
      std::string inst_name_mapped(name.substr(0, delim));
      std::string_view inst_name = nameMapLookup(inst_name_mapped);
      if (!inst_name.empty()) {
        Instance *inst = findInstanceRelative(inst_name);
        std::string port_name(name.substr(delim + 1, std::string::npos));
        if (inst) {
          pin = network_->findPin(inst, port_name);
          if (pin == nullptr)
            warn(1647, "pin {}{}{} not found.",
                 inst_name, delimiter_, port_name);
        }
        else
          warn(1648, "instance {}{}{} not found.",
               inst_name, delimiter_, port_name);
      }
    }
    else {
      pin = findPortPinRelative(name);
      if (pin == nullptr)
        warn(1649, "pin {} not found.", name);
    }
  }
  return pin;
}

Net *
SpefReader::findNet(std::string_view name)
{
  Net *net = nullptr;
  std::string_view name1 = nameMapLookup(name);
  if (!name1.empty()) {
    net = findNetRelative(name1);
    if (net == nullptr)
      warn(1650, "net {} not found.", name1);
  }
  return net;
}

void
SpefReader::rspfBegin(Net *net,
                      SpefTriple *total_cap)
{
  if (net)
    parasitics_->deleteParasitics(net);
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
    parasitic_ = parasitics_->makePiElmore(drvr_pin, RiseFall::rise(), MinMax::max(),
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
      parasitics_->deleteReducedParasitics(net);
      parasitic_ = parasitics_->makeParasiticNetwork(net, pin_cap_included_);
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
      parasitic_ = parasitics_->findParasiticNetwork(parasitic_owner);
      if (parasitic_ == nullptr)
        parasitic_ =
            parasitics_->makeParasiticNetwork(parasitic_owner, pin_cap_included_);
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
    arc_delay_calc_->reduceParasitic(parasitic_, net_, scene_, min_max_);
    parasitics_->deleteParasiticNetwork(net_);
  }
  parasitic_ = nullptr;
  net_ = nullptr;
}

ParasiticNode *
SpefReader::findParasiticNode(std::string_view name,
                              bool local_only)
{
  if (!name.empty() && parasitic_) {
    size_t delim = name.rfind(delimiter_);
    if (delim != std::string::npos) {
      std::string name1_mapped(name.substr(0, delim));
      std::string name2(name.substr(delim + 1, std::string::npos));
      std::string_view name1 = nameMapLookup(name1_mapped);
      if (!name1.empty()) {
        Instance *inst = findInstanceRelative(name1);
        if (inst) {
          // <instance>:<port>
          Pin *pin = network_->findPin(inst, name2);
          if (pin) {
            if (local_only && !network_->isConnected(net_, pin))
              warn(1651, "{} not connected to net {}.", name1,
                   sdc_network_->pathName(net_));
            return parasitics_->ensureParasiticNode(parasitic_, pin, network_);
          }
          else
            warn(1652, "pin {}{}{} not found.",
                 name1, delimiter_, name2);
        }
        else {
          Net *net = findNet(name1);
          if (net) {
            // <net>:<subnode_id>
            if (isDigits(name2)) {
              int id = std::stoi(name2);
              if (local_only && !network_->isConnected(net, net_))
                warn(1653, "{}{}{} not connected to net {}.",
                     name1, delimiter_, name2, network_->pathName(net_));
              return parasitics_->ensureParasiticNode(parasitic_, net, id, network_);
            }
            else
              warn(1654, "node {}{}{} not a pin or net:number",
                   name1, delimiter_, name2);
          }
        }
      }
    }
    else {
      // <top_level_port>
      std::string_view name1 = nameMapLookup(name);
      if (!name1.empty()) {
        Pin *pin = findPortPinRelative(name1);
        if (pin) {
          if (local_only && !network_->isConnected(net_, pin))
            warn(1655, "{} not connected to net {}.", name1,
                 network_->pathName(net_));
          return parasitics_->ensureParasiticNode(parasitic_, pin, network_);
        }
        else
          warn(1656, "pin {} not found.", name1);
      }
      else
        warn(1657, "pin {} not found.", name);
    }
  }
  return nullptr;
}

void
SpefReader::makeCapacitor(int,
                          std::string_view node_name,
                          SpefTriple *cap)
{
  ParasiticNode *node = findParasiticNode(node_name, true);
  if (node) {
    float cap1 = cap->value(triple_index_) * cap_scale_;
    parasitics_->incrCap(node, cap1);
  }
  delete cap;
}

void
SpefReader::makeCapacitor(int id,
                          std::string_view node_name1,
                          std::string_view node_name2,
                          SpefTriple *cap)
{
  ParasiticNode *node1 = findParasiticNode(node_name1, false);
  ParasiticNode *node2 = findParasiticNode(node_name2, false);
  float cap1 = cap->value(triple_index_) * cap_scale_;
  if (cap1 > 0.0) {
    if (keep_coupling_caps_)
      parasitics_->makeCapacitor(parasitic_, id, cap1, node1, node2);
    else {
      float scaled_cap = cap1 * coupling_cap_factor_;
      if (node1 && parasitics_->net(node1, network_) == net_)
        parasitics_->incrCap(node1, scaled_cap);
      if (node2 && parasitics_->net(node2, network_) == net_)
        parasitics_->incrCap(node2, scaled_cap);
    }
  }
  delete cap;
}

void
SpefReader::makeResistor(int id,
                         std::string_view node_name1,
                         std::string_view node_name2,
                         SpefTriple *res)
{
  ParasiticNode *node1 = findParasiticNode(node_name1, true);
  ParasiticNode *node2 = findParasiticNode(node_name2, true);
  if (node1 && node2) {
    float res1 = res->value(triple_index_) * res_scale_;
    parasitics_->makeResistor(parasitic_, id, res1, node1, node2);
  }
  delete res;
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
                         std::string_view filename,
                         SpefReader *reader,
                         Report *report) :
  yyFlexLexer(stream),
  filename_(filename),
  reader_(reader),
  report_(report)
{
}

void
SpefScanner::error(std::string_view msg)
{
  report_->fileError(1658, filename_, lineno(), "{}", msg);
}

}  // namespace sta
