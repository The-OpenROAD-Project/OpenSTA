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

#include "WritePathSpice.hh"

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#include "Debug.hh"
#include "Error.hh"
#include "Format.hh"
#include "FuncExpr.hh"
#include "Graph.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Parasitics.hh"
#include "Path.hh"
#include "PathExpanded.hh"
#include "PortDirection.hh"
#include "Report.hh"
#include "Sdc.hh"
#include "Sequential.hh"
#include "StaState.hh"
#include "StringUtil.hh"
#include "TableModel.hh"
#include "TimingArc.hh"
#include "Units.hh"
#include "WriteSpice.hh"
#include "PathStages.hh"

namespace sta {

class WritePathSpice : public WriteSpice
{
public:
  WritePathSpice(const Path *path,
                 std::string_view spice_filename,
                 std::string_view subckt_filename,
                 StringSeq lib_subckt_filenames,
                 std::string_view model_filename,
                 std::string_view power_name,
                 std::string_view gnd_name,
                 CircuitSim ckt_sim,
                 const StaState *sta);
  void writeSpice();

private:
  void initPowerGnd();
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
  StringSet findPathCellNames();
  void findPathCellSubckts(StringSet &path_cell_names);
  float maxTime();
  float pathMaxTime();
  void writeMeasureDelayStmt(Stage stage,
                             const Path *from_path,
                             const Path *to_path);
  void writeMeasureSlewStmt(Stage stage,
                            const Path *path);
  void writeInputWaveform();
  void writeClkWaveform();

  float findSlew(const Path *path);
  float findSlew(const Path *path,
                 const RiseFall *rf,
                 const TimingArc *next_arc);
  const Path *path_;
  PathExpanded path_expanded_;
  PathStages stages_;
  // Input clock waveform cycles.
  int clk_cycle_count_{3};

  InstanceSet written_insts_;

  using WriteSpice::writeHeader;
  using WriteSpice::writePrintStmt;
  using WriteSpice::writeSubckts;
  using WriteSpice::writeVoltageSource;
  using WriteSpice::writeMeasureDelayStmt;
  using WriteSpice::writeMeasureSlewStmt;
  using WriteSpice::findSlew;
  using WriteSpice::initPowerGnd;
};

////////////////////////////////////////////////////////////////

void
writePathSpice(const Path *path,
               std::string_view spice_filename,
               std::string_view subckt_filename,
               StringSeq lib_subckt_filenames,
               std::string_view model_filename,
               std::string_view power_name,
               std::string_view gnd_name,
               CircuitSim ckt_sim,
               StaState *sta)
{
  WritePathSpice writer(path, spice_filename, subckt_filename,
                        std::move(lib_subckt_filenames), model_filename,
                        power_name, gnd_name, ckt_sim, sta);
  writer.writeSpice();
}

WritePathSpice::WritePathSpice(const Path *path,
                               std::string_view spice_filename,
                               std::string_view subckt_filename,
                               StringSeq lib_subckt_filenames,
                               std::string_view model_filename,
                               std::string_view power_name,
                               std::string_view gnd_name,
                               CircuitSim ckt_sim,
                               const StaState *sta) :
  WriteSpice(spice_filename, subckt_filename, std::move(lib_subckt_filenames),
             model_filename, power_name, gnd_name, ckt_sim,
             path->scene(sta), path->minMax(sta), sta),
  path_(path),
  path_expanded_(sta),
  stages_(path_expanded_, sta),
  written_insts_(network_)
{
}

void
WritePathSpice::writeSpice()
{
  spice_stream_.open(spice_filename_);
  if (spice_stream_.is_open()) {
    path_expanded_.expand(path_, true);

    initPowerGnd();
    // Find subckt port names as a side-effect of writeSubckts.
    writeSubckts();
    writeHeader();
    writePrintStmt();
    if (ckt_sim_ == CircuitSim::hspice)
      writeMeasureStmts();
    writeInputSource();
    writeStageInstances();
    writeStageSubckts();
    sta::print(spice_stream_, ".end\n");
    spice_stream_.close();
  }
  else
    throw FileNotWritable(spice_filename_);
}

void
WritePathSpice::initPowerGnd()
{
  Scene *scene = path_->scene(this);
  const MinMax *min_max = path_->minMax(this);
  LibertyLibrary *threshold_lib = nullptr;
  for (size_t i = 0; i < path_expanded_.size(); i++) {
    const Path *path = path_expanded_.path(i);
    const Pin *pin = path->pin(this);
    const LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      threshold_lib = port->scenePort(scene, min_max)->libertyLibrary();
      break;
    }
  }
  if (threshold_lib)
    initPowerGnd(threshold_lib);
  else
    report_->error(1606, "No instance with Liberty cell found in path.");
}

