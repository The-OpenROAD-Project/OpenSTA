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

#include "WriteGateSpice.hh"

#include <set>
#include <algorithm>

#include "Format.hh"
#include "Units.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Scene.hh"
#include "Transition.hh"
#include "Graph.hh"
#include "TimingArc.hh"
#include "ArcDelayCalc.hh"
#include "dcalc/DelayCalcBase.hh"
#include "GraphDelayCalc.hh"
#include "spice/WriteSpice.hh"

// input waveform with zero slew fails
// input clk waveform from register

namespace sta {

using std::max;

using ParasiticSet = std::set<const Parasitic*>;

class WriteGateSpice : public WriteSpice
{
public:
  WriteGateSpice(ArcDcalcArgSeq &gates,
                 const char *spice_filename,
                 const char *subckt_filename,
                 StringSeq lib_subckt_filenames,
                 const char *model_filename,
                 const char *power_name,
                 const char *gnd_name,
                 CircuitSim ckt_sim,
                 const Scene *scene,
                 const MinMax *min_max,
                 const StaState *sta);
  void writeSpice();

private:
  void findLoadPins();
  void writeHeader();
  void writePrintStmt();
  void writeMeasureStmts();
  void writeGate(const ArcDcalcArg &gate,
                 const NetSet &drvr_nets,
                 ParasiticSet &written_parasitics,
                 InstanceSet &written_insts);
  void writeInputSource(const Pin *in_pin,
                        const RiseFall *in_rf,
                        const TimingRole *role,
                        float delay);
  void writeSubcktInstVoltSrcs(const Pin *in_pin,
                               const RiseFall *input_rf,
                               const Pin *drvr_pin,
                               const RiseFall *drvr_rf);
  const Edge *gateEdge(const Pin *in_pin,
                       const Pin *drvr_pin);
  float maxTime();

  ArcDcalcArgSeq &gates_;

  PinSet load_pins_;

