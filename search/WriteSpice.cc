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

#include <string>
#include <iostream>
#include <fstream>
#include <regex>
#include "Machine.hh"
#include "Debug.hh"
#include "Error.hh"
#include "Report.hh"
#include "StringUtil.hh"
#include "FuncExpr.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "DcalcAnalysisPt.hh"
#include "Parasitics.hh"
#include "PathAnalysisPt.hh"
#include "Path.hh"
#include "PathRef.hh"
#include "PathExpanded.hh"
#include "StaState.hh"
#include "WriteSpice.hh"

namespace sta {

using std::string;
using std::ofstream;
using std::ifstream;

typedef Vector<string> StringVector;
typedef Map<string, StringVector*> CellSpicePortNames;
typedef int Stage;
typedef Map<ParasiticNode*, int> ParasiticNodeMap;

void
split(const string &text,
      const string &delims,
      // Return values.
      StringVector &tokens);
void
streamPrint(ofstream &stream,
	    const char *fmt,
	    ...) __attribute__((format (printf, 2, 3)));

////////////////////////////////////////////////////////////////

class WriteSpice : public StaState
{
public:
  WriteSpice(Path *path,
	     const char *spice_filename,
	     const char *subckts_filename,
	     const char *lib_subckts_filename,
	     const char *models_filename,
	     const StaState *sta);
  ~WriteSpice();
  void writeSpice();;

private:
  void writeHeader();
  void writeStageInstances();
  void writeInputSource();
  void writeStageSubckts();
  void writeInputStage(Stage stage);
  void writeMeasureStmts();
  void writeGateStage(Stage stage);
  void writeStageVoltageSources(LibertyCell *cell,
				StringVector *spice_port_names,
				const char *inst_name,
				LibertyPort *from_port,
				LibertyPort *drvr_port);
  void writeStageParasitics(Stage stage);
  void writeSubckts();
  void findPathCellnames(// Return values.
			 StringSet &path_cell_names);
  void recordSpicePortNames(const char *cell_name,
			    StringVector &tokens);
  float pgPortVoltage(const char *pg_port_name,
		      LibertyCell *cell);
  float pgPortVoltage(LibertyPgPort *pg_port);
  float maxTime();
  const char *nodeName(ParasiticNode *node);
  void initNodeMap(const char *net_name);

  // Stage "accessors".
  // Internally a stage index from stageFirst() to stageLast()
  // is turned into an index into path_expanded_.
  Stage stageFirst();
  Stage stageLast();
  string stageName(Stage stage);
  int stageGateInputPathIndex(Stage stage);
  int stageDrvrPathIndex(Stage stage);
  int stageLoadPathIndex(Stage stage);
  PathRef *stageGateInputPath(Stage stage);
  PathRef *stageDrvrPath(Stage stage);
  PathRef *stageLoadPath(Stage stage);
  TimingArc *stageGateArc(Stage stage);
  TimingArc *stageWireArc(Stage stage);
  Edge *stageGateEdge(Stage stage);
  Edge *stageWireEdge(Stage stage);
  Pin *stageInputPin(Stage stage);
  Pin *stageDrvrPin(Stage stage);
  Pin *stageLoadPin(Stage stage);
  const char *stageInputPinName(Stage stage);
  const char *stageDrvrPinName(Stage stage);
  const char *stageLoadPinName(Stage stage);

  Path *path_;
  const char *spice_filename_;
  const char *subckts_filename_;
  const char *lib_subckts_filename_;
  const char *models_filename_;

  ofstream spice_stream_;
  PathExpanded path_expanded_;
  CellSpicePortNames cell_spice_port_names_;
  ParasiticNodeMap node_map_;
  int next_node_index_;
  const char *net_name_;

