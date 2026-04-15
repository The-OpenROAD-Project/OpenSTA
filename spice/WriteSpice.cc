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

#include "spice/WriteSpice.hh"

#include <algorithm>  // swap
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "Bdd.hh"
#include "Clock.hh"
#include "ContainerHelpers.hh"
#include "Debug.hh"
#include "FuncExpr.hh"
#include "Graph.hh"
#include "Liberty.hh"
#include "Mode.hh"
#include "Network.hh"
#include "Path.hh"
#include "PortDirection.hh"
#include "Sdc.hh"
#include "Sequential.hh"
#include "TableModel.hh"
#include "TimingArc.hh"
#include "TimingRole.hh"
#include "Units.hh"
#include "cudd.h"
#include "search/Sim.hh"

namespace sta {

Net *
pinNet(const Pin *pin,
       const Network *network);

WriteSpice::WriteSpice(std::string_view spice_filename,
                       std::string_view subckt_filename,
                       std::string_view lib_subckt_filename,
                       std::string_view model_filename,
                       std::string_view power_name,
                       std::string_view gnd_name,
                       CircuitSim ckt_sim,
                       const Scene *scene,
                       const MinMax *min_max,
                       const StaState *sta) :
  StaState(sta),
  spice_filename_(spice_filename),
  subckt_filename_(subckt_filename),
  lib_subckt_filename_(lib_subckt_filename),
  model_filename_(model_filename),
  power_name_(power_name),
  gnd_name_(gnd_name),
  ckt_sim_(ckt_sim),
  scene_(scene),
  min_max_(min_max),
  default_library_(network_->defaultLibertyLibrary()),
  bdd_(sta),
  parasitics_(scene->parasitics(min_max))
{
}

void
WriteSpice::initPowerGnd()
{
  bool exists = false;
  default_library_->supplyVoltage(power_name_, power_voltage_, exists);
  if (!exists) {
    const OperatingConditions *op_cond =
        scene_->sdc()->operatingConditions(min_max_);
    if (op_cond == nullptr)
      op_cond = network_->defaultLibertyLibrary()->defaultOperatingConditions();
    power_voltage_ = op_cond->voltage();
  }
  default_library_->supplyVoltage(gnd_name_, gnd_voltage_, exists);
  if (!exists)
    gnd_voltage_ = 0.0;
}

void
WriteSpice::writeHeader(std::string &title,
                        float max_time,
                        float time_step)
{
  sta::print(spice_stream_, "* {}\n", title);
  sta::print(spice_stream_, ".include \"{}\"\n", model_filename_);
  std::filesystem::path subckt_filename =
      std::filesystem::path(subckt_filename_).filename();
  sta::print(spice_stream_, ".include \"{}\"\n", subckt_filename.string());
  sta::print(spice_stream_, ".tran {:.3g} {:.3g}\n", time_step, max_time);
  // Suppress printing model parameters.
  if (ckt_sim_ == CircuitSim::hspice)
    sta::print(spice_stream_, ".options nomod\n");
  sta::print(spice_stream_, "\n");
  max_time_ = max_time;
}

void
WriteSpice::writePrintStmt(StringSeq &node_names)
{
  sta::print(spice_stream_, ".print tran");
  if (ckt_sim_ == CircuitSim::xyce) {
    std::string csv_filename = replaceFileExt(spice_filename_, "csv");
    sta::print(spice_stream_, " format=csv file={}", csv_filename);
    writeGnuplotFile(node_names);
  }
  for (std::string &name : node_names)
    sta::print(spice_stream_, " v({})", name);
  sta::print(spice_stream_, "\n\n");
}

std::string
WriteSpice::replaceFileExt(std::string_view filename,
                           std::string_view ext)
{
  size_t dot = filename.rfind('.');
  std::string ext_filename(filename.substr(0, dot + 1));
  ext_filename += ext;
  return ext_filename;
}

// Write gnuplot command file for use with xyce csv file.
void
WriteSpice::writeGnuplotFile(StringSeq &node_nanes)
{
  std::string gnuplot_filename = replaceFileExt(spice_filename_, "gnuplot");
  std::string csv_filename = replaceFileExt(spice_filename_, "csv");
  std::ofstream gnuplot_stream;
  gnuplot_stream.open(gnuplot_filename);
  if (gnuplot_stream.is_open()) {
    sta::print(gnuplot_stream, "set datafile separator ','\n");
    sta::print(gnuplot_stream, "set key autotitle columnhead\n");
    sta::print(gnuplot_stream, "plot\\\n");
    sta::print(gnuplot_stream, "\"{}\" using 1:2 with lines", csv_filename);
    for (size_t i = 3; i <= node_nanes.size() + 1; i++) {
      sta::print(gnuplot_stream, ",\\\n");
      sta::print(gnuplot_stream, "'' using 1:{} with lines", i);
    }
    sta::print(gnuplot_stream, "\n");
    sta::print(gnuplot_stream, "pause mouse close\n");
    gnuplot_stream.close();
  }
}

void
WriteSpice::writeSubckts(StringSet &cell_names)
{
  findCellSubckts(cell_names);
  std::ifstream lib_subckts_stream{std::string(lib_subckt_filename_)};
  if (lib_subckts_stream.is_open()) {
    std::ofstream subckts_stream{std::string(subckt_filename_)};
    if (subckts_stream.is_open()) {
      std::string line;
      int line_num = 0;
      while (std::getline(lib_subckts_stream, line)) {
        line_num++;
        // .subckt <cell_name> [args..]
        StringSeq tokens = parseTokens(line);
        if (tokens.size() >= 2 && stringEqual(tokens[0], ".subckt")) {
          const std::string &cell_name = tokens[1];
          if (cell_names.contains(cell_name)) {
            subckts_stream << line << "\n";
            bool found_ends = false;
            while (std::getline(lib_subckts_stream, line)) {
              subckts_stream << line << "\n";
              if (stringBeginEqual(line, ".ends")) {
                subckts_stream << "\n";
                found_ends = true;
                break;
              }
            }
            if (!found_ends)
              report_->fileError(1606, lib_subckt_filename_, line_num,
                                 "spice subckt for cell {} missing .ends.",
                                 cell_name);
            cell_names.erase(cell_name);
          }
          recordSpicePortNames(cell_name, tokens);
        }
      }
      subckts_stream.close();
      lib_subckts_stream.close();

      if (!cell_names.empty()) {
        std::string missing_cells;
        for (const std::string &cell_name : cell_names) {
          missing_cells += "\n";
          missing_cells += cell_name;
        }
        report_->error(1605, "The subkct file {} is missing definitions for {}",
                       lib_subckt_filename_, missing_cells);
      }
    }
    else {
      lib_subckts_stream.close();
      throw FileNotWritable(subckt_filename_);
    }
  }
  else
    throw FileNotReadable(lib_subckt_filename_);
}

void
WriteSpice::recordSpicePortNames(std::string_view cell_name,
                                 StringSeq &tokens)
{
  LibertyCell *cell = network_->findLibertyCell(cell_name);
  if (cell) {
    StringSeq &spice_port_names = cell_spice_port_names_[std::string(cell_name)];
    for (size_t i = 2; i < tokens.size(); i++) {
      const std::string &port_name = tokens[i];
      LibertyPort *port = cell->findLibertyPort(port_name);
      if (port == nullptr
          && port_name != power_name_
          && port_name != gnd_name_)
        report_->error(1606,
                       "subckt {} port {} has no corresponding liberty port, "
                       "pg_port and is not power or ground.",
                       cell_name, port_name);
      spice_port_names.push_back(port_name);
    }
  }
}

// Subckts can call subckts (asap7).
void
WriteSpice::findCellSubckts(StringSet &cell_names)
{
  std::ifstream lib_subckts_stream{std::string(lib_subckt_filename_)};
  if (lib_subckts_stream.is_open()) {
    std::string line;
    while (std::getline(lib_subckts_stream, line)) {
      // .subckt <cell_name> [args..]
      StringSeq tokens = parseTokens(line);
      if (tokens.size() >= 2 && stringEqual(tokens[0], ".subckt")) {
        const std::string &cell_name = tokens[1];
        if (cell_names.contains(cell_name)) {
          // Scan the subckt definition for subckt calls.
          std::string stmt;
          while (std::getline(lib_subckts_stream, line)) {
            if (line[0] == '+')
              stmt += line.substr(1);
            else {
              // Process previous statement.
              if (tolower(stmt[0]) == 'x') {
                StringSeq tokens = parseTokens(line);
                std::string &subckt_cell = tokens[tokens.size() - 1];
                cell_names.insert(subckt_cell);
              }
              stmt = line;
            }
            if (stringBeginEqual(line, ".ends"))
              break;
          }
        }
      }
    }
  }
  else
    throw FileNotReadable(lib_subckt_filename_);
}

////////////////////////////////////////////////////////////////

void
WriteSpice::writeSubcktInst(const Instance *inst)
{
  std::string inst_name = network_->pathName(inst);
  LibertyCell *cell = network_->libertyCell(inst);
  const std::string &cell_name = cell->name();
  StringSeq &spice_port_names = cell_spice_port_names_[cell_name];
  sta::print(spice_stream_, "x{}", inst_name);
  for (std::string &subckt_port_name : spice_port_names) {
    Pin *pin = network_->findPin(inst, subckt_port_name);
    LibertyPort *pg_port = cell->findLibertyPort(subckt_port_name);
    if (pin)
      sta::print(spice_stream_, " {}", network_->pathName(pin));
    else if (pg_port)
      sta::print(spice_stream_, " {}/{}", inst_name, subckt_port_name);
    else if (subckt_port_name == power_name_
             || subckt_port_name == gnd_name_)
      sta::print(spice_stream_, " {}/{}", inst_name, subckt_port_name);
  }
  sta::print(spice_stream_, " {}\n", cell_name);
}

// Power/ground and input voltage sources.
void
WriteSpice::writeSubcktInstVoltSrcs(const Instance *inst,
                                    LibertyPortLogicValues &port_values,
                                    const PinSet &excluded_input_pins)
{
  LibertyCell *cell = network_->libertyCell(inst);
  const std::string &cell_name = cell->name();
  StringSeq &spice_port_names = cell_spice_port_names_[cell_name];
  std::string inst_name = network_->pathName(inst);

  debugPrint(debug_, "write_spice", 2, "subckt {}", cell->name());
  for (std::string &subckt_port_name : spice_port_names) {
    LibertyPort *port = cell->findLibertyPort(subckt_port_name);
    const Pin *pin = port ? network_->findPin(inst, port) : nullptr;
    bool is_pg_port = port && port->isPwrGnd();
    debugPrint(debug_, "write_spice", 2, " port {}{}",
               subckt_port_name,
               is_pg_port ? " pwr/gnd" : "");
    if (is_pg_port)
      writeVoltageSource(inst_name, subckt_port_name, pgPortVoltage(port));
    else if (subckt_port_name == power_name_)
      writeVoltageSource(inst_name, subckt_port_name, power_voltage_);
    else if (subckt_port_name == gnd_name_)
      writeVoltageSource(inst_name, subckt_port_name, gnd_voltage_);
    else if (port && !excluded_input_pins.contains(pin)
             && port->direction()->isAnyInput()) {
      // Input voltage to sensitize path from gate input to output.
      // Look for tie high/low or propagated constant values.
      LogicValue port_value = scene_->mode()->sim()->simValue(pin);
      if (port_value == LogicValue::unknown) {
        bool has_value;
        LogicValue value;
        findKeyValue(port_values, port, value, has_value);
        if (has_value)
          port_value = value;
      }
      switch (port_value) {
        case LogicValue::zero:
        case LogicValue::unknown:
          writeVoltageSource(inst_name, subckt_port_name,
                             port->relatedGroundPort(), gnd_voltage_);
          break;
        case LogicValue::one:
          writeVoltageSource(inst_name, subckt_port_name,
                             port->relatedPowerPort(), power_voltage_);
          break;
        case LogicValue::rise:
        case LogicValue::fall:
          break;
      }
    }
  }
}

void
WriteSpice::writeVoltageSource(std::string_view inst_name,
                               std::string_view port_name,
                               float voltage)
{
  std::string node_name(inst_name);
  node_name += '/';
  node_name += port_name;
  writeVoltageSource(node_name, voltage);
}

void
WriteSpice::writeVoltageSource(std::string_view inst_name,
                               std::string_view subckt_port_name,
                               const LibertyPort *pg_port,
                               float voltage)
{
  if (pg_port)
    voltage = pgPortVoltage(pg_port);
  writeVoltageSource(inst_name, subckt_port_name, voltage);
}

void
WriteSpice::writeVoltageSource(LibertyCell *cell,
                               std::string_view inst_name,
                               std::string_view subckt_port_name,
                               const std::string &pg_port_name,
                               float voltage)
{
  if (!pg_port_name.empty()) {
    LibertyPort *pg_port = cell->findLibertyPort(pg_port_name);
    if (pg_port)
      voltage = pgPortVoltage(pg_port);
    else
      report_->error(1603, "{} pg_port {} not found,", cell->name(), pg_port_name);
  }
  writeVoltageSource(inst_name, subckt_port_name, voltage);
}

float
WriteSpice::pgPortVoltage(const LibertyPort *pg_port)
{
  LibertyLibrary *liberty = pg_port->libertyCell()->libertyLibrary();
  float voltage = 0.0;
  bool exists;
  const std::string &voltage_name = pg_port->voltageName();
  if (!voltage_name.empty()) {
    liberty->supplyVoltage(voltage_name, voltage, exists);
    if (!exists) {
      if (voltage_name == power_name_)
        voltage = power_voltage_;
      else if (voltage_name == gnd_name_)
        voltage = gnd_voltage_;
      else
        report_->error(1601, "pg_pin {}/{} voltage {} not found,",
                       pg_port->libertyCell()->name(), pg_port->name(),
                       voltage_name);
    }
  }
  else
    report_->error(1602, "Liberty pg_port {}/{} missing voltage_name attribute,",
                   pg_port->libertyCell()->name(), pg_port->name());
  return voltage;
}

////////////////////////////////////////////////////////////////

float
WriteSpice::findSlew(Vertex *vertex,
                     const RiseFall *rf,
                     const TimingArc *next_arc)
{
  DcalcAPIndex ap_index = scene_->dcalcAnalysisPtIndex(min_max_);
  float slew = delayAsFloat(graph_->slew(vertex, rf, ap_index));
  if (slew == 0.0 && next_arc)
    slew = slewAxisMinValue(next_arc);
  if (slew == 0.0)
    slew = units_->timeUnit()->scale();
  return slew;
}

// Look up the smallest slew axis value in the timing arc delay table.
float
WriteSpice::slewAxisMinValue(const TimingArc *arc)
{
  GateTableModel *gate_model = arc->gateTableModel(scene_, min_max_);
  if (gate_model) {
    const TableModel *model = gate_model->delayModels()->model();
    const TableAxis *axis1 = model->axis1();
    TableAxisVariable var1 = axis1->variable();
    if (var1 == TableAxisVariable::input_transition_time
        || var1 == TableAxisVariable::input_net_transition)
      return axis1->axisValue(0);

    const TableAxis *axis2 = model->axis2();
    TableAxisVariable var2 = axis2->variable();
    if (var2 == TableAxisVariable::input_transition_time
        || var2 == TableAxisVariable::input_net_transition)
      return axis2->axisValue(0);

    const TableAxis *axis3 = model->axis3();
    TableAxisVariable var3 = axis3->variable();
    if (var3 == TableAxisVariable::input_transition_time
        || var3 == TableAxisVariable::input_net_transition)
      return axis3->axisValue(0);
  }
  return 0.0;
}

////////////////////////////////////////////////////////////////

void
WriteSpice::writeDrvrParasitics(const Pin *drvr_pin,
                                const Parasitic *parasitic,
                                const NetSet &coupling_nets)
{
  Net *net = network_->net(drvr_pin);
  std::string net_name = net
    ? std::string(network_->pathName(net))
    : std::string(network_->pathName(drvr_pin));
  sta::print(spice_stream_, "* Net {}\n", net_name);

  if (parasitics_->isParasiticNetwork(parasitic))
    writeParasiticNetwork(drvr_pin, parasitic, coupling_nets);
  else if (parasitics_->isPiElmore(parasitic))
    writePiElmore(drvr_pin, parasitic);
  else {
    sta::print(spice_stream_, "* Net has no parasitics.\n");
    writeNullParasitic(drvr_pin);
  }
}

void
WriteSpice::writeParasiticNetwork(const Pin *drvr_pin,
                                  const Parasitic *parasitic,
                                  const NetSet &coupling_nets)
{
  std::set<const Pin *> reachable_pins;
  // Sort resistors for consistent regression results.
  ParasiticResistorSeq resistors = parasitics_->resistors(parasitic);
  sort(resistors, [this](const ParasiticResistor *r1, const ParasiticResistor *r2) {
    return parasitics_->id(r1) < parasitics_->id(r2);
  });
  for (ParasiticResistor *resistor : resistors) {
    float resistance = parasitics_->value(resistor);
    ParasiticNode *node1 = parasitics_->node1(resistor);
    ParasiticNode *node2 = parasitics_->node2(resistor);
    sta::print(spice_stream_, "R{} {} {} {:.3e}\n", res_index_++,
               parasitics_->name(node1), parasitics_->name(node2), resistance);

    // Necessary but not sufficient. Need a DFS.
    const Pin *pin1 = parasitics_->pin(node1);
    if (pin1)
      reachable_pins.insert(pin1);
    const Pin *pin2 = parasitics_->pin(node2);
    if (pin2)
      reachable_pins.insert(pin2);
  }

  // Add resistors from drvr to load for missing parasitic connections.
  auto pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (pin != drvr_pin && network_->isLoad(pin) && !network_->isHierarchical(pin)
        && !reachable_pins.contains(pin)) {
      sta::print(spice_stream_, "R{} {} {} {:.3e}\n", res_index_++,
                 network_->pathName(drvr_pin), network_->pathName(pin),
                 short_ckt_resistance_);
    }
  }
  delete pin_iter;

