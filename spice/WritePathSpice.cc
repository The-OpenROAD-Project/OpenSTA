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

#include "WritePathSpice.hh"

#include <string>
#include <fstream>

#include "Debug.hh"
#include "Error.hh"
#include "Report.hh"
#include "StringUtil.hh"
#include "FuncExpr.hh"
#include "Units.hh"
#include "Sequential.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "PortDirection.hh"
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
#include "search/Sim.hh"
#include "WriteSpice.hh"

namespace sta {

using std::ofstream;
using std::ifstream;
using std::max;

typedef int Stage;

////////////////////////////////////////////////////////////////

class WritePathSpice : public WriteSpice
{
public:
  WritePathSpice(Path *path,
		 const char *spice_filename,
                 const char *subckt_filename,
		 const char *lib_subckt_filename,
		 const char *model_filename,
		 const char *power_name,
		 const char *gnd_name,
                 CircuitSim ckt_sim,
		 const StaState *sta);
  void writeSpice();

private:
  void writeHeader();
  void writePrintStmt();
  void writeStageInstances();
  void writeInputSource();
  void writeStageSubckts();
  void writeInputStage(Stage stage);
  void writeMeasureStmts();
  void writeMeasureStmt(const Pin *pin);
  void writeGateStage(Stage stage);
  void writeStageParasitics(Stage stage);
  void writeSubckts();
  StdStringSet findPathCellNames();
  void findPathCellSubckts(StdStringSet &path_cell_names);
  float maxTime();
  float pathMaxTime();
  void writeMeasureDelayStmt(Stage stage,
			     const Path *from_path,
			     const Path *to_path);
  void writeMeasureSlewStmt(Stage stage,
			    const Path *path);
  void writeInputWaveform();
  void writeClkWaveform();

  // Stage "accessors".
  //
  //           stage
  //      |---------------|
  //        |\             |\   .
  // -------| >---/\/\/----| >---
  //  gate  |/ drvr    load|/
  //  input
  //
  // A path from an input port has no GateInputPath (the input port is the drvr).
  // Internally a stage index from stageFirst() to stageLast()
  // is turned into an index into path_expanded_.
  //
  Stage stageFirst();
  Stage stageLast();
  string stageName(Stage stage);
  int stageGateInputPathIndex(Stage stage);
  int stageDrvrPathIndex(Stage stage);
  int stageLoadPathIndex(Stage stage);
  const PathRef *stageGateInputPath(Stage stage);
  const PathRef *stageDrvrPath(Stage stage);
  const PathRef *stageLoadPath(Stage stage);
  TimingArc *stageGateArc(Stage stage);
  TimingArc *stageWireArc(Stage stage);
  Edge *stageGateEdge(Stage stage);
  Edge *stageWireEdge(Stage stage);
  Pin *stageGateInputPin(Stage stage);
  Pin *stageDrvrPin(Stage stage);
  LibertyPort *stageGateInputPort(Stage stage);
  LibertyPort *stageDrvrPort(Stage stage);
  Pin *stageLoadPin(Stage stage);
  const char *stageGateInputPinName(Stage stage);
  const char *stageDrvrPinName(Stage stage);
  const char *stageLoadPinName(Stage stage);
  LibertyCell *stageLibertyCell(Stage stage);
  Instance *stageInstance(Stage stage);

  float findSlew(const Path *path);
  float findSlew(const Path *path,
		 const RiseFall *rf,
		 TimingArc *next_arc);
  Path *path_;
  PathExpanded path_expanded_;
  // Input clock waveform cycles.
  int clk_cycle_count_;

  InstanceSet written_insts_;