  // Resistance to use to simulate a short circuit between spice nodes.
  static const float short_ckt_resistance_;
};

////////////////////////////////////////////////////////////////

class SubcktEndsMissing : public StaException
{
public:
  SubcktEndsMissing(const char *cell_name,
		    const char *subckt_filename);;
  const char *what() const throw();

protected:
  string what_;
};

SubcktEndsMissing::SubcktEndsMissing(const char *cell_name,
				     const char *subckt_filename)
{
  what_ = "Error: spice subckt for cell ";
  what_ += cell_name;
  what_ += " missing .ends in ";
  what_ += subckt_filename;
}

const char *
SubcktEndsMissing::what() const throw()
{
  return what_.c_str();
}

////////////////////////////////////////////////////////////////

void
writeSpice (Path *path,
	    const char *spice_filename,
	    const char *subckts_filename,
	    const char *lib_subckts_filename,
	    const char *models_filename,
	    StaState *sta)
{
  WriteSpice writer(path, spice_filename, subckts_filename,
		    lib_subckts_filename, models_filename, sta);
  writer.writeSpice();
}

const float WriteSpice::short_ckt_resistance_ = .0001;

WriteSpice::WriteSpice(Path *path,
		       const char *spice_filename,
		       const char *subckts_filename,
		       const char *lib_subckts_filename,
		       const char *models_filename,
		       const StaState *sta) :
  StaState(sta),
  path_(path),
  spice_filename_(spice_filename),
  subckts_filename_(subckts_filename),
  lib_subckts_filename_(lib_subckts_filename),
  models_filename_(models_filename),
  path_expanded_(sta),
  net_name_(NULL)
{
}

WriteSpice::~WriteSpice()
{
  cell_spice_port_names_.deleteContents();
}

void
WriteSpice::writeSpice()
{
  spice_stream_.open(spice_filename_);
  if (spice_stream_.is_open()) {
    path_expanded_.expand(path_, true);
    // Find subckt port names as a side-effect of writeSubckts.
    writeSubckts();
    writeHeader();
    writeStageInstances();
    writeInputSource();
    writeStageSubckts();
    streamPrint(spice_stream_, ".end\n");
    spice_stream_.close();
  }
  else
    throw FileNotWritable(spice_filename_);
}

void
WriteSpice::writeHeader()
{
  const MinMax *min_max = path_->minMax(this);
  const Pvt *pvt = sdc_->operatingConditions(min_max);
  if (pvt == NULL)
    pvt = network_->defaultLibertyLibrary()->defaultOperatingConditions();
  float temp = pvt->temperature();
  streamPrint(spice_stream_, ".temp %.1f\n", temp);
  streamPrint(spice_stream_, ".include \"%s\"\n", models_filename_);
  streamPrint(spice_stream_, ".include \"%s\"\n", subckts_filename_);

  float max_time = maxTime();
  float time_step = max_time / 1e+3;
  streamPrint(spice_stream_, ".tran %.3g %.3g\n\n",
	      time_step, max_time);
}

float
WriteSpice::maxTime()
{
  float end_slew = path_->slew(this);
  float max_time = (path_->arrival(this) + end_slew * 2) * 1.5;
  return max_time;
}

void
WriteSpice::writeStageInstances()
{
  streamPrint(spice_stream_, "*****************\n");
  streamPrint(spice_stream_, "* Stage instances\n");
  streamPrint(spice_stream_, "*****************\n\n");

  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    const char *stage_name = stageName(stage).c_str();
    if (stage == stageFirst())
      streamPrint(spice_stream_, "x%s %s %s %s\n",
		  stage_name,
		  stageDrvrPinName(stage),
		  stageLoadPinName(stage),
		  stage_name);
    else 
      streamPrint(spice_stream_, "x%s %s %s %s %s\n",
		  stage_name,
		  stageInputPinName(stage),
		  stageDrvrPinName(stage),
		  stageLoadPinName(stage),
		  stage_name);
  }
  streamPrint(spice_stream_, "\n");
}

float
WriteSpice::pgPortVoltage(const char *pg_port_name,
			  LibertyCell *cell)
{
  auto pg_port = cell->findPgPort(pg_port_name);
  return pgPortVoltage(pg_port);
}