  // Grounded node capacitors.
  // Sort nodes for consistent regression results.
  ParasiticNodeSeq nodes = parasitics_->nodes(parasitic);
  sort(nodes, [this](const ParasiticNode *node1, const ParasiticNode *node2) {
    std::string name1 = parasitics_->name(node1);
    std::string name2 = parasitics_->name(node2);
    return name1 < name2;
  });

  for (ParasiticNode *node : nodes) {
    float cap = parasitics_->nodeGndCap(node);
    // Spice has a cow over zero value caps.
    if (cap > 0.0) {
      sta::print(spice_stream_, "C{} {} 0 {:.3e}\n", cap_index_++,
                 parasitics_->name(node), cap);
    }
  }

  // Sort coupling capacitors for consistent regression results.
  ParasiticCapacitorSeq capacitors = parasitics_->capacitors(parasitic);
  sort(capacitors,
       [this](const ParasiticCapacitor *c1, const ParasiticCapacitor *c2) {
         return parasitics_->id(c1) < parasitics_->id(c2);
       });
  const Net *net = pinNet(drvr_pin, network_);
  for (ParasiticCapacitor *capacitor : capacitors) {
    ParasiticNode *node1 = parasitics_->node1(capacitor);
    ParasiticNode *node2 = parasitics_->node2(capacitor);
    float cap = parasitics_->value(capacitor);
    const Net *net1 = node1 ? parasitics_->net(node1, network_) : nullptr;
    const Net *net2 = node2 ? parasitics_->net(node2, network_) : nullptr;
    if (net2 == net) {
      std::swap(net1, net2);
      std::swap(node1, node2);
    }
    if (net2 && coupling_nets.contains(net2))
      // Write half the capacitance because the coupled net will do the same.
      sta::print(spice_stream_, "C{} {} {} {:.3e}\n", cap_index_++,
                 parasitics_->name(node1), parasitics_->name(node2), cap * .5);
    else
      sta::print(spice_stream_, "C{} {} 0 {:.3e}\n", cap_index_++,
                 parasitics_->name(node1), cap);
  }
}

