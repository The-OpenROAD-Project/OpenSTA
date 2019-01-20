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
#include "Units.hh"
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
#include "Sim.hh"
#include "WritePathSpice.hh"

namespace sta {

using std::string;
using std::ofstream;
using std::ifstream;

typedef Vector<string> StringVector;
typedef Map<string, StringVector*> CellSpicePortNames;
typedef int Stage;
typedef Map<ParasiticNode*, int> ParasiticNodeMap;
typedef Map<LibertyPort*, LogicValue> LibertyPortLogicValues;

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

class WritePathSpice : public StaState
{
public:
  WritePathSpice(Path *path,
		 const char *spice_filename,
		 const char *subckt_filename,
		 const char *lib_subckt_filename,
		 const char *model_filename,
		 const char *power_name,
		 const char *gnd_name,
		 const StaState *sta);
  ~WritePathSpice();
  void writeSpice();;

private:
  void writeHeader();
  void writeStageInstances();
  void writeInputSource();
  void writeStageSubckts();
  void writeInputStage(Stage stage);
  void writeMeasureStmts();
  void writeMeasureStmt(const Pin *pin);
  void writeGateStage(Stage stage);
  void writeStageVoltageSources(LibertyCell *cell,
				StringVector *spice_port_names,
				const Instance *inst,
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
  const char *spiceTrans(const TransRiseFall *tr);
  void writeMeasureDelayStmt(Stage stage,
			     Path *from_path,
			     Path *to_path);
  void writeMeasureSlewStmt(Stage stage,
			    Path *path);
  void sensitizationValues(const Instance *inst,
			   FuncExpr *expr,
			   LibertyPort *from_port,
			   // Return values.
			   LibertyPortLogicValues &port_values);

  // Stage "accessors".
  //
  //           stage
  //      |---------------|
  //        |\             |\
  // -------| >---/\/\/----| >---
  //  gate  |/ drvr    load|/
  //  input
  //
  // A path from an input port has no GateInputPath.
  // Internally a stage index from stageFirst() to stageLast()
  // is turned into an index into path_expanded_.
  //
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
  const char *stageGateInputPinName(Stage stage);
  const char *stageDrvrPinName(Stage stage);
  const char *stageLoadPinName(Stage stage);

  Path *path_;
  const char *spice_filename_;
  const char *subckt_filename_;
  const char *lib_subckt_filename_;
  const char *model_filename_;
  const char *power_name_;
  const char *gnd_name_;

  ofstream spice_stream_;
  PathExpanded path_expanded_;
  CellSpicePortNames cell_spice_port_names_;
  ParasiticNodeMap node_map_;
  int next_node_index_;
  const char *net_name_;
  float power_voltage_;
  float gnd_voltage_;
  // Resistance to use to simulate a short circuit between spice nodes.
  float short_ckt_resistance_;
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
writePathSpice(Path *path,
	       const char *spice_filename,
	       const char *subckt_filename,
	       const char *lib_subckt_filename,
	       const char *model_filename,
	       const char *power_name,
	       const char *gnd_name,
	       StaState *sta)
{
  WritePathSpice writer(path, spice_filename, subckt_filename,
			lib_subckt_filename, model_filename,
			power_name, gnd_name, sta);
  writer.writeSpice();
}

WritePathSpice::WritePathSpice(Path *path,
			       const char *spice_filename,
			       const char *subckt_filename,
			       const char *lib_subckt_filename,
			       const char *model_filename,
			       const char *power_name,
			       const char *gnd_name,
			       const StaState *sta) :
  StaState(sta),
  path_(path),
  spice_filename_(spice_filename),
  subckt_filename_(subckt_filename),
  lib_subckt_filename_(lib_subckt_filename),
  model_filename_(model_filename),
  power_name_(power_name),
  gnd_name_(gnd_name),
  path_expanded_(sta),
  net_name_(NULL),
  short_ckt_resistance_(.0001)
{
  auto lib = network_->defaultLibertyLibrary();
  power_voltage_ = lib->supplyVoltage(power_name_);
  gnd_voltage_ = lib->supplyVoltage(gnd_name_);
}

WritePathSpice::~WritePathSpice()
{
  stringDelete(net_name_);
  cell_spice_port_names_.deleteContents();
}

void
WritePathSpice::writeSpice()
{
  spice_stream_.open(spice_filename_);
  if (spice_stream_.is_open()) {
    path_expanded_.expand(path_, true);
    // Find subckt port names as a side-effect of writeSubckts.
    writeSubckts();
    writeHeader();
    writeStageInstances();
    writeMeasureStmts();
    writeInputSource();
    writeStageSubckts();
    streamPrint(spice_stream_, ".end\n");
    spice_stream_.close();
  }
  else
    throw FileNotWritable(spice_filename_);
}

void
WritePathSpice::writeHeader()
{
  auto min_max = path_->minMax(this);
  auto pvt = sdc_->operatingConditions(min_max);
  if (pvt == NULL)
    pvt = network_->defaultLibertyLibrary()->defaultOperatingConditions();
  auto temp = pvt->temperature();
  streamPrint(spice_stream_, ".temp %.1f\n", temp);
  streamPrint(spice_stream_, ".include \"%s\"\n", model_filename_);
  streamPrint(spice_stream_, ".include \"%s\"\n", subckt_filename_);

  auto max_time = maxTime();
  auto time_step = max_time / 1e+3;
  streamPrint(spice_stream_, ".tran %.3g %.3g\n\n",
	      time_step, max_time);
}

float
WritePathSpice::maxTime()
{
  auto input_stage = stageFirst();
  auto input_path = stageDrvrPath(input_stage);
  auto input_slew = input_path->slew(this);
  auto end_slew = path_->slew(this);
  auto max_time = delayAsFloat(input_slew
			       + path_->arrival(this)
			       + end_slew * 2) * 1.5;
  return max_time;
}

void
WritePathSpice::writeStageInstances()
{
  streamPrint(spice_stream_, "*****************\n");
  streamPrint(spice_stream_, "* Stage instances\n");
  streamPrint(spice_stream_, "*****************\n\n");

  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    auto stage_name = stageName(stage).c_str();
    if (stage == stageFirst())
      streamPrint(spice_stream_, "x%s %s %s %s\n",
		  stage_name,
		  stageDrvrPinName(stage),
		  stageLoadPinName(stage),
		  stage_name);
    else 
      streamPrint(spice_stream_, "x%s %s %s %s %s\n",
		  stage_name,
		  stageGateInputPinName(stage),
		  stageDrvrPinName(stage),
		  stageLoadPinName(stage),
		  stage_name);
  }
  streamPrint(spice_stream_, "\n");
}

float
WritePathSpice::pgPortVoltage(const char *pg_port_name,
			      LibertyCell *cell)
{
  auto pg_port = cell->findPgPort(pg_port_name);
  return pgPortVoltage(pg_port);
}

float
WritePathSpice::pgPortVoltage(LibertyPgPort *pg_port)
{
  auto cell = pg_port->cell();
  auto voltage_name = pg_port->voltageName();
  auto lib = cell->libertyLibrary();
  auto voltage = lib->supplyVoltage(voltage_name);
  return voltage;
}

void
WritePathSpice::writeInputSource()
{
  streamPrint(spice_stream_, "**************\n");
  streamPrint(spice_stream_, "* Input source\n");
  streamPrint(spice_stream_, "**************\n\n");

  auto input_stage = stageFirst();
  streamPrint(spice_stream_, "v1 %s 0 pwl(\n",
	      stageDrvrPinName(input_stage));
  auto wire_arc = stageWireArc(input_stage);
  float volt0, volt1;
  if (wire_arc->fromTrans()->asRiseFall() == TransRiseFall::rise()) {
    volt0 = gnd_voltage_;
    volt1 = power_voltage_;
  }
  else {
    volt0 = power_voltage_;
    volt1 = gnd_voltage_;
  }
  auto input_path = stageDrvrPath(input_stage);
  auto input_slew = delayAsFloat(input_path->slew(this));
  if (input_slew == 0.0)
    input_slew = maxTime() / 1e+3;
  // Arbitrary offset.
  auto time0 = input_slew;
  auto time1 = time0 + input_slew;
  streamPrint(spice_stream_, "+%.3e %.3e\n", 0.0, volt0);
  streamPrint(spice_stream_, "+%.3e %.3e\n", time0, volt0);
  streamPrint(spice_stream_, "+%.3e %.3e\n", time1, volt1);
  streamPrint(spice_stream_, "+%.3e %.3e\n", maxTime(), volt1);
  streamPrint(spice_stream_, "+)\n\n");
}

void
WritePathSpice::writeMeasureStmts()
{
  streamPrint(spice_stream_, "********************\n");
  streamPrint(spice_stream_, "* Measure statements\n");
  streamPrint(spice_stream_, "********************\n\n");

  for (auto stage = stageFirst(); stage <= stageLast(); stage++) {
    auto gate_input_path = stageGateInputPath(stage);
    auto drvr_path = stageDrvrPath(stage);
    auto load_path = stageLoadPath(stage);
    if (gate_input_path) {
      // gate input -> gate output
      writeMeasureSlewStmt(stage, gate_input_path);
      writeMeasureDelayStmt(stage, gate_input_path, drvr_path);
    }
    writeMeasureSlewStmt(stage, drvr_path);
    // gate output | input port -> load
    writeMeasureDelayStmt(stage, drvr_path, load_path);
    if (stage == stageLast())
      writeMeasureSlewStmt(stage, load_path);
  }
  streamPrint(spice_stream_, "\n");
}

void
WritePathSpice::writeMeasureDelayStmt(Stage stage,
				      Path *from_path,
				      Path *to_path)
{
  auto lib = network_->defaultLibertyLibrary();
  auto from_pin_name = network_->pathName(from_path->pin(this));
  auto from_tr = from_path->transition(this);
  auto from_threshold = power_voltage_ * lib->inputThreshold(from_tr);

  auto to_pin_name = network_->pathName(to_path->pin(this));
  auto to_tr = to_path->transition(this);
  auto to_threshold = power_voltage_ * lib->inputThreshold(to_tr);
  streamPrint(spice_stream_,
	      ".measure tran %s_%s_delay_%s\n",
	      stageName(stage).c_str(),
	      from_pin_name,
	      to_pin_name);
  streamPrint(spice_stream_,
	      "+trig v(%s) val=%.3f %s=last\n",
	      from_pin_name,
	      from_threshold,
	      spiceTrans(from_tr));
  streamPrint(spice_stream_,
	      "+targ v(%s) val=%.3f %s=last\n",
	      to_pin_name,
	      to_threshold,
	      spiceTrans(to_tr));
}

void
WritePathSpice::writeMeasureSlewStmt(Stage stage,
				     Path *path)
{
  auto lib = network_->defaultLibertyLibrary();
  auto pin_name = network_->pathName(path->pin(this));
  auto tr = path->transition(this);
  auto spice_tr = spiceTrans(tr);
  auto lower = power_voltage_ * lib->slewLowerThreshold(tr);
  auto upper = power_voltage_ * lib->slewUpperThreshold(tr);
  float threshold1, threshold2;
  if (tr == TransRiseFall::rise()) {
    threshold1 = lower;
    threshold2 = upper;
  }
  else {
    threshold1 = upper;
    threshold2 = lower;
  }
  streamPrint(spice_stream_,
	      ".measure tran %s_%s_slew\n",
	      stageName(stage).c_str(),
	      pin_name);
  streamPrint(spice_stream_,
	      "+trig v(%s) val=%.3f %s=last\n",
	      pin_name,
	      threshold1,
	      spice_tr);
  streamPrint(spice_stream_,
	      "+targ v(%s) val=%.3f %s=last\n",
	      pin_name,
	      threshold2,
	      spice_tr);
}

const char *
WritePathSpice::spiceTrans(const TransRiseFall *tr)
{
  if (tr == TransRiseFall::rise())
    return "RISE";
  else
    return "FALL";
}

void
WritePathSpice::writeStageSubckts()
{
  streamPrint(spice_stream_, "***************\n");
  streamPrint(spice_stream_, "* Stage subckts\n");
  streamPrint(spice_stream_, "***************\n\n");

  for (auto stage = stageFirst(); stage <= stageLast(); stage++) {
    if (stage == stageFirst())
      writeInputStage(stage);
    else
      writeGateStage(stage);
  }
}

// Input port to first gate input.
void
WritePathSpice::writeInputStage(Stage stage)
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
WritePathSpice::writeGateStage(Stage stage)
{
  auto input_pin = stageInputPin(stage);
  auto input_pin_name = stageGateInputPinName(stage);
  auto drvr_pin = stageDrvrPin(stage);
  auto drvr_pin_name = stageDrvrPinName(stage);
  auto load_pin_name = stageLoadPinName(stage);
  streamPrint(spice_stream_, ".subckt stage%d %s %s %s\n",
	      stage,
	      input_pin_name,
	      drvr_pin_name,
	      load_pin_name);
  auto inst = network_->instance(input_pin);
  auto inst_name = network_->pathName(inst);
  auto cell = network_->libertyCell(inst);
  auto cell_name = cell->name();
  auto spice_port_names = cell_spice_port_names_[cell_name];

  // Instance subckt call.
  streamPrint(spice_stream_, "x%s", inst_name);
  StringVector::Iterator port_iter(spice_port_names);
  while (port_iter.hasNext()) {
    auto subckt_port_name = port_iter.next().c_str();
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
			   inst, inst_name,
			   network_->libertyPort(input_pin),
			   network_->libertyPort(drvr_pin));
  writeStageParasitics(stage);
  streamPrint(spice_stream_, ".ends\n\n");
}

// Power/ground and input voltage sources.
void
WritePathSpice::writeStageVoltageSources(LibertyCell *cell,
					 StringVector *spice_port_names,
					 const Instance *inst,
					 const char *inst_name,
					 LibertyPort *from_port,
					 LibertyPort *drvr_port)
{
  auto from_port_name = from_port->name();
  auto drvr_port_name = drvr_port->name();
  auto lib = cell->libertyLibrary();
  LibertyPortLogicValues port_values;
  sensitizationValues(inst, drvr_port->function(), from_port, port_values);
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
      auto port = cell->findLibertyPort(subckt_port_name);
      if (port) {
	const char *pg_port_name = NULL;
	const Pin *pin = network_->findPin(inst, port);
	// Look for tie high/low or propagated constant values.
	LogicValue port_value = sim_->logicValue(pin);
	if (port_value == logic_unknown) {
	  bool has_value;
	  LogicValue value;
	  port_values.findKey(port, value, has_value);
	  if (has_value)
	    port_value = value;
	}
	if (port_value == logic_zero)
	  pg_port_name = port->relatedGroundPin();
	else if (port_value == logic_one)
	  pg_port_name = port->relatedPowerPin();
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

// Find the logic values for expression inputs to enable paths from_port.
void
WritePathSpice::sensitizationValues(const Instance *inst,
				    FuncExpr *expr,
				    LibertyPort *from_port,
				    // Return values.
				    LibertyPortLogicValues &port_values)
{
  auto left = expr->left();
  auto right = expr->right();
  switch (expr->op()) {
  case FuncExpr::op_port:
    break;
  case FuncExpr::op_not:
    sensitizationValues(inst, expr->left(), from_port, port_values);
    break;
  case FuncExpr::op_or:
    if (left->hasPort(from_port)
	&& right->op() == FuncExpr::op_port)
      port_values[right->port()] = logic_zero;
    else if (left->hasPort(from_port)
	     && right->op() == FuncExpr::op_not
	     && right->left()->op() == FuncExpr::op_port)
	// from_port + !right_port
	port_values[right->left()->port()] = logic_one;
    else if (right->hasPort(from_port)
	     && left->op() == FuncExpr::op_port)
	port_values[left->port()] = logic_zero;
    else if (right->hasPort(from_port)
	     && left->op() == FuncExpr::op_not
	     && left->left()->op() == FuncExpr::op_port)
      // from_port + !left_port
      port_values[left->left()->port()] = logic_one;
    else {
      sensitizationValues(inst, expr->left(), from_port, port_values);
      sensitizationValues(inst, expr->right(), from_port, port_values);
    }
    break;
  case FuncExpr::op_and:
    if (left->hasPort(from_port)
	&& right->op() == FuncExpr::op_port)
	port_values[right->port()] = logic_one;
    else if (left->hasPort(from_port)
	     && right->op() == FuncExpr::op_not
	     && right->left()->op() == FuncExpr::op_port)
      // from_port * !right_port
      port_values[right->left()->port()] = logic_zero;
    else if (right->hasPort(from_port)
	     && left->op() == FuncExpr::op_port)
      port_values[left->port()] = logic_one;
    else if (right->hasPort(from_port)
	     && left->op() == FuncExpr::op_not
	     && left->left()->op() == FuncExpr::op_port)
      // from_port * !left_port
      port_values[left->left()->port()] = logic_zero;
    else {
      sensitizationValues(inst, expr->left(), from_port, port_values);
      sensitizationValues(inst, expr->right(), from_port, port_values);
    }
    break;
  case FuncExpr::op_xor:
    // Need to know timing arc sense to get this right.
    if (left->port() == from_port
	&& right->op() == FuncExpr::op_port)
      port_values[right->port()] = logic_zero;
    else if (right->port() == from_port
	     && left->op() == FuncExpr::op_port)
      port_values[left->port()] = logic_zero;
    else {
      sensitizationValues(inst, expr->left(), from_port, port_values);
      sensitizationValues(inst, expr->right(), from_port, port_values);
    }
    break;
  case FuncExpr::op_one:
  case FuncExpr::op_zero:
    break;
  }
}

class ParasiticNodeNameLess
{
public:
  ParasiticNodeNameLess(Parasitics *parasitics);
  bool operator()(const ParasiticNode *node1,
		  const ParasiticNode *node2) const;

private:
  Parasitics *parasitics_;
};

ParasiticNodeNameLess::ParasiticNodeNameLess(Parasitics *parasitics) :
  parasitics_(parasitics)
{
}

bool
ParasiticNodeNameLess::operator()(const ParasiticNode *node1,
				  const ParasiticNode *node2) const
{
  return stringLess(parasitics_->name(node1),
		    parasitics_->name(node2));
}

typedef Set<ParasiticDevice*> ParasiticDeviceSet;
// Less uses names rather than pointers for stable results.
typedef Set<ParasiticNode*, ParasiticNodeNameLess> ParasiticNodeSet;

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
      if (other_node)
	findParasiticDevicesNodes(other_node, parasitics, nodes, devices);
    }
  }
  delete device_iter;
}

void
WritePathSpice::writeStageParasitics(Stage stage)
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
    auto net = network_->net(drvr_pin);
    auto net_name =
      net ? network_->pathName(net) : network_->pathName(drvr_pin);
    initNodeMap(net_name);
    streamPrint(spice_stream_, "* Net %s\n", net_name);
    auto node = parasitics_->findNode(parasitic, drvr_pin);
    ParasiticNodeNameLess node_name_less(parasitics_);
    ParasiticNodeSet nodes(node_name_less);
    ParasiticDeviceSet devices;
    findParasiticDevicesNodes(node, parasitics_, nodes, devices);
    ParasiticDeviceSet::Iterator device_iter(devices);
    while (device_iter.hasNext()) {
      auto device = device_iter.next();
      auto resistance = parasitics_->value(device, parasitic_ap);
      if (parasitics_->isResistor(device)) {
	ParasiticNode *node1 = parasitics_->node1(device);
	ParasiticNode *node2 = parasitics_->node2(device);
	streamPrint(spice_stream_, "R%d %s %s %.3e\n",
		    resistor_index,
		    nodeName(node1),
		    nodeName(node2),
		    resistance);
	resistor_index++;
      }
      else if (parasitics_->isCouplingCap(device)) {
	// Ground coupling caps for now.
	ParasiticNode *node1 = 	parasitics_->node1(device);
	auto cap = parasitics_->value(device, parasitic_ap);
	streamPrint(spice_stream_, "C%d %s 0 %.3e\n",
		    cap_index,
		    nodeName(node1),
		    cap);
	cap_index++;
      }
    }
    ParasiticNodeSet::Iterator node_iter(nodes);
    while (node_iter.hasNext()) {
      auto node = node_iter.next();
      auto cap = parasitics_->nodeGndCap(node, parasitic_ap);
      if (cap > 0.0) {
	streamPrint(spice_stream_, "C%d %s 0 %.3e\n",
		    cap_index,
		    nodeName(node),
		    cap);
	cap_index++;
      }
    }
  }
  else {
    streamPrint(spice_stream_, "* No parasitics found for this net.\n");
    streamPrint(spice_stream_, "R1 %s %s %.3e\n",
		network_->pathName(drvr_pin),
		network_->pathName(load_pin),
		short_ckt_resistance_);
  }
}