void
WritePathSpice::writeHeader()
{
  const Path *start_path = path_expanded_.startPath();
  std::string title = sta::format("Path from {} {} to {} {}",
                                 network_->pathName(start_path->pin(this)),
                                 start_path->transition(this)->shortName(),
                                 network_->pathName(path_->pin(this)),
                                 path_->transition(this)->shortName());
  float max_time = maxTime();
  float time_step = 1e-13;
  writeHeader(title, max_time, time_step);
}

void
WritePathSpice::writePrintStmt()
{
  StringSeq node_names;
  for (Stage stage : stages_) {
    node_names.push_back(stages_.stageDrvrPinName(stage));
    node_names.push_back(stages_.stageLoadPinName(stage));
  }
  writePrintStmt(node_names);
}

float
WritePathSpice::maxTime()
{
  Stage input_stage = stages_.stageFirst();
  const Path *input_path = stages_.stageDrvrPath(input_stage);
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
    const Path *path = path_expanded_.path(i);
    const RiseFall *rf = path->transition(this);
    Vertex *vertex = path->vertex(this);
    float path_max_slew = railToRailSlew(findSlew(vertex,rf,nullptr), rf);
    if (vertex->isDriver(network_)) {
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        Vertex *load = edge->to(graph_);
        float load_slew = railToRailSlew(findSlew(load, rf, nullptr), rf);
        path_max_slew = std::max(load_slew, path_max_slew);
      }
    }
    float path_max_time = delayAsFloat(path->arrival()) + path_max_slew * 2.0;
    max_time = std::max(path_max_time, max_time);
  }
  return max_time;
}

void
WritePathSpice::writeStageInstances()
{
  sta::print(spice_stream_, "*****************\n");
  sta::print(spice_stream_, "* Stage instances\n");
  sta::print(spice_stream_, "*****************\n\n");

  for (Stage stage : stages_) {
    std::string stage_name = stages_.stageName(stage);
    if (stage == stages_.stageFirst())
      sta::print(spice_stream_, "x{} {} {} {}\n",
                 stage_name,
                 stages_.stageDrvrPinName(stage),
                 stages_.stageLoadPinName(stage),
                 stage_name);
    else {
      sta::print(spice_stream_, "x{} {} {} {} {}\n",
                 stage_name,
                 stages_.stageGateInputPinName(stage),
                 stages_.stageDrvrPinName(stage),
                 stages_.stageLoadPinName(stage),
                 stage_name);
    }
  }
  sta::print(spice_stream_, "\n");
}

void
WritePathSpice::writeInputSource()
{
  sta::print(spice_stream_, "**************\n");
  sta::print(spice_stream_, "* Input source\n");
  sta::print(spice_stream_, "**************\n\n");

  Stage input_stage = stages_.stageFirst();
  const Path *input_path = stages_.stageDrvrPath(input_stage);
  if (input_path->isClock(this))
    writeClkWaveform();
  else
    writeInputWaveform();
  sta::print(spice_stream_, "\n");
}