  using WriteSpice::writeHeader;
  using WriteSpice::writePrintStmt;
  using WriteSpice::writeSubckts;
  using WriteSpice::writeVoltageSource;
  using WriteSpice::writeMeasureDelayStmt;
  using WriteSpice::writeMeasureSlewStmt;
  using WriteSpice::findSlew;
};

////////////////////////////////////////////////////////////////

void
writePathSpice(Path *path,
	       const char *spice_filename,
	       const char *subckt_filename,
	       const char *lib_subckt_filename,
	       const char *model_filename,
               const char *power_name,
	       const char *gnd_name,
               CircuitSim ckt_sim,
	       StaState *sta)
{
  if (sta->network()->defaultLibertyLibrary() == nullptr)
    sta->report()->error(1600, "No liberty libraries found,");
  WritePathSpice writer(path, spice_filename, subckt_filename,
                        lib_subckt_filename, model_filename,
                        power_name, gnd_name, ckt_sim, sta);
  writer.writeSpice();
}

WritePathSpice::WritePathSpice(Path *path,
                               const char *spice_filename,
			       const char *subckt_filename,
			       const char *lib_subckt_filename,
			       const char *model_filename,
			       const char *power_name,
			       const char *gnd_name,
                               CircuitSim ckt_sim,
			       const StaState *sta) :
  WriteSpice(spice_filename, subckt_filename, lib_subckt_filename,
             model_filename, power_name, gnd_name, ckt_sim,
             path->dcalcAnalysisPt(sta), sta),
  path_(path),
  path_expanded_(sta),
  clk_cycle_count_(3),
  written_insts_(network_)
{
  initPowerGnd();
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
    writePrintStmt();
    if (ckt_sim_ == CircuitSim::hspice)
      writeMeasureStmts();
    writeInputSource();
    writeStageInstances();
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
  Path *start_path = path_expanded_.startPath();
  string title = stdstrPrint("Path from %s %s to %s %s",
                             network_->pathName(start_path->pin(this)),
                             start_path->transition(this)->asString(),
                             network_->pathName(path_->pin(this)),
                             path_->transition(this)->asString());
  float max_time = maxTime();
  float time_step = 1e-13;
  writeHeader(title, max_time, time_step);
}

void
WritePathSpice::writePrintStmt()
{
  StdStringSeq node_names;
  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    node_names.push_back(stageDrvrPinName(stage));
    node_names.push_back(stageLoadPinName(stage));
  }
  writePrintStmt(node_names);
}

float
WritePathSpice::maxTime()
{
  Stage input_stage = stageFirst();
  const PathRef *input_path = stageDrvrPath(input_stage);
  if (input_path->isClock(this)) {
    const Clock *clk = input_path->clock(this);
    float period = clk->period();
    float first_edge_offset = period / 10;
    float max_time = period * clk_cycle_count_ + first_edge_offset;
    return max_time;
  }
  else
    return pathMaxTime();
}

// Make sure run time is long enough to see side load transitions along the path.
float
WritePathSpice::pathMaxTime()
{
  float max_time = 0.0;
  for (size_t i = 0; i < path_expanded_.size(); i++) {
    const PathRef *path = path_expanded_.path(i);
    const RiseFall *rf = path->transition(this);
    Vertex *vertex = path->vertex(this);
    float path_max_slew = railToRailSlew(findSlew(vertex,rf,nullptr), rf);
    if (vertex->isDriver(network_)) {
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        Vertex *load = edge->to(graph_);
        float load_slew = railToRailSlew(findSlew(load, rf, nullptr), rf);
        if (load_slew > path_max_slew)
          path_max_slew = load_slew;
      }
    }
    float path_max_time = delayAsFloat(path->arrival(this)) + path_max_slew * 2.0;
    if (path_max_time > max_time)
      max_time = path_max_time;
  }
  return max_time;
}

void
WritePathSpice::writeStageInstances()
{
  streamPrint(spice_stream_, "*****************\n");
  streamPrint(spice_stream_, "* Stage instances\n");
  streamPrint(spice_stream_, "*****************\n\n");

  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    string stage_name = stageName(stage);
    const char *stage_cname = stage_name.c_str();
    if (stage == stageFirst())
      streamPrint(spice_stream_, "x%s %s %s %s\n",
		  stage_cname,
		  stageDrvrPinName(stage),
		  stageLoadPinName(stage),
		  stage_cname);
    else {
      streamPrint(spice_stream_, "x%s %s %s %s %s\n",
		  stage_cname,
		  stageGateInputPinName(stage),
		  stageDrvrPinName(stage),
		  stageLoadPinName(stage),
                  stage_cname);
    }
  }
  streamPrint(spice_stream_, "\n");
}