Net *
pinNet(const Pin *pin,
       const Network *network)
{
  Net *net = network->net(pin);
  // Pins on the top level instance may not have nets.
  // Use the net connected to the pin's terminal.
  if (net == nullptr && network->isTopLevelPort(pin)) {
    Term *term = network->term(pin);
    if (term)
      return network->net(term);
  }
  return net;
}

void
WriteSpice::writePiElmore(const Pin *drvr_pin,
                          const Parasitic *parasitic)
{
  float c2, rpi, c1;
  parasitics_->piModel(parasitic, c2, rpi, c1);
  const char *c1_node = "n1";
  sta::print(spice_stream_, "RPI {} {} {:.3e}\n", network_->pathName(drvr_pin),
             c1_node, rpi);
  if (c2 > 0.0)
    sta::print(spice_stream_, "C2 {} 0 {:.3e}\n", network_->pathName(drvr_pin), c2);
  if (c1 > 0.0)
    sta::print(spice_stream_, "C1 {} 0 {:.3e}\n", c1_node, c1);

  int load_index = 3;
  auto pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    const Pin *load_pin = pin_iter->next();
    if (load_pin != drvr_pin && network_->isLoad(load_pin)
        && !network_->isHierarchical(load_pin)) {
      float elmore;
      bool exists;
      parasitics_->findElmore(parasitic, load_pin, elmore, exists);
      if (exists) {
        sta::print(spice_stream_, "E{} el{} 0 {} 0 1.0\n", load_index, load_index,
                   network_->pathName(drvr_pin));
        sta::print(spice_stream_, "R{} el{} {} 1.0\n", load_index, load_index,
                   network_->pathName(load_pin));
        sta::print(spice_stream_, "C{} {} 0 {:.3e}\n", load_index,
                   network_->pathName(load_pin), elmore);
      }
      else
        // Add resistor from drvr to load for missing elmore.
        sta::print(spice_stream_, "R{} {} {} {:.3e}\n", load_index,
                   network_->pathName(drvr_pin), network_->pathName(load_pin),
                   short_ckt_resistance_);
      load_index++;
    }
  }
  delete pin_iter;
}

