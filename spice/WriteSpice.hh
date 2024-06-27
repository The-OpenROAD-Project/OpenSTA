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

#pragma once

#include <fstream>
#include <string>
#include <map>
#include <vector>

#include "StaState.hh"
#include "StringSet.hh"
#include "Liberty.hh"
#include "GraphClass.hh"
#include "Parasitics.hh"
#include "Bdd.hh"
#include "CircuitSim.hh"

namespace sta {

using std::string;
using std::ofstream;

typedef std::map<const ParasiticNode*, int> ParasiticNodeMap;
typedef Map<string, StringVector> CellSpicePortNames;
typedef Map<const LibertyPort*, LogicValue> LibertyPortLogicValues;
typedef std::vector<string> StdStringSeq;

// Utilities for writing a spice deck.
class WriteSpice : public StaState
{
public:
  WriteSpice(const char *spice_filename,
             const char *subckt_filename,
             const char *lib_subckt_filename,
             const char *model_filename,
             const char *power_name,
             const char *gnd_name,
             CircuitSim ckt_sim,
             const DcalcAnalysisPt *dcalc_ap,
             const StaState *sta);

protected:
  void initPowerGnd();
  void writeHeader(string &title,
                   float max_time,
                   float time_step);
  void writePrintStmt(StdStringSeq &node_names);
  void writeGnuplotFile(StdStringSeq &node_nanes);
  void writeSubckts(StdStringSet &cell_names);
  void findCellSubckts(StdStringSet &cell_names);
  void recordSpicePortNames(const char *cell_name,
			    StringVector &tokens);
  void writeSubcktInst(const Instance *inst);
  void writeSubcktInstVoltSrcs(const Instance *inst,
			       LibertyPortLogicValues &port_values,
                               const PinSet &excluded_input_pins);
  float pgPortVoltage(LibertyPgPort *pg_port);
  void writeVoltageSource(const char *inst_name,
			  const char *port_name,
			  float voltage);
  void writeVoltageSource(LibertyCell *cell,
			  const char *inst_name,
			  const char *subckt_port_name,
			  const char *pg_port_name,
			  float voltage);
  void writeClkedStepSource(const Pin *pin,
			    const RiseFall *rf,
			    const Clock *clk);
  void writeDrvrParasitics(const Pin *drvr_pin,
                           const RiseFall *drvr_rf,
                           // Nets with parasitics to include coupling caps to.
                           const NetSet &coupling_nets,
                           const ParasiticAnalysisPt *parasitic_ap);
  void writeDrvrParasitics(const Pin *drvr_pin,
                           const Parasitic *parasitic,
                           const NetSet &coupling_nets);
  void writeParasiticNetwork(const Pin *drvr_pin,
                             const Parasitic *parasitic,
                             const NetSet &aggressor_nets);
  void writePiElmore(const Pin *drvr_pin,
                     const Parasitic *parasitic);
  void writeNullParasitic(const Pin *drvr_pin);

  void writeVoltageSource(const char *node_name,
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
                             string prefix);
  void writeMeasureSlewStmt(const Pin *pin,
                            const RiseFall *rf,
                            string prefix);
  const char *spiceTrans(const RiseFall *rf);
  float findSlew(Vertex *vertex,
		 const RiseFall *rf,
		 TimingArc *next_arc);
  float slewAxisMinValue(TimingArc *arc);
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
  string replaceFileExt(string filename,
                        const char *ext);

  const char *spice_filename_;
  const char *subckt_filename_;
  const char *lib_subckt_filename_;
  const char *model_filename_;
  const char *power_name_;
  const char *gnd_name_;
  CircuitSim ckt_sim_;
  const DcalcAnalysisPt *dcalc_ap_;

  ofstream spice_stream_;
  LibertyLibrary *default_library_;
  float power_voltage_;
  float gnd_voltage_;
  float max_time_;
  // Resistance to use to simulate a short circuit between spice nodes.
  float short_ckt_resistance_;
  // Input clock waveform cycles.
  // Sequential device numbers.
  int cap_index_;
  int res_index_;
  int volt_index_;
  CellSpicePortNames cell_spice_port_names_;
  Bdd bdd_;
};

void
streamPrint(ofstream &stream,
	    const char *fmt,
	    ...) __attribute__((format (printf, 2, 3)));

} // namespace