void
WritePathSpice::writeInputSource()
{
  streamPrint(spice_stream_, "**************\n");
  streamPrint(spice_stream_, "* Input source\n");
  streamPrint(spice_stream_, "**************\n\n");

  Stage input_stage = stageFirst();
  const PathRef *input_path = stageDrvrPath(input_stage);
  if (input_path->isClock(this))
    writeClkWaveform();
  else
    writeInputWaveform();
  streamPrint(spice_stream_, "\n");
}

void
WritePathSpice::writeInputWaveform()
{
  Stage input_stage = stageFirst();
  const PathRef *input_path = stageDrvrPath(input_stage);
  const RiseFall *rf = input_path->transition(this);
  TimingArc *next_arc = stageGateArc(input_stage + 1);
  float slew0 = findSlew(input_path, rf, next_arc);

  float threshold = default_library_->inputThreshold(rf);
  float dt = railToRailSlew(slew0, rf);
  float time0 = dt * threshold;

  const Pin *drvr_pin = stageDrvrPin(input_stage);
  const Pin *load_pin = stageLoadPin(input_stage);
  const LibertyPort *load_port = network_->libertyPort(load_pin);
  DriverWaveform *drvr_waveform = nullptr;
  if (load_port)
    drvr_waveform = load_port->driverWaveform(rf);
  if (drvr_waveform)
    writeWaveformVoltSource(drvr_pin, drvr_waveform, rf, 0.0, slew0);
  else
    writeRampVoltSource(drvr_pin, rf, time0, slew0);
}

void
WritePathSpice::writeClkWaveform()
{
  Stage input_stage = stageFirst();
  const PathRef *input_path = stageDrvrPath(input_stage);
  TimingArc *next_arc = stageGateArc(input_stage + 1);
  const ClockEdge *clk_edge = input_path->clkEdge(this);

  const Clock *clk = clk_edge->clock();
  float period = clk->period();
  float time_offset = clkWaveformTimeOffset(clk);
  RiseFall *rf0, *rf1;
  float volt0;
  if (clk_edge->time() < period) {
    rf0 = RiseFall::rise();
    rf1 = RiseFall::fall();
    volt0 = gnd_voltage_;
  }
  else {
    rf0 = RiseFall::fall();
    rf1 = RiseFall::rise();
    volt0 = power_voltage_;
  }
  float slew0 = findSlew(input_path, rf0, next_arc);
  float slew1 = findSlew(input_path, rf1, next_arc);
  streamPrint(spice_stream_, "v1 %s 0 pwl(\n",
	      stageDrvrPinName(input_stage));
  streamPrint(spice_stream_, "+%.3e %.3e\n", 0.0, volt0);
  for (int cycle = 0; cycle < clk_cycle_count_; cycle++) {
    float time0 = time_offset + cycle * period;
    float time1 = time0 + period / 2.0;
    writeWaveformEdge(rf0, time0, slew0);
    writeWaveformEdge(rf1, time1, slew1);
  }
  streamPrint(spice_stream_, "+%.3e %.3e\n", max_time_, volt0);
  streamPrint(spice_stream_, "+)\n");
}

float
WritePathSpice::findSlew(const Path *path)
{
  Vertex *vertex = path->vertex(this);
  const RiseFall *rf = path->transition(this);
  return findSlew(vertex, rf, nullptr);
}

float
WritePathSpice::findSlew(const Path *path,
                         const RiseFall *rf,
                         TimingArc *next_arc)
{
  Vertex *vertex = path->vertex(this);
  return findSlew(vertex, rf, next_arc);
}

////////////////////////////////////////////////////////////////

void
WritePathSpice::writeMeasureStmts()
{
  streamPrint(spice_stream_, "********************\n");
  streamPrint(spice_stream_, "* Measure statements\n");
  streamPrint(spice_stream_, "********************\n\n");

  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    const PathRef *gate_input_path = stageGateInputPath(stage);
    const PathRef *drvr_path = stageDrvrPath(stage);
    const PathRef *load_path = stageLoadPath(stage);
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
				      const Path *from_path,
				      const Path *to_path)
{
  writeMeasureDelayStmt(from_path->pin(this), from_path->transition(this),
                        to_path->pin(this), to_path->transition(this),
                        stageName(stage));
}