float
WriteSpice::pgPortVoltage(LibertyPgPort *pg_port)
{
  auto cell = pg_port->cell();
  auto voltage_name = pg_port->voltageName();
  auto lib = cell->libertyLibrary();
  float voltage = lib->supplyVoltage(voltage_name);
  return voltage;
}

void
WriteSpice::writeInputSource()
{
  streamPrint(spice_stream_, "**************\n");
  streamPrint(spice_stream_, "* Input source\n");
  streamPrint(spice_stream_, "**************\n\n");

  Stage input_stage = stageFirst();
  streamPrint(spice_stream_, "v1 %s 0 pwl(\n",
	      stageDrvrPinName(input_stage));
  auto wire_arc = stageWireArc(input_stage);
  auto load_pin = stageLoadPin(input_stage);
  auto cell = network_->libertyCell(network_->instance(load_pin));
  auto load_port = network_->libertyPort(load_pin);
  const char *pg_gnd_port_name = load_port->relatedGroundPin();
  const char *pg_pwr_port_name = load_port->relatedPowerPin();
  auto gnd_volt = pgPortVoltage(pg_gnd_port_name, cell);
  auto pwr_volt = pgPortVoltage(pg_pwr_port_name, cell);
  float volt0, volt1;
  if (wire_arc->fromTrans()->asRiseFall() == TransRiseFall::rise()) {
    volt0 = gnd_volt;
    volt1 = pwr_volt;
  }
  else {
    volt0 = pwr_volt;
    volt1 = gnd_volt;
  }
  float time0 = .1e-9;
  float time1 = .2e-9;
  streamPrint(spice_stream_, "+%.3e %.3e\n", 0.0, volt0);
  streamPrint(spice_stream_, "+%.3e %.3e\n", time0, volt0);
  streamPrint(spice_stream_, "+%.3e %.3e\n", time1, volt1);
  streamPrint(spice_stream_, "+%.3e %.3e\n", maxTime(), volt1);
  streamPrint(spice_stream_, "+)\n\n");
}

void
WriteSpice::writeMeasureStmts()
{
  streamPrint(spice_stream_, "********************\n");
  streamPrint(spice_stream_, "* Measure statements\n");
  streamPrint(spice_stream_, "********************\n\n");
}

void
WriteSpice::writeStageSubckts()
{
  streamPrint(spice_stream_, "***************\n");
  streamPrint(spice_stream_, "* Stage subckts\n");
  streamPrint(spice_stream_, "***************\n\n");

  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    if (stage == stageFirst())
      writeInputStage(stage);
    else
      writeGateStage(stage);
  }
}

// Input port to first gate input.
void
WriteSpice::writeInputStage(Stage stage)
{
  // Input arc.
  // External driver not handled.
  auto drvr_pin_name = stageDrvrPinName(stage);
  auto load_pin_name = stageLoadPinName(stage);
  streamPrint(spice_stream_, ".subckt %s %s %s\n",
	      stageName(stage).c_str(),
	      drvr_pin_name,
	      load_pin_name);
  writeStageParasitics(stage);
  streamPrint(spice_stream_, ".ends\n\n");
}

// Gate and load parasitics.
void
WriteSpice::writeGateStage(Stage stage)
{
  auto input_pin = stageInputPin(stage);
  auto input_pin_name = stageInputPinName(stage);
  auto drvr_pin = stageDrvrPin(stage);
  auto drvr_pin_name = stageDrvrPinName(stage);
  auto load_pin_name = stageLoadPinName(stage);
  streamPrint(spice_stream_, ".subckt stage%d %s %s %s\n",
	      stage,
	      input_pin_name,
	      drvr_pin_name,
	      load_pin_name);
  Instance *inst = network_->instance(input_pin);
  const char *inst_name = network_->pathName(inst);
  LibertyCell *cell = network_->libertyCell(inst);
  const char *cell_name = cell->name();
  auto spice_port_names = cell_spice_port_names_[cell_name];

  // Instance subckt call.
  streamPrint(spice_stream_, "x%s", inst_name);
  StringVector::Iterator port_iter(spice_port_names);
  while (port_iter.hasNext()) {
    const char *subckt_port_name = port_iter.next().c_str();
    auto pin = network_->findPin(inst, subckt_port_name);
    auto pg_port = cell->findPgPort(subckt_port_name);
    const char *pin_name;
    if (pin) {
      pin_name = network_->pathName(pin);
      streamPrint(spice_stream_, " %s", pin_name);
    }
    else if (pg_port)
      streamPrint(spice_stream_, " %s/%s", inst_name, subckt_port_name);
  }
  streamPrint(spice_stream_, " %s\n", cell_name);

  writeStageVoltageSources(cell, spice_port_names,
			   inst_name,
			   network_->libertyPort(input_pin),
			   network_->libertyPort(drvr_pin));
  writeStageParasitics(stage);
  streamPrint(spice_stream_, ".ends\n\n");
}