void
WriteSpice::writeNullParasitic(const Pin *drvr_pin)
{
  // Add resistors from drvr to load for missing parasitic connections.
  auto pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    const Pin *load_pin = pin_iter->next();
    if (load_pin != drvr_pin && network_->isLoad(load_pin)
        && !network_->isHierarchical(load_pin)) {
      sta::print(spice_stream_, "R{} {} {} {:.3e}\n", res_index_++,
                 network_->pathName(drvr_pin), network_->pathName(load_pin),
                 short_ckt_resistance_);
    }
  }
  delete pin_iter;
}

////////////////////////////////////////////////////////////////

void
WriteSpice::writeVoltageSource(std::string_view node_name,
                               float voltage)
{
  sta::print(spice_stream_, "v{} {} 0 {:.3f}\n", volt_index_++, node_name, voltage);
}

void
WriteSpice::writeWaveformVoltSource(const Pin *pin,
                                    DriverWaveform *drvr_waveform,
                                    const RiseFall *rf,
                                    float delay,
                                    float slew)
{
  float volt0, volt1, volt_factor;
  if (rf == RiseFall::rise()) {
    volt0 = gnd_voltage_;
    volt1 = power_voltage_;
    volt_factor = power_voltage_;
  }
  else {
    volt0 = power_voltage_;
    volt1 = gnd_voltage_;
    volt_factor = -power_voltage_;
  }
  sta::print(spice_stream_, "v{} {} 0 pwl(\n", volt_index_++,
             network_->pathName(pin));
  sta::print(spice_stream_, "+{:.3e} {:.3e}\n", 0.0, volt0);
  Table waveform = drvr_waveform->waveform(slew);
  const TableAxis *time_axis = waveform.axis1();
  for (size_t time_index = 0; time_index < time_axis->size(); time_index++) {
    float time = delay + time_axis->axisValue(time_index);
    float wave_volt = waveform.value(time_index);
    float volt = volt0 + wave_volt * volt_factor;
    sta::print(spice_stream_, "+{:.3e} {:.3e}\n", time, volt);
  }
  sta::print(spice_stream_, "+{:.3e} {:.3e}\n", max_time_, volt1);
  sta::print(spice_stream_, "+)\n");
}