  using WriteSpice::writeHeader;
  using WriteSpice::writePrintStmt;
  using WriteSpice::writeSubcktInstVoltSrcs;
};

void
writeGateSpice(ArcDcalcArgSeq &gates,
               const char *spice_filename,
               const char *subckt_filename,
               StringSeq lib_subckt_filenames,
               const char *model_filename,
               const char *power_name,
               const char *gnd_name,
               CircuitSim ckt_sim,
               const Scene *scene,
               const MinMax *min_max,
               const StaState *sta)
{
  WriteGateSpice writer(gates, spice_filename, subckt_filename,
                        lib_subckt_filenames, model_filename, power_name,
                        gnd_name, ckt_sim, scene, min_max, sta);
  writer.writeSpice();
}
 
////////////////////////////////////////////////////////////////

WriteGateSpice::WriteGateSpice(ArcDcalcArgSeq &gates,
                               const char *spice_filename,
                               const char *subckt_filename,
                               StringSeq lib_subckt_filenames,
                               const char *model_filename,
                               const char *power_name,
                               const char *gnd_name,
                               CircuitSim ckt_sim,
                               const Scene *scene,
                               const MinMax *min_max,
                               const StaState *sta) :
  WriteSpice(spice_filename, subckt_filename, lib_subckt_filenames,
             model_filename, power_name, gnd_name, ckt_sim,
             scene, min_max, sta),
  gates_(gates),
  load_pins_(network_)
{
  arc_delay_calc_->setDcalcArgParasiticSlew(gates_, scene, min_max);
  initPowerGnd(gates_[0].drvrLibrary());
}

void
WriteGateSpice::writeSpice()
{
  spice_stream_.open(spice_filename_);
  if (spice_stream_.is_open()) {
    StringSet cell_names;
    NetSet drvr_nets(network_);
    for (const ArcDcalcArg &gate : gates_) {
      const Net *net = gate.drvrNet(network_);
      drvr_nets.insert(net);
 
      const Pin *drvr_pin = gate.drvrPin();
      const LibertyCell *drvr_cell = gate.drvrCell();
      cell_names.insert(drvr_cell->name());

      for (const Pin *load_pin : drvrLoads(drvr_pin)) {
        const Instance *load_inst = network_->instance(load_pin);
        const LibertyCell *load_cell = network_->libertyCell(load_inst);
        if (load_cell)
          cell_names.insert(load_cell->name());
      }
    }

    ////////////////////////////////////////////////////////////////
    writeHeader();

    writeSubckts(cell_names);

    writePrintStmt();

    if (ckt_sim_ == CircuitSim::hspice)
      writeMeasureStmts();

    findLoadPins();
    ParasiticSet written_parasitics;
    InstanceSet written_insts(network_);
    for (const ArcDcalcArg &gate : gates_)
      writeGate(gate, drvr_nets, written_parasitics, written_insts);

    sta::print(spice_stream_, ".end\n");
    spice_stream_.close();
  }
  else
    throw FileNotWritable(spice_filename_);
}

void
WriteGateSpice::findLoadPins()
{
  for (const ArcDcalcArg &drvr : gates_) {
    const Pin *drvr_pin = drvr.drvrPin();
    PinConnectedPinIterator *pin_iter = network_->connectedPinIterator(drvr_pin);
    while (pin_iter->hasNext()) {
      const Pin *load_pin = pin_iter->next();
      load_pins_.insert(load_pin);
    }
    delete pin_iter;
  }
}

void
WriteGateSpice::writeHeader()
{
  const ArcDcalcArg &gate = gates_[0];
  const Pin *drvr_pin = gate.drvrPin();
  const Instance *inst = network_->instance(drvr_pin);
  const Pin *in_pin = gate.inPin();
  const LibertyPort *in_port = network_->libertyPort(in_pin);
  const LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  std::string title = sta::format("Gate {} {} {} -> {} {}",
                                  network_->pathName(inst),
                                  in_port->name(),
                                  gate.inEdge()->shortName(),
                                  drvr_port->name(),
                                  gate.drvrEdge()->shortName());
  float max_time = maxTime();
  float time_step = 1e-12;
  writeHeader(title, max_time, time_step);
}

float
WriteGateSpice::maxTime()
{
  DcalcAPIndex dcalc_ap_index = scene_->dcalcAnalysisPtIndex(min_max_);
  const ArcDcalcArg &gate = gates_[0];
  const Pin *in_pin = gate.inPin();
  const Pin *drvr_pin = gate.drvrPin();

  Vertex *in_vertex = graph_->pinLoadVertex(in_pin);
  const RiseFall *in_rf = gate.inEdge();
  float in_slew = delayAsFloat(graph_->slew(in_vertex, in_rf, dcalc_ap_index));
  Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
  const RiseFall *drvr_rf = gate.drvrEdge();
  float drvr_slew = delayAsFloat(graph_->slew(drvr_vertex, drvr_rf, dcalc_ap_index));

  float gate_delay = delayAsFloat(graph_->arcDelay(gate.edge(), gate.arc(),
                                                   dcalc_ap_index));

  float load_slew = 0.0;
  float wire_delay = 0.0;
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      float slew = delayAsFloat(graph_->slew(load_vertex, drvr_rf, dcalc_ap_index));
      load_slew = max(load_slew, slew);
      float delay = delayAsFloat(graph_->wireArcDelay(wire_edge,drvr_rf,dcalc_ap_index));
      wire_delay = max(wire_delay, delay);
    }
  }

  float max_time = in_slew + gate_delay + drvr_slew + wire_delay + load_slew;
  return max_time;
}

void
WriteGateSpice::writePrintStmt()
{
  StringSeq node_names;
  NetSet visited_nets(network_);
  for (const ArcDcalcArg &gate : gates_) {
    const Pin *in_pin = gate.inPin();
    node_names.push_back(network_->pathName(in_pin));
    const Net *drvr_net = gate.drvrNet(network_);
    if (!visited_nets.contains(drvr_net)) {
      const Pin *drvr_pin = gate.drvrPin();
      node_names.push_back(network_->pathName(drvr_pin));
      for (const Pin *load_pin : drvrLoads(drvr_pin))
        node_names.push_back(network_->pathName(load_pin));
      visited_nets.insert(drvr_net);
    }
  }

  writePrintStmt(node_names);
}