void
WritePathSpice::writeMeasureSlewStmt(Stage stage,
				     const Path *path)
{
  const Pin *pin = path->pin(this);
  const RiseFall *rf = path->transition(this);
  string prefix = stageName(stage);
  writeMeasureSlewStmt(pin, rf, prefix);
}

void
WritePathSpice::writeStageSubckts()
{
  streamPrint(spice_stream_, "***************\n");
  streamPrint(spice_stream_, "* Stage subckts\n");
  streamPrint(spice_stream_, "***************\n\n");

  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    cap_index_ = 1;
    res_index_ = 1;
    volt_index_ = 1;
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
  const char *drvr_pin_name = stageDrvrPinName(stage);
  const char *load_pin_name = stageLoadPinName(stage);
  string prefix = stageName(stage);
  streamPrint(spice_stream_, ".subckt %s %s %s\n",
	      prefix.c_str(),
	      drvr_pin_name,
	      load_pin_name);
  writeStageParasitics(stage);
  streamPrint(spice_stream_, ".ends\n\n");
}

// Gate and load parasitics.
void
WritePathSpice::writeGateStage(Stage stage)
{
  const Pin *input_pin = stageGateInputPin(stage);
  const char *input_pin_name = stageGateInputPinName(stage);
  const Pin *drvr_pin = stageDrvrPin(stage);
  const char *drvr_pin_name = stageDrvrPinName(stage);
  const Pin *load_pin = stageLoadPin(stage);
  const char *load_pin_name = stageLoadPinName(stage);
  string subckt_name = "stage" + std::to_string(stage);

  const Instance *inst = stageInstance(stage);
  LibertyPort *input_port = stageGateInputPort(stage);
  LibertyPort *drvr_port = stageDrvrPort(stage);

  streamPrint(spice_stream_, ".subckt %s %s %s %s\n",
	      subckt_name.c_str(),
	      input_pin_name,
	      drvr_pin_name,
	      load_pin_name);

  // Driver subckt call.
  streamPrint(spice_stream_, "* Gate %s %s -> %s\n",
	      network_->pathName(inst),
	      input_port->name(),
	      drvr_port->name());
  writeSubcktInst(inst);

  const PathRef *drvr_path = stageDrvrPath(stage);
  const RiseFall *drvr_rf = drvr_path->transition(this);
  Edge *gate_edge = stageGateEdge(stage);

  LibertyPortLogicValues port_values;
  bool is_clked;
  gatePortValues(input_pin, drvr_pin, drvr_rf, gate_edge,
                 port_values, is_clked);

  PinSet inputs(network_);
  inputs.insert(input_pin);
  writeSubcktInstVoltSrcs(inst, port_values, inputs);
  streamPrint(spice_stream_, "\n");

  PinSet drvr_loads(network_);
  PinConnectedPinIterator *pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    const Pin *load_pin = pin_iter->next();
    drvr_loads.insert(load_pin);
  }
  delete pin_iter;

  writeSubcktInstLoads(drvr_pin, load_pin, drvr_loads, written_insts_);
  writeStageParasitics(stage);
  streamPrint(spice_stream_, ".ends\n\n");
}

void
WritePathSpice::writeStageParasitics(Stage stage)
{
  const PathRef *drvr_path = stageDrvrPath(stage);
  DcalcAnalysisPt *dcalc_ap = drvr_path->dcalcAnalysisPt(this);
  ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
  const Pin *drvr_pin = stageDrvrPin(stage);
  const Parasitic *parasitic = parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
  if (parasitic == nullptr) {
    const RiseFall *drvr_rf = drvr_path->transition(this);
    parasitic = parasitics_->findPiElmore(drvr_pin, drvr_rf, parasitic_ap);
  }
  NetSet coupling_nets;
  writeDrvrParasitics(drvr_pin, parasitic, coupling_nets);
}

////////////////////////////////////////////////////////////////

// Copy the subckt definition from lib_subckt_filename for
// each cell in path to path_subckt_filename.
void
WritePathSpice::writeSubckts()
{
  StdStringSet cell_names = findPathCellNames();
  writeSubckts(cell_names);
}