void
WriteSpice::writeRampVoltSource(const Pin *pin,
                                const RiseFall *rf,
                                float time,
                                float slew)
{
  float volt0, volt1;
  if (rf == RiseFall::rise()) {
    volt0 = gnd_voltage_;
    volt1 = power_voltage_;
  }
  else {
    volt0 = power_voltage_;
    volt1 = gnd_voltage_;
  }
  sta::print(spice_stream_, "v{} {} 0 pwl(\n", volt_index_++,
             network_->pathName(pin));
  sta::print(spice_stream_, "+{:.3e} {:.3e}\n", 0.0, volt0);
  writeWaveformEdge(rf, time, slew);
  sta::print(spice_stream_, "+{:.3e} {:.3e}\n", max_time_, volt1);
  sta::print(spice_stream_, "+)\n");
}

// Write PWL rise/fall edge that crosses threshold at time.
void
WriteSpice::writeWaveformEdge(const RiseFall *rf,
                              float time,
                              float slew)
{
  float volt0, volt1;
  if (rf == RiseFall::rise()) {
    volt0 = gnd_voltage_;
    volt1 = power_voltage_;
  }
  else {
    volt0 = power_voltage_;
    volt1 = gnd_voltage_;
  }
  float threshold = default_library_->inputThreshold(rf);
  float dt = railToRailSlew(slew, rf);
  float time0 = time - dt * threshold;
  float time1 = time0 + dt;
  if (time0 > 0.0)
    sta::print(spice_stream_, "+{:.3e} {:.3e}\n", time0, volt0);
  sta::print(spice_stream_, "+{:.3e} {:.3e}\n", time1, volt1);
}