typedef Map<LibertyPort*, LogicValue> LibertyPortLogicValues;

// Find the logic values for expression inputs to enable paths from_port.
void
sensitizationValues(FuncExpr *expr,
		    LibertyPort *from_port,
		    // Return values.
		    LibertyPortLogicValues &port_values)
{
  switch (expr->op()) {
  case FuncExpr::op_port: {
    break;
  }
  case FuncExpr::op_not: {
    sensitizationValues(expr->left(), from_port, port_values);
    break;
  }
  case FuncExpr::op_or: {
    FuncExpr *left = expr->left();
    FuncExpr *right = expr->right();
    if (left->port() == from_port
	&& right->op() == FuncExpr::op_port)
      port_values[right->port()] = logic_zero;
    else if (right->port() == from_port
	     && left->op() == FuncExpr::op_port)
      port_values[left->port()] = logic_zero;
    break;
  }
  case FuncExpr::op_and: {
    FuncExpr *left = expr->left();
    FuncExpr *right = expr->right();
    if (left->port() == from_port
	&& right->op() == FuncExpr::op_port)
      port_values[right->port()] = logic_one;
    else if (right->port() == from_port
	     && left->op() == FuncExpr::op_port)
      port_values[left->port()] = logic_one;
    break;
  }
  case FuncExpr::op_xor: {
    // Need to know timing arc sense to get this right.
    FuncExpr *left = expr->left();
    FuncExpr *right = expr->right();
    if (left->port() == from_port
	&& right->op() == FuncExpr::op_port)
      port_values[right->port()] = logic_zero;
    else if (right->port() == from_port
	     && left->op() == FuncExpr::op_port)
      port_values[left->port()] = logic_zero;
    break;
  }
  case FuncExpr::op_one:
  case FuncExpr::op_zero:
    break;
  }
}

// Power/ground and input voltage sources.
void
WriteSpice::writeStageVoltageSources(LibertyCell *cell,
				     StringVector *spice_port_names,
				     const char *inst_name,
				     LibertyPort *from_port,
				     LibertyPort *drvr_port)
{
  auto from_port_name = from_port->name();
  auto drvr_port_name = drvr_port->name();
  LibertyLibrary *lib = cell->libertyLibrary();
  LibertyPortLogicValues port_values;
  sensitizationValues(drvr_port->function(), from_port, port_values);
  int volt_source = 1;
  debugPrint1(debug_, "write_spice", 2, "subckt %s\n", cell->name());
  StringVector::Iterator port_iter(spice_port_names);
  while (port_iter.hasNext()) {
    auto subckt_port_name = port_iter.next().c_str();
    auto pg_port = cell->findPgPort(subckt_port_name);
    debugPrint2(debug_, "write_spice", 2, " port %s%s\n",
		subckt_port_name,
		pg_port ? " pwr/gnd" : "");
    if (pg_port) {
      auto voltage = pgPortVoltage(pg_port);
      streamPrint(spice_stream_, "v%d %s/%s 0 %.3f\n",
		  volt_source,
		  inst_name, subckt_port_name,
		  voltage);
      volt_source++;
    } else if (!(stringEq(subckt_port_name, from_port_name)
		 || stringEq(subckt_port_name, drvr_port_name))) {
      // Input voltage to sensitize path from gate input to output.
      LibertyPort *port = cell->findLibertyPort(subckt_port_name);
      if (port) {
	const char *pg_port_name = NULL;
	bool port_has_value;
	LogicValue port_value;
	port_values.findKey(port, port_value, port_has_value);
	if (port_has_value) {
	  switch (port_value) {
	  case logic_zero:
	    pg_port_name = port->relatedGroundPin();
	    break;
	  case logic_one:
	    pg_port_name = port->relatedPowerPin();
	    break;
	  default:
	    break;
	  }
	  if (pg_port_name) {
	    auto pg_port = cell->findPgPort(pg_port_name);
	    if (pg_port) {
	      auto voltage_name = pg_port->voltageName();
	      if (voltage_name) {
		float voltage = lib->supplyVoltage(voltage_name);
		streamPrint(spice_stream_, "v%d %s/%s 0 %.3f\n",
			    volt_source,
			    inst_name, subckt_port_name,
			    voltage);
		volt_source++;
	      }
	      else
	      report_->error("port %s %s voltage %s not found,\n",
			     subckt_port_name,
			     pg_port_name,
			     voltage_name);
	    }
	    else
	      report_->error("port %s %s not found,\n",
			     subckt_port_name,
			     pg_port_name);
	  }
	}
      }
    }
  }
}