void
WritePathSpice::initNodeMap(const char *net_name)
{
  stringDelete(net_name_);
  node_map_.clear();
  next_node_index_ = 1;
  net_name_ = stringCopy(net_name);
}

const char *
WritePathSpice::nodeName(ParasiticNode *node)
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
    return stringPrintTmp("%s/%d", net_name_, node_index);
  }
}

////////////////////////////////////////////////////////////////

// Copy the subckt definition from lib_subckt_filename for
// each cell in path to path_subckt_filename.
void
WritePathSpice::writeSubckts()
{
  StringSet path_cell_names;
  findPathCellnames(path_cell_names);

  ifstream lib_subckts_stream(lib_subckt_filename_);
  if (lib_subckts_stream.is_open()) {
    ofstream subckts_stream(subckt_filename_);
    if (subckts_stream.is_open()) {
      string line;
      while (getline(lib_subckts_stream, line)) {
	// .subckt <cell_name> [args..]
	StringVector tokens;
	split(line, " \t", tokens);
	if (tokens.size() >= 2
	    && stringEqual(tokens[0].c_str(), ".subckt")) {
	  auto cell_name = tokens[1].c_str();
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
	      throw SubcktEndsMissing(cell_name, lib_subckt_filename_);
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
		       lib_subckt_filename_);
	while (cell_iter.hasNext()) {
	  auto cell_name = cell_iter.next();
	  report_->printError(" %s\n", cell_name);
	}
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
WritePathSpice::findPathCellnames(// Return values.
				  StringSet &path_cell_names)
{
  for (auto stage = stageFirst(); stage <= stageLast(); stage++) {
    auto arc = stageGateArc(stage);
    if (arc) {
      auto cell = arc->set()->libertyCell();
      if (cell) {
	debugPrint1(debug_, "write_spice", 2, "cell %s\n", cell->name());
	path_cell_names.insert(cell->name());
      }
    }
  }
}

void
WritePathSpice::recordSpicePortNames(const char *cell_name,
				     StringVector &tokens)
{
  auto cell = network_->findLibertyCell(cell_name);
  if (cell) {
    auto spice_port_names = new StringVector;
    for (auto i = 2; i < tokens.size(); i++) {
      auto port_name = tokens[i].c_str();
      auto port = cell->findLibertyPort(port_name);
      auto pg_port = cell->findPgPort(port_name);
      if (port == NULL && pg_port == NULL)
	report_->error("subckt %s port %s has no corresponding liberty port or pg_port.\n",
		       cell_name, port_name);
      spice_port_names->push_back(port_name);
    }
    cell_spice_port_names_[cell_name] = spice_port_names;
  }
}

////////////////////////////////////////////////////////////////

Stage
WritePathSpice::stageFirst()
{
  return 1;
}

Stage
WritePathSpice::stageLast()
{
  return (path_expanded_.size() + 1) / 2;
}

string
WritePathSpice::stageName(Stage stage)
{
  string name;
  stringPrint(name, "stage%d", stage);
  return name;
}

int
WritePathSpice::stageGateInputPathIndex(Stage stage)
{
  return stage * 2 - 3;
}

int
WritePathSpice::stageDrvrPathIndex(Stage stage)
{
  return stage * 2 - 2;
}

int
WritePathSpice::stageLoadPathIndex(Stage stage)
{
  return stage * 2 - 1;
}

PathRef *
WritePathSpice::stageGateInputPath(Stage stage)
{
  auto path_index = stageGateInputPathIndex(stage);
  return path_expanded_.path(path_index);
}

PathRef *
WritePathSpice::stageDrvrPath(Stage stage)
{
  auto path_index = stageDrvrPathIndex(stage);
  return path_expanded_.path(path_index);
}

PathRef *
WritePathSpice::stageLoadPath(Stage stage)
{
  auto path_index = stageLoadPathIndex(stage);
  return path_expanded_.path(path_index);
}

TimingArc *
WritePathSpice::stageGateArc(Stage stage)
{
  auto path_index = stageDrvrPathIndex(stage);
  if (path_index >= 0)
    return path_expanded_.prevArc(path_index);
  else
    return NULL;
}

TimingArc *
WritePathSpice::stageWireArc(Stage stage)
{
  auto path_index = stageLoadPathIndex(stage);
  return path_expanded_.prevArc(path_index);
}

Edge *
WritePathSpice::stageGateEdge(Stage stage)
{
  auto path = stageGateInputPath(stage);
  auto arc = stageGateArc(stage);
  return path->prevEdge(arc, this);
}

Edge *
WritePathSpice::stageWireEdge(Stage stage)
{
  auto path = stageLoadPath(stage);
  auto arc = stageWireArc(stage);
  return path->prevEdge(arc, this);
}

Pin *
WritePathSpice::stageInputPin(Stage stage)
{
  auto path = stageGateInputPath(stage);
  return path->pin(this);
}

Pin *
WritePathSpice::stageDrvrPin(Stage stage)
{
  auto path = stageDrvrPath(stage);
  return path->pin(this);
}

Pin *
WritePathSpice::stageLoadPin(Stage stage)
{
  auto path = stageLoadPath(stage);
  return path->pin(this);
}

const char *
WritePathSpice::stageGateInputPinName(Stage stage)
{
  auto pin = stageInputPin(stage);
  return network_->pathName(pin);
}

const char *
WritePathSpice::stageDrvrPinName(Stage stage)
{
  auto pin = stageDrvrPin(stage);
  return network_->pathName(pin);
}

const char *
WritePathSpice::stageLoadPinName(Stage stage)
{
  auto pin = stageLoadPin(stage);
  return network_->pathName(pin);
}

////////////////////////////////////////////////////////////////

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

} // namespace