float
WriteSpice::railToRailSlew(float slew,
                           const RiseFall *rf)
{
  float lower = default_library_->slewLowerThreshold(rf);
  float upper = default_library_->slewUpperThreshold(rf);
  return slew / (upper - lower);
}

////////////////////////////////////////////////////////////////

// Find the logic values for expression inputs to sensitize the path from input_port.
void
WriteSpice::gatePortValues(const Pin *input_pin,
                           const Pin *drvr_pin,
                           const RiseFall *drvr_rf,
                           const Edge *gate_edge,
                           // Return values.
                           LibertyPortLogicValues &port_values,
                           bool &is_clked)
{
  is_clked = false;
  const Instance *inst = network_->instance(input_pin);
  const LibertyPort *input_port = network_->libertyPort(input_pin);
  const LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  const FuncExpr *drvr_func = drvr_port->function();
  if (drvr_func) {
    if (gate_edge && gate_edge->role()->genericRole() == TimingRole::regClkToQ())
      regPortValues(input_pin, drvr_rf, drvr_port, drvr_func, port_values, is_clked);
    else
      gatePortValues(inst, drvr_func, input_port, port_values);
  }
}

void
WriteSpice::gatePortValues(const Instance *,
                           const FuncExpr *expr,
                           const LibertyPort *input_port,
                           // Return values.
                           LibertyPortLogicValues &port_values)
{
  DdNode *bdd = bdd_.funcBdd(expr);
  DdNode *input_node = bdd_.findNode(input_port);
  unsigned input_node_index = Cudd_NodeReadIndex(input_node);
  DdManager *cudd_mgr = bdd_.cuddMgr();
  DdNode *diff = Cudd_bddBooleanDiff(cudd_mgr, bdd, input_node_index);
  int *cube;
  CUDD_VALUE_TYPE value;
  DdGen *cube_gen = Cudd_FirstCube(cudd_mgr, diff, &cube, &value);

  LibertyPortSet ports = expr->ports();
  for (const LibertyPort *port : ports) {
    if (port != input_port) {
      DdNode *port_node = bdd_.findNode(port);
      int var_index = Cudd_NodeReadIndex(port_node);
      LogicValue value;
      switch (cube[var_index]) {
        case 0:
          value = LogicValue::zero;
          break;
        case 1:
          value = LogicValue::one;
          break;
        case 2:
        default:
          value = LogicValue::unknown;
          break;
      }
      port_values[port] = value;
    }
  }
  Cudd_GenFree(cube_gen);
  Cudd_Ref(diff);
  bdd_.clearVarMap();
}