typedef Set<ParasiticDevice*> ParasiticDeviceSet;
typedef Set<ParasiticNode*> ParasiticNodeSet;

void
findParasiticDevicesNodes(ParasiticNode *node,
			  Parasitics *parasitics,
			  // Return values.
			  ParasiticNodeSet &nodes,
			  ParasiticDeviceSet &devices)
{
  nodes.insert(node);
  auto device_iter = parasitics->deviceIterator(node);
  while (device_iter->hasNext()) {
    auto device = device_iter->next();
    if (!devices.hasKey(device)) {
      devices.insert(device);
      auto other_node = parasitics->otherNode(device, node);
      findParasiticDevicesNodes(other_node, parasitics, nodes, devices);
    }
  }
  delete device_iter;
}

void
WriteSpice::writeStageParasitics(Stage stage)
{
  auto drvr_path = stageDrvrPath(stage);
  auto drvr_pin = stageDrvrPin(stage);
  auto load_pin = stageLoadPin(stage);
  auto dcalc_ap = drvr_path->dcalcAnalysisPt(this);
  auto parasitic_ap = dcalc_ap->parasiticAnalysisPt();
  auto parasitic = parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
  int resistor_index = 1;
  int cap_index = 1;
  if (parasitic) {
    Net *net = network_->net(drvr_pin);
    auto net_name =
      net ? network_->pathName(net) : network_->pathName(drvr_pin);
    initNodeMap(net_name);
    streamPrint(spice_stream_, "* Net %s\n", net_name);
    auto node = parasitics_->findNode(parasitic, drvr_pin);
    ParasiticNodeSet nodes;
    ParasiticDeviceSet devices;
    findParasiticDevicesNodes(node, parasitics_, nodes, devices);
    ParasiticDeviceSet::Iterator device_iter(devices);
    while (device_iter.hasNext()) {
      auto device = device_iter.next();
      auto resistance = parasitics_->value(device, parasitic_ap);
      if (parasitics_->isResistor(device)) {
	ParasiticNode *node1, *node2;
	parasitics_->resistorNodes(device, node1, node2);
	streamPrint(spice_stream_, "R%d %s %s %.3e\n",
		    resistor_index,
		    nodeName(node1),
		    nodeName(node2),
		    resistance);
	resistor_index++;
      }
      else if (parasitics_->isCouplingCap(device)) {
      }
    }
    ParasiticNodeSet::Iterator node_iter(nodes);
    while (node_iter.hasNext()) {
      auto node = node_iter.next();
      auto cap = parasitics_->nodeGndCap(node, parasitic_ap);
      streamPrint(spice_stream_, "C%d %s 0 %.3e\n",
		  cap_index,
		  nodeName(node),
		  cap);
      cap_index++;
    }
  }
  else
    streamPrint(spice_stream_, "R1 %s %s %.3e\n",
		network_->pathName(drvr_pin),
		network_->pathName(load_pin),
		short_ckt_resistance_);
}