void
WritePathSpice::writeInputWaveform()
{
  Stage input_stage = stages_.stageFirst();
  const Path *input_path = stages_.stageDrvrPath(input_stage);
  const RiseFall *rf = input_path->transition(this);
  const TimingArc *next_arc = stages_.stageGateArc(input_stage + 1);
  float slew0 = findSlew(input_path, rf, next_arc);

  float threshold = threshold_library_->inputThreshold(rf);
  float dt = railToRailSlew(slew0, rf);
  float time0 = dt * threshold;

  const Pin *drvr_pin = stages_.stageDrvrPin(input_stage);
  const Pin *load_pin = stages_.stageLoadPin(input_stage);
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
  Stage input_stage = stages_.stageFirst();
  const Path *input_path = stages_.stageDrvrPath(input_stage);
  const TimingArc *next_arc = stages_.stageGateArc(input_stage + 1);
  const ClockEdge *clk_edge = input_path->clkEdge(this);

  const Clock *clk = clk_edge->clock();
  float period = clk->period();
  float time_offset = 0.0;
  const RiseFall *rf0, *rf1;
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
  sta::print(spice_stream_, "v1 {} 0 pwl(\n",
             stages_.stageDrvrPinName(input_stage));
  sta::print(spice_stream_, "+{:.3e} {:.3e}\n", 0.0, volt0);
  for (int cycle = 0; cycle < clk_cycle_count_; cycle++) {
    float time0 = time_offset + cycle * period;
    float time1 = time0 + period / 2.0;
    writeWaveformEdge(rf0, time0, slew0);
    writeWaveformEdge(rf1, time1, slew1);
  }
  sta::print(spice_stream_, "+{:.3e} {:.3e}\n", max_time_, volt0);
  sta::print(spice_stream_, "+)\n");
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
                         const TimingArc *next_arc)
{
  Vertex *vertex = path->vertex(this);
  return findSlew(vertex, rf, next_arc);
}

////////////////////////////////////////////////////////////////

void
WritePathSpice::writeMeasureStmts()
{
  sta::print(spice_stream_, "********************\n");
  sta::print(spice_stream_, "* Measure statements\n");
  sta::print(spice_stream_, "********************\n\n");

  for (Stage stage : stages_) {
    const Path *gate_input_path = stages_.stageGateInputPath(stage);
    const Path *drvr_path = stages_.stageDrvrPath(stage);
    const Path *load_path = stages_.stageLoadPath(stage);
    if (gate_input_path) {
      // gate input -> gate output
      writeMeasureSlewStmt(stage, gate_input_path);
      writeMeasureDelayStmt(stage, gate_input_path, drvr_path);
    }
    writeMeasureSlewStmt(stage, drvr_path);
    // gate output | input port -> load
    writeMeasureDelayStmt(stage, drvr_path, load_path);
    if (stage == stages_.stageLast())
      writeMeasureSlewStmt(stage, load_path);
  }
  sta::print(spice_stream_, "\n");
}

void
WritePathSpice::writeMeasureDelayStmt(Stage stage,
                                      const Path *from_path,
                                      const Path *to_path)
{
  writeMeasureDelayStmt(from_path->pin(this), from_path->transition(this),
                        to_path->pin(this), to_path->transition(this),
                        stages_.stageName(stage));
}

void
WritePathSpice::writeMeasureSlewStmt(Stage stage,
                                     const Path *path)
{
  const Pin *pin = path->pin(this);
  const RiseFall *rf = path->transition(this);
  std::string prefix = stages_.stageName(stage);
  writeMeasureSlewStmt(pin, rf, prefix);
}

