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

#pragma once

#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Bdd.hh"
#include "CircuitSim.hh"
#include "Format.hh"
#include "GraphClass.hh"
#include "Liberty.hh"
#include "Parasitics.hh"
#include "StaState.hh"
#include "StringUtil.hh"

namespace sta {

using ParasiticNodeMap = std::map<const ParasiticNode*, int>;
using CellSpicePortNames = std::map<std::string, StringSeq, std::less<>>;
using LibertyPortLogicValues = std::map<const LibertyPort*, LogicValue>;

// Utilities for writing a spice deck.
class WriteSpice : public StaState
{
public:
  WriteSpice(std::string_view spice_filename,
             std::string_view subckt_filename,
             std::string_view lib_subckt_filename,
             std::string_view model_filename,
             std::string_view power_name,
             std::string_view gnd_name,
             CircuitSim ckt_sim,
             const Scene *scene,
             const MinMax *min_max,
             const StaState *sta);

protected:
  void initPowerGnd();
  void writeHeader(std::string &title,
                   float max_time,
                   float time_step);
  void writePrintStmt(StringSeq &node_names);
  void writeGnuplotFile(StringSeq &node_nanes);
  void writeSubckts(StringSet &cell_names);
  void findCellSubckts(StringSet &cell_names);
  void recordSpicePortNames(std::string_view cell_name,
                            StringSeq &tokens);
  void writeSubcktInst(const Instance *inst);
  void writeSubcktInstVoltSrcs(const Instance *inst,
                               LibertyPortLogicValues &port_values,
                               const PinSet &excluded_input_pins);
  float pgPortVoltage(const LibertyPort *pg_port);
  void writeVoltageSource(std::string_view inst_name,
                          std::string_view port_name,
                          float voltage);
  void writeVoltageSource(std::string_view inst_name,
                          std::string_view subckt_port_name,
                          const LibertyPort *pg_port,
                          float voltage);
  void writeVoltageSource(LibertyCell *cell,
                          std::string_view inst_name,
                          std::string_view subckt_port_name,
                          const std::string &pg_port_name,
                          float voltage);
  void writeClkedStepSource(const Pin *pin,
                            const RiseFall *rf,
                            const Clock *clk);
  void writeDrvrParasitics(const Pin *drvr_pin,
                           const Parasitic *parasitic,
                           const NetSet &coupling_nets);
  void writeParasiticNetwork(const Pin *drvr_pin,
                             const Parasitic *parasitic,
                             const NetSet &coupling_nets);
  void writePiElmore(const Pin *drvr_pin,
                     const Parasitic *parasitic);
  void writeNullParasitic(const Pin *drvr_pin);

  void writeVoltageSource(std::string_view node_name,
                          float voltage);
  void writeRampVoltSource(const Pin *pin,
                           const RiseFall *rf,
                           float time,
                           float slew);
  void writeWaveformVoltSource(const Pin *pin,
                               DriverWaveform *drvr_waveform,
                               const RiseFall *rf,
                               float delay,
                               float slew);
  void writeWaveformEdge(const RiseFall *rf,
                         float time,
                         float slew);
  float railToRailSlew(float slew,
                       const RiseFall *rf);
  void seqPortValues(Sequential *seq,
                     const RiseFall *rf,
                     // Return values.
                     LibertyPortLogicValues &port_values);
  LibertyPort *onePort(FuncExpr *expr);
  void writeMeasureDelayStmt(const Pin *from_pin,
                             const RiseFall *from_rf,
                             const Pin *to_pin,
                             const RiseFall *to_rf,
                             std::string_view prefix);
  void writeMeasureSlewStmt(const Pin *pin,
                            const RiseFall *rf,
                            std::string_view prefix);
  std::string_view spiceTrans(const RiseFall *rf);
  float findSlew(Vertex *vertex,
                 const RiseFall *rf,
                 const TimingArc *next_arc);
  float slewAxisMinValue(const TimingArc *arc);
  float clkWaveformTimeOffset(const Clock *clk);

  void gatePortValues(const Pin *input_pin,
                      const Pin *drvr_pin,
                      const RiseFall *drvr_rf,
                      const Edge *gate_edge,
                      // Return values.
                      LibertyPortLogicValues &port_values,
                      bool &is_clked);
  void regPortValues(const Pin *input_pin,
                     const RiseFall *drvr_rf,
                     const LibertyPort *drvr_port,
                     const FuncExpr *drvr_func,
                     // Return values.
                     LibertyPortLogicValues &port_values,
                     bool &is_clked);
  void gatePortValues(const Instance *inst,
                      const FuncExpr *expr,
                      const LibertyPort *input_port,
                      // Return values.
                      LibertyPortLogicValues &port_values);
  void writeSubcktInstLoads(const Pin *drvr_pin,
                            const Pin *path_load,
                            const PinSet &excluded_input_pins,
                            InstanceSet &written_insts);
  PinSeq drvrLoads(const Pin *drvr_pin);
  void writeSubcktInstVoltSrcs();
  std::string replaceFileExt(std::string_view filename,
                             std::string_view ext);

  const std::string_view spice_filename_;
  const std::string_view subckt_filename_;
  const std::string_view lib_subckt_filename_;
  const std::string_view model_filename_;
  const std::string_view power_name_;
  const std::string_view gnd_name_;
  CircuitSim ckt_sim_;
  const Scene *scene_;
  const MinMax *min_max_;

  std::ofstream spice_stream_;
  LibertyLibrary *default_library_;
  float power_voltage_;
  float gnd_voltage_;
  float max_time_;
  // Resistance to use to simulate a short circuit between spice nodes.
  float short_ckt_resistance_{0.0001F};
  // Input clock waveform cycles.
  // Sequential device numbers.
  int cap_index_{1};
  int res_index_{1};
  int volt_index_{1};
  CellSpicePortNames cell_spice_port_names_;
  Bdd bdd_;
  Parasitics *parasitics_;
};

}  // namespace sta