void
WriteSpice::regPortValues(const Pin *input_pin,
                          const RiseFall *drvr_rf,
                          const LibertyPort *drvr_port,
                          const FuncExpr *drvr_func,
                          // Return values.
                          LibertyPortLogicValues &port_values,
                          bool &is_clked)
{
  is_clked = false;
  LibertyPort *q_port = drvr_func->port();
  if (q_port) {
    // Drvr (register/latch output) function should be a reference
    // to an internal port like IQ or IQN.
    LibertyCell *cell = drvr_port->libertyCell();
    Sequential *seq = cell->outputPortSequential(q_port);
    if (seq) {
      seqPortValues(seq, drvr_rf, port_values);
      is_clked = true;
    }
    else {
      const LibertyPort *input_port = network_->libertyPort(input_pin);
      report_->error(1604, "no register/latch found for path from {} to {},",
                     input_port->name(), drvr_port->name());
    }
  }
}

void
WriteSpice::seqPortValues(Sequential *seq,
                          const RiseFall *rf,
                          // Return values.
                          LibertyPortLogicValues &port_values)
{
  FuncExpr *data = seq->data();
  // SHOULD choose values for all ports of data to make output rise/fall
  // matching rf.
  LibertyPort *port = onePort(data);
  if (port) {
    TimingSense sense = data->portTimingSense(port);
    switch (sense) {
      case TimingSense::positive_unate:
        if (rf == RiseFall::rise())
          port_values[port] = LogicValue::one;
        else
          port_values[port] = LogicValue::zero;
        break;
      case TimingSense::negative_unate:
        if (rf == RiseFall::rise())
          port_values[port] = LogicValue::zero;
        else
          port_values[port] = LogicValue::one;
        break;
      case TimingSense::non_unate:
      case TimingSense::none:
      case TimingSense::unknown:
      default:
        break;
    }
  }
}

// Pick a port, any port...
LibertyPort *
WriteSpice::onePort(FuncExpr *expr)
{
  FuncExpr *left = expr->left();
  FuncExpr *right = expr->right();
  LibertyPort *port;
  switch (expr->op()) {
    case FuncExpr::Op::port:
      return expr->port();
    case FuncExpr::Op::not_:
      return onePort(left);
    case FuncExpr::Op::or_:
    case FuncExpr::Op::and_:
    case FuncExpr::Op::xor_:
      port = onePort(left);
      if (port == nullptr)
        port = onePort(right);
      return port;
    case FuncExpr::Op::one:
    case FuncExpr::Op::zero:
    default:
      return nullptr;
  }
}