void
WriteSpice::initNodeMap(const char *net_name)
{
  stringDelete(net_name_);
  node_map_.clear();
  next_node_index_ = 1;
  net_name_ = stringCopy(net_name);
}

const char *
WriteSpice::nodeName(ParasiticNode *node)
{
  auto pin = parasitics_->connectionPin(node);
  if (pin)
    return parasitics_->name(node);
  else {
    int node_index;
    bool node_index_exists;
    node_map_.findKey(node, node_index, node_index_exists);
    if (!node_index_exists) {
      node_index = next_node_index_++;
      node_map_[node] = node_index;
    }
    return stringPrintTmp(strlen(net_name_) + 10, "%s/%d",
			  net_name_, node_index);
  }
}

////////////////////////////////////////////////////////////////

// Copy the subckt definition from lib_subckts_filename for
// each cell in path to path_subckts_filename.
void
WriteSpice::writeSubckts()
{
  StringSet path_cell_names;
  findPathCellnames(path_cell_names);

  ifstream lib_subckts_stream(lib_subckts_filename_);
  if (lib_subckts_stream.is_open()) {
    ofstream subckts_stream(subckts_filename_);
    if (subckts_stream.is_open()) {
      string line;
      while (getline(lib_subckts_stream, line)) {
	// .subckt <cell_name> [args..]
	StringVector tokens;
	split(line, " \t", tokens);
	if (tokens.size() >= 2
	    && stringEqual(tokens[0].c_str(), ".subckt")) {
	  const char *cell_name = tokens[1].c_str();
	  if (path_cell_names.hasKey(cell_name)) {
	    subckts_stream << line << "\n";
	    bool found_ends = false;
	    while (getline(lib_subckts_stream, line)) {
	      subckts_stream << line << "\n";
	      if (stringEqual(line.c_str(), ".ends")) {
		subckts_stream << "\n";
		found_ends = true;
		break;
	      }
	    }
	    if (!found_ends)
	      throw SubcktEndsMissing(cell_name, lib_subckts_filename_);
	    path_cell_names.eraseKey(cell_name);
	  }
	  recordSpicePortNames(cell_name, tokens);
	}
      }
      subckts_stream.close();
      lib_subckts_stream.close();

      if (!path_cell_names.empty()) {
	StringSet::Iterator cell_iter(path_cell_names);
	report_->error("The following subkcts are missing from %s\n",
		       lib_subckts_filename_);
	while (cell_iter.hasNext()) {
	  const char *cell_name = cell_iter.next();
	  report_->printError(" %s\n", cell_name);
	}
      }
    }
    else {
      lib_subckts_stream.close();
      throw FileNotWritable(subckts_filename_);
    }
  }
  else
    throw FileNotReadable(lib_subckts_filename_);
}

void
WriteSpice::findPathCellnames(// Return values.
			      StringSet &path_cell_names)
{
  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    auto arc = stageGateArc(stage);
    if (arc) {
      LibertyCell *cell = arc->set()->libertyCell();
      if (cell) {
	debugPrint1(debug_, "write_spice", 2, "cell %s\n", cell->name());
	path_cell_names.insert(cell->name());
      }
    }
  }
}

void
WriteSpice::recordSpicePortNames(const char *cell_name,
				 StringVector &tokens)
{
  auto cell = network_->findLibertyCell(cell_name);
  auto spice_port_names = new StringVector;
  for (int i = 2; i < tokens.size(); i++) {
    const char *port_name = tokens[i].c_str();
    auto port = cell->findLibertyPort(port_name);
    auto pg_port = cell->findPgPort(port_name);
    if (port == NULL && pg_port == NULL)
      report_->error("subckt %s port %s has no corresponding liberty port or pg_port.\n",
		     cell_name, port_name);
    spice_port_names->push_back(port_name);
  }
  cell_spice_port_names_[cell_name] = spice_port_names;
}