void
WriteGateSpice::writeGate(const ArcDcalcArg &gate,
                          const NetSet &drvr_nets,
                          ParasiticSet &written_parasitics,
                          InstanceSet &written_insts)
{
  const Pin *in_pin = gate.inPin();
  const Net *in_net = network_->net(in_pin);
  const RiseFall *in_rf = gate.inEdge();
  float input_delay = gate.inputDelay();
  const Instance *inst = network_->instance(in_pin);
  sta::print(spice_stream_, "*****************************\n");
  sta::print(spice_stream_, "* Gate {}\n", network_->pathName(inst));
  // No input source necessary if it driven by one of the gates.
  if (!drvr_nets.contains(in_net))
    writeInputSource(in_pin, in_rf, gate.arc()->role(), input_delay);
  const Pin *drvr_pin = gate.drvrPin();
  const RiseFall *drvr_rf = gate.drvrEdge();
  if (!written_insts.contains(inst)) {
    writeSubcktInst(inst);
    writeSubcktInstVoltSrcs(in_pin, gate.inEdge(), drvr_pin, drvr_rf);
    written_insts.insert(inst);
  }

  // Do not put voltage sources in input pins that are connected to parasitics.
  writeSubcktInstLoads(drvr_pin, nullptr, load_pins_, written_insts);
  const Parasitic *parasitic = gate.parasitic();
  if (!written_parasitics.contains(parasitic)) {
    writeDrvrParasitics(drvr_pin, parasitic, drvr_nets);
    written_parasitics.insert(parasitic);
  }
}

void
WriteGateSpice::writeInputSource(const Pin *in_pin,
                                 const RiseFall *in_rf,
                                 const TimingRole *role,
                                 float delay)
{
  sta::print(spice_stream_, "* Input source\n");
  const LibertyPort *in_port = network_->libertyPort(in_pin);
  Vertex *in_vertex = graph_->pinLoadVertex(in_pin);
  float in_slew = delayAsFloat(graph_delay_calc_->edgeFromSlew(in_vertex, in_rf,
                                                               role, scene_,
                                                               min_max_));
  DriverWaveform *drvr_waveform = nullptr;
  if (in_port)
    drvr_waveform = in_port->driverWaveform(in_rf);
  if (drvr_waveform)
    writeWaveformVoltSource(in_pin, drvr_waveform, in_rf, delay, in_slew);
  else {
    float threshold = threshold_library_->inputThreshold(in_rf);
    float dt = railToRailSlew(in_slew, in_rf);
    float time0 = delay + dt * threshold;
    writeRampVoltSource(in_pin, in_rf, time0, in_slew);
  }
  sta::print(spice_stream_, "\n");
}

void
WriteGateSpice::writeSubcktInstVoltSrcs(const Pin *in_pin,
                                        const RiseFall *input_rf,
                                        const Pin *drvr_pin,
                                        const RiseFall *drvr_rf)
{
  const Edge *gate_edge = gateEdge(in_pin, drvr_pin);
  PortLogicValues port_values;
  bool is_clked;
  gatePortValues(in_pin, drvr_pin, input_rf, drvr_rf, gate_edge, port_values, is_clked);
  PinSet loads(network_);
  loads.insert(in_pin);
  const Instance *inst = network_->instance(in_pin);
  writeSubcktInstVoltSrcs(inst, port_values, loads);
  sta::print(spice_stream_, "\n");
}

const Edge *
WriteGateSpice::gateEdge(const Pin *in_pin,
                         const Pin *drvr_pin)
{
  Vertex *in_vertex = graph_->pinLoadVertex(in_pin);
  Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
  // Iterate over load drivers to avoid driver fanout^2.
  VertexInEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->from(graph_) == in_vertex)
      return edge;
  }
  return nullptr;
}

void
WriteGateSpice::writeMeasureStmts()
{
}

} // namespace sta