StdStringSet
WritePathSpice::findPathCellNames()
{
  StdStringSet path_cell_names;
  for (Stage stage = stageFirst(); stage <= stageLast(); stage++) {
    TimingArc *arc = stageGateArc(stage);
    if (arc) {
      LibertyCell *cell = arc->set()->libertyCell();
      if (cell) {
	debugPrint(debug_, "write_spice", 2, "cell %s", cell->name());
	path_cell_names.insert(cell->name());
      }
      // Include side receivers.
      Pin *drvr_pin = stageDrvrPin(stage);
      auto pin_iter = network_->connectedPinIterator(drvr_pin);
      while (pin_iter->hasNext()) {
	const Pin *pin = pin_iter->next();
	LibertyPort *port = network_->libertyPort(pin);
	if (port) {
	  LibertyCell *cell = port->libertyCell();
	  path_cell_names.insert(cell->name());
	}
      }
      delete pin_iter;
    }
  }
  return path_cell_names;
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

const PathRef *
WritePathSpice::stageGateInputPath(Stage stage)
{
  int path_index = stageGateInputPathIndex(stage);
  return path_expanded_.path(path_index);
}

const PathRef *
WritePathSpice::stageDrvrPath(Stage stage)
{
  int path_index = stageDrvrPathIndex(stage);
  return path_expanded_.path(path_index);
}

const PathRef *
WritePathSpice::stageLoadPath(Stage stage)
{
  int path_index = stageLoadPathIndex(stage);
  return path_expanded_.path(path_index);
}

TimingArc *
WritePathSpice::stageGateArc(Stage stage)
{
  int path_index = stageDrvrPathIndex(stage);
  if (path_index >= 0)
    return path_expanded_.prevArc(path_index);
  else
    return nullptr;
}

TimingArc *
WritePathSpice::stageWireArc(Stage stage)
{
  int path_index = stageLoadPathIndex(stage);
  return path_expanded_.prevArc(path_index);
}

Edge *
WritePathSpice::stageGateEdge(Stage stage)
{
  const PathRef *path = stageDrvrPath(stage);
  TimingArc *arc = stageGateArc(stage);
  return path->prevEdge(arc, this);
}

Edge *
WritePathSpice::stageWireEdge(Stage stage)
{
  const PathRef *path = stageLoadPath(stage);
  TimingArc *arc = stageWireArc(stage);
  return path->prevEdge(arc, this);
}

Pin *
WritePathSpice::stageGateInputPin(Stage stage)
{
  const PathRef *path = stageGateInputPath(stage);
  return path->pin(this);
}

LibertyPort *
WritePathSpice::stageGateInputPort(Stage stage)
{
  Pin *pin = stageGateInputPin(stage);
  return network_->libertyPort(pin);
}

Pin *
WritePathSpice::stageDrvrPin(Stage stage)
{
  const PathRef *path = stageDrvrPath(stage);
  return path->pin(this);
}

LibertyPort *
WritePathSpice::stageDrvrPort(Stage stage)
{
  Pin *pin = stageDrvrPin(stage);
  return network_->libertyPort(pin);
}

Pin *
WritePathSpice::stageLoadPin(Stage stage)
{
  const PathRef *path = stageLoadPath(stage);
  return path->pin(this);
}

const char *
WritePathSpice::stageGateInputPinName(Stage stage)
{
  Pin *pin = stageGateInputPin(stage);
  return network_->pathName(pin);
}

const char *
WritePathSpice::stageDrvrPinName(Stage stage)
{
  Pin *pin = stageDrvrPin(stage);
  return network_->pathName(pin);
}

const char *
WritePathSpice::stageLoadPinName(Stage stage)
{
  Pin *pin = stageLoadPin(stage);
  return network_->pathName(pin);
}

Instance *
WritePathSpice::stageInstance(Stage stage)
{
  Pin *pin = stageDrvrPin(stage);
  return network_->instance(pin);
}

LibertyCell *
WritePathSpice::stageLibertyCell(Stage stage)
{
  Pin *pin = stageDrvrPin(stage);
  return network_->libertyPort(pin)->libertyCell();
}

} // namespace