////////////////////////////////////////////////////////////////

PinSeq
WriteSpice::drvrLoads(const Pin *drvr_pin)
{
  PinSeq loads;
  Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      const Pin *load_pin = load_vertex->pin();
      loads.push_back(load_pin);
    }
  }
  return loads;
}

void
WriteSpice::writeSubcktInstLoads(const Pin *drvr_pin,
                                 const Pin *path_load,
                                 const PinSet &excluded_input_pins,
                                 InstanceSet &written_insts)
{
  sta::print(spice_stream_, "* Load pins\n");
  PinSeq drvr_loads = drvrLoads(drvr_pin);
  // Do not sensitize side load gates.
  LibertyPortLogicValues port_values;
  for (const Pin *load_pin : drvr_loads) {
    const Instance *load_inst = network_->instance(load_pin);
    if (load_pin != path_load && network_->direction(load_pin)->isAnyInput()
        && !network_->isHierarchical(load_pin) && !network_->isTopLevelPort(load_pin)
        && !written_insts.contains(load_inst)) {
      writeSubcktInst(load_inst);
      writeSubcktInstVoltSrcs(load_inst, port_values, excluded_input_pins);
      sta::print(spice_stream_, "\n");
      written_insts.insert(load_inst);
    }
  }
}

////////////////////////////////////////////////////////////////

void
WriteSpice::writeMeasureDelayStmt(const Pin *from_pin,
                                  const RiseFall *from_rf,
                                  const Pin *to_pin,
                                  const RiseFall *to_rf,
                                  std::string_view prefix)
{
  std::string from_pin_name = network_->pathName(from_pin);
  float from_threshold = power_voltage_ * default_library_->inputThreshold(from_rf);
  std::string to_pin_name = network_->pathName(to_pin);
  float to_threshold = power_voltage_ * default_library_->inputThreshold(to_rf);
  sta::print(spice_stream_, ".measure tran {}_{}_delay_{}\n", prefix,
             from_pin_name, to_pin_name);
  sta::print(spice_stream_, "+trig v({}) val={:.3f} {}=last\n", from_pin_name,
             from_threshold, spiceTrans(from_rf));
  sta::print(spice_stream_, "+targ v({}) val={:.3f} {}=last\n", to_pin_name,
             to_threshold, spiceTrans(to_rf));
}

void
WriteSpice::writeMeasureSlewStmt(const Pin *pin,
                                 const RiseFall *rf,
                                 std::string_view prefix)
{
  std::string pin_name = network_->pathName(pin);
  std::string_view spice_rf = spiceTrans(rf);
  float lower = power_voltage_ * default_library_->slewLowerThreshold(rf);
  float upper = power_voltage_ * default_library_->slewUpperThreshold(rf);
  float threshold1, threshold2;
  if (rf == RiseFall::rise()) {
    threshold1 = lower;
    threshold2 = upper;
  }
  else {
    threshold1 = upper;
    threshold2 = lower;
  }
  sta::print(spice_stream_, ".measure tran {}_{}_slew\n", prefix, pin_name);
  sta::print(spice_stream_, "+trig v({}) val={:.3f} {}=last\n", pin_name, threshold1,
             spice_rf);
  sta::print(spice_stream_, "+targ v({}) val={:.3f} {}=last\n", pin_name, threshold2,
             spice_rf);
}

////////////////////////////////////////////////////////////////

std::string_view
WriteSpice::spiceTrans(const RiseFall *rf)
{
  if (rf == RiseFall::rise())
    return "RISE";
  else
    return "FALL";
}

////////////////////////////////////////////////////////////////

// Unused
// PWL voltage source that rises half way into the first clock cycle.
void
WriteSpice::writeClkedStepSource(const Pin *pin,
                                 const RiseFall *rf,
                                 const Clock *clk)
{
  Vertex *vertex = graph_->pinLoadVertex(pin);
  float slew = findSlew(vertex, rf, nullptr);
  float time = clkWaveformTimeOffset(clk) + clk->period() / 2.0;
  writeRampVoltSource(pin, rf, time, slew);
}

float
WriteSpice::clkWaveformTimeOffset(const Clock *clk)
{
  return clk->period() / 10;
}

}  // namespace sta