////////////////////////////////////////////////////////////////

Stage
WriteSpice::stageFirst()
{
  return 1;
}

Stage
WriteSpice::stageLast()
{
  return (path_expanded_.size() + 1) / 2;
}

string
WriteSpice::stageName(Stage stage)
{
  string name;
  stringPrint(name, "stage%d", stage);
  return name;
}

int
WriteSpice::stageGateInputPathIndex(Stage stage)
{
  return stage * 2 - 3;
}

int
WriteSpice::stageDrvrPathIndex(Stage stage)
{
  return stage * 2 - 2;
}

int
WriteSpice::stageLoadPathIndex(Stage stage)
{
  return stage * 2 - 1;
}

PathRef *
WriteSpice::stageGateInputPath(Stage stage)
{
  int path_index = stageGateInputPathIndex(stage);
  return path_expanded_.path(path_index);
}

PathRef *
WriteSpice::stageDrvrPath(Stage stage)
{
  int path_index = stageDrvrPathIndex(stage);
  return path_expanded_.path(path_index);
}

PathRef *
WriteSpice::stageLoadPath(Stage stage)
{
  int path_index = stageLoadPathIndex(stage);
  return path_expanded_.path(path_index);
}

TimingArc *
WriteSpice::stageGateArc(Stage stage)
{
  int path_index = stageDrvrPathIndex(stage);
  if (path_index >= 0)
    return path_expanded_.prevArc(path_index);
  else
    return NULL;
}

TimingArc *
WriteSpice::stageWireArc(Stage stage)
{
  int path_index = stageLoadPathIndex(stage);
  return path_expanded_.prevArc(path_index);
}

Edge *
WriteSpice::stageGateEdge(Stage stage)
{
  PathRef *path = stageGateInputPath(stage);
  TimingArc *arc = stageGateArc(stage);
  return path->prevEdge(arc, this);
}

Edge *
WriteSpice::stageWireEdge(Stage stage)
{
  PathRef *path = stageLoadPath(stage);
  TimingArc *arc = stageWireArc(stage);
  return path->prevEdge(arc, this);
}

Pin *
WriteSpice::stageInputPin(Stage stage)
{
  PathRef *path = stageGateInputPath(stage);
  return path->pin(this);
}

Pin *
WriteSpice::stageDrvrPin(Stage stage)
{
  PathRef *path = stageDrvrPath(stage);
  return path->pin(this);
}

Pin *
WriteSpice::stageLoadPin(Stage stage)
{
  PathRef *path = stageLoadPath(stage);
  return path->pin(this);
}

const char *
WriteSpice::stageInputPinName(Stage stage)
{
  const Pin *pin = stageInputPin(stage);
  return network_->pathName(pin);
}

const char *
WriteSpice::stageDrvrPinName(Stage stage)
{
  Pin *pin = stageDrvrPin(stage);
  return network_->pathName(pin);
}

const char *
WriteSpice::stageLoadPinName(Stage stage)
{
  const Pin *pin = stageLoadPin(stage);
  return network_->pathName(pin);
}

////////////////////////////////////////////////////////////////

void
split(const string &text,
      const string &delims,
      // Return values.
      StringVector &tokens)
{
  auto start = text.find_first_not_of(delims);
  auto end = text.find_first_of(delims, start);
  while (end != string::npos) {
    tokens.push_back(text.substr(start, end - start));
    start = text.find_first_not_of(delims, end);
    end = text.find_first_of(delims, start);
  }
  if (start != string::npos)
    tokens.push_back(text.substr(start));
}

// fprintf for c++ streams.
// Yes, I hate formatted output to ostream THAT much.
void
streamPrint(ofstream &stream,
	    const char *fmt,
	    ...)
{
  va_list args;
  va_start(args, fmt);
  char *result;
  vasprintf(&result, fmt, args);
  stream << result;
  free(result);
  va_end(args);
}

} // namespace