void
WritePathSpice::writeStageSubckts()
{
  sta::print(spice_stream_, "***************\n");
  sta::print(spice_stream_, "* Stage subckts\n");
  sta::print(spice_stream_, "***************\n\n");

  for (Stage stage : stages_) {
    cap_index_ = 1;
    res_index_ = 1;
    volt_index_ = 1;
    if (stage == stages_.stageFirst())
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
  std::string drvr_pin_name = stages_.stageDrvrPinName(stage);
  std::string load_pin_name = stages_.stageLoadPinName(stage);
  std::string prefix = stages_.stageName(stage);
  sta::print(spice_stream_, ".subckt {} {} {}\n",
             prefix,
             drvr_pin_name,
             load_pin_name);
  writeStageParasitics(stage);
  sta::print(spice_stream_, ".ends\n\n");
}

// Gate and load parasitics.
void
WritePathSpice::writeGateStage(Stage stage)
{
  const Pin *input_pin = stages_.stageGateInputPin(stage);
  std::string input_pin_name = stages_.stageGateInputPinName(stage);
  const Pin *drvr_pin = stages_.stageDrvrPin(stage);
  std::string drvr_pin_name = stages_.stageDrvrPinName(stage);
  const Pin *load_pin = stages_.stageLoadPin(stage);
  std::string load_pin_name = stages_.stageLoadPinName(stage);
  std::string subckt_name = "stage" + std::to_string(stage);

  const Instance *inst = stages_.stageInstance(stage);
  const LibertyPort *input_port = stages_.stageGateInputPort(stage);
  const LibertyPort *drvr_port = stages_.stageDrvrPort(stage);

  sta::print(spice_stream_, ".subckt {} {} {} {}\n",
             subckt_name,
             input_pin_name,
             drvr_pin_name,
             load_pin_name);

  // Driver subckt call.
  sta::print(spice_stream_, "* Gate {} {} -> {}\n",
             network_->pathName(inst),
             input_port->name(),
             drvr_port->name());
  writeSubcktInst(inst);

  const Path *drvr_path = stages_.stageDrvrPath(stage);
  const RiseFall *drvr_rf = drvr_path->transition(this);
  const Path *gate_input_path = stages_.stageGateInputPath(stage);
  const RiseFall *input_rf = gate_input_path->transition(this);
  const Edge *gate_edge = stages_.stageGateEdge(stage);

  PortLogicValues port_values;
  bool is_clked;
  gatePortValues(input_pin, drvr_pin, input_rf, drvr_rf, gate_edge,
                 port_values, is_clked);

  PinSet inputs(network_);
  inputs.insert(input_pin);
  writeSubcktInstVoltSrcs(inst, port_values, inputs);
  sta::print(spice_stream_, "\n");

  PinSet drvr_loads(network_);
  PinConnectedPinIterator *pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    const Pin *load_pin = pin_iter->next();
    drvr_loads.insert(load_pin);
  }
  delete pin_iter;

  writeSubcktInstLoads(drvr_pin, load_pin, drvr_loads, written_insts_);
  writeStageParasitics(stage);
  sta::print(spice_stream_, ".ends\n\n");
}

void
WritePathSpice::writeStageParasitics(Stage stage)
{
  const Path *drvr_path = stages_.stageDrvrPath(stage);
  const MinMax *min_max = drvr_path->minMax(this);
  const Pin *drvr_pin = stages_.stageDrvrPin(stage);
  const Parasitic *parasitic = parasitics_->findParasiticNetwork(drvr_pin);
  if (parasitic == nullptr) {
    const RiseFall *drvr_rf = drvr_path->transition(this);
    parasitic = parasitics_->findPiElmore(drvr_pin, drvr_rf, min_max);
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
  StringSet cell_names = findPathCellNames();
  writeSubckts(cell_names);
}

StringSet
WritePathSpice::findPathCellNames()
{
  StringSet path_cell_names;
  for (Stage stage : stages_) {
    const TimingArc *arc = stages_.stageGateArc(stage);
    if (arc) {
      LibertyCell *cell = arc->set()->libertyCell();
      if (cell) {
        debugPrint(debug_, "write_spice", 2, "cell {}", cell->name());
        path_cell_names.insert(cell->name());
      }
      // Include side receivers.
      const Pin *drvr_pin = stages_.stageDrvrPin(stage);
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

} // namespace sta
