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

%module liberty

%{
#include "PortDirection.hh"
#include "Liberty.hh"
#include "EquivCells.hh"
#include "LibertyWriter.hh"
#include "Sta.hh"

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// Empty class definitions to make swig happy.
// Private constructor/destructor so swig doesn't emit them.
//
////////////////////////////////////////////////////////////////

class LibertyLibrary
{
private:
  LibertyLibrary();
  ~LibertyLibrary();
};

class LibertyLibraryIterator
{
private:
  LibertyLibraryIterator();
  ~LibertyLibraryIterator();
};

class LibertyCell
{
private:
  LibertyCell();
  ~LibertyCell();
};

class LibertyPort
{
private:
  LibertyPort();
  ~LibertyPort();
};

class LibertyCellPortIterator
{
private:
  LibertyCellPortIterator();
  ~LibertyCellPortIterator();
};

class LibertyPortMemberIterator
{
private:
  LibertyPortMemberIterator();
  ~LibertyPortMemberIterator();
};

class TimingArcSet
{
private:
  TimingArcSet();
  ~TimingArcSet();
};

class TimingArc
{
private:
  TimingArc();
  ~TimingArc();
};

class Wireload
{
private:
  Wireload();
  ~Wireload();
};

class WireloadSelection
{
private:
  WireloadSelection();
  ~WireloadSelection();
};

%inline %{

bool
read_liberty_cmd(char *filename,
		 Corner *corner,
		 const MinMaxAll *min_max,
		 bool infer_latches)
{
  Sta *sta = Sta::sta();
  LibertyLibrary *lib = sta->readLiberty(filename, corner, min_max, infer_latches);
  return (lib != nullptr);
}

void
write_liberty_cmd(LibertyLibrary *library,
                  char *filename)
{
  writeLiberty(library, filename, Sta::sta());
}

void
make_equiv_cells(LibertyLibrary *lib)
{
  LibertyLibrarySeq libs;
  libs.push_back(lib);
  Sta::sta()->makeEquivCells(&libs, nullptr);
}

LibertyCellSeq *
find_equiv_cells(LibertyCell *cell)
{
  return Sta::sta()->equivCells(cell);
}

bool
equiv_cells(LibertyCell *cell1,
	    LibertyCell *cell2)
{
  return sta::equivCells(cell1, cell2);
}

bool
equiv_cell_ports(LibertyCell *cell1,
		 LibertyCell *cell2)
{
  return equivCellPorts(cell1, cell2);
}

bool
equiv_cell_timing_arcs(LibertyCell *cell1,
		       LibertyCell *cell2)
{
  return equivCellTimingArcSets(cell1, cell2);
}

LibertyCellSeq *
find_library_buffers(LibertyLibrary *library)
{
  return library->buffers();
}

const char *
liberty_port_direction(const LibertyPort *port)
{
  return port->direction()->name();
}
	     
bool
liberty_supply_exists(const char *supply_name)
{
  auto network = Sta::sta()->network();
  auto lib = network->defaultLibertyLibrary();
  return lib && lib->supplyExists(supply_name);
}

LibertyLibraryIterator *
liberty_library_iterator()
{
  return cmdNetwork()->libertyLibraryIterator();
}

LibertyLibrary *
find_liberty(const char *name)
{
  return cmdNetwork()->findLiberty(name);
}

LibertyCell *
find_liberty_cell(const char *name)
{
  return cmdNetwork()->findLibertyCell(name);
}

bool
timing_role_is_check(TimingRole *role)
{
  return role->isTimingCheck();
}

%} // inline

////////////////////////////////////////////////////////////////
//
// Object Methods
//
////////////////////////////////////////////////////////////////

%extend LibertyLibrary {
const char *name() { return self->name(); }

LibertyCell *
find_liberty_cell(const char *name)
{
  return self->findLibertyCell(name);
}

LibertyCellSeq
find_liberty_cells_matching(const char *pattern,
			    bool regexp,
			    bool nocase)
{
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  return self->findLibertyCellsMatching(&matcher);
}

Wireload *
find_wireload(const char *model_name)
{
  return self->findWireload(model_name);
}

WireloadSelection *
find_wireload_selection(const char *selection_name)
{
  return self->findWireloadSelection(selection_name);
}

OperatingConditions *
find_operating_conditions(const char *op_cond_name)
{
  return self->findOperatingConditions(op_cond_name);
}

OperatingConditions *
default_operating_conditions()
{
  return self->defaultOperatingConditions();
}

} // LibertyLibrary methods

%extend LibertyCell {
const char *name() { return self->name(); }
bool is_leaf() { return self->isLeaf(); }
bool is_buffer() { return self->isBuffer(); }
bool is_inverter() { return self->isInverter(); }
LibertyLibrary *liberty_library() { return self->libertyLibrary(); }
Cell *cell() { return reinterpret_cast<Cell*>(self); }
LibertyPort *
find_liberty_port(const char *name)
{
  return self->findLibertyPort(name);
}

LibertyPortSeq
find_liberty_ports_matching(const char *pattern,
			    bool regexp,
			    bool nocase)
{
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  return self->findLibertyPortsMatching(&matcher);
}

LibertyCellPortIterator *
liberty_port_iterator() { return new LibertyCellPortIterator(self); }

const TimingArcSetSeq &
timing_arc_sets()
{
  return self->timingArcSets();
}

void
ensure_voltage_waveforms()
{
  Corners *corners = Sta::sta()->corners();
  const DcalcAnalysisPtSeq &dcalc_aps = corners->dcalcAnalysisPts();
  self->ensureVoltageWaveforms(dcalc_aps);
}

LibertyCell *test_cell() { return self->testCell(); }

} // LibertyCell methods

%extend LibertyPort {
const char *bus_name() { return self->busName(); }
Cell *cell() { return self->cell(); }
bool is_bus() { return self->isBus(); }
LibertyPortMemberIterator *
member_iterator() { return new LibertyPortMemberIterator(self); }

const char *
function()
{
  FuncExpr *func = self->function();
  if (func)
    return func->asString();
  else
    return nullptr;
}

const char *
tristate_enable()
{
  FuncExpr *enable = self->tristateEnable();
  if (enable)
    return enable->asString();
  else
    return nullptr;
}

float
capacitance(Corner *corner,
	    const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  return sta->capacitance(self, corner, min_max);
}

void
set_direction(const char *dir)
{
  self->setDirection(PortDirection::find(dir));
}

} // LibertyPort methods

%extend TimingArcSet {
LibertyPort *from() { return self->from(); }
LibertyPort *to() { return self->to(); }
TimingRole *role() { return self->role(); }
const char *sdf_cond() { return self->sdfCond(); }

const char *
full_name()
{
  const char *from = self->from()->name();
  const char *to = self->to()->name();
  const char *cell_name = self->libertyCell()->name();
  return stringPrintTmp("%s %s -> %s",
			cell_name,
			from,
			to);
}

TimingArcSeq &
timing_arcs() { return self->arcs(); }

} // TimingArcSet methods

%extend TimingArc {
LibertyPort *from() { return self->from(); }
LibertyPort *to() { return self->to(); }
Transition *from_edge() { return self->fromEdge(); }
const char *from_edge_name() { return self->fromEdge()->asRiseFall()->name(); }
Transition *to_edge() { return self->toEdge(); }
const char *to_edge_name() { return self->toEdge()->asRiseFall()->name(); }
TimingRole *role() { return self->role(); }

float
time_voltage(float in_slew,
             float load_cap,
             float time)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms)
      return waveforms->timeVoltage(in_slew, load_cap, time);
  }
  return 0.0;
}

float
time_current(float in_slew,
             float load_cap,
             float time)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms)
      return waveforms->timeCurrent(in_slew, load_cap, time);
  }
  return 0.0;
}

float
voltage_current(float in_slew,
                float load_cap,
                float voltage)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms)
      return waveforms->voltageCurrent(in_slew, load_cap, voltage);
  }
  return 0.0;
}

float
voltage_time(float in_slew,
             float load_cap,
             float voltage)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms)
      return waveforms->voltageTime(in_slew, load_cap, voltage);
  }
  return 0.0;
}

Table1
voltage_waveform(float in_slew,
                 float load_cap)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms) {
      Table1 waveform = waveforms->voltageWaveform(in_slew, load_cap);
      return waveform;
    }
  }
  return Table1();
}

const Table1 *
voltage_waveform_raw(float in_slew,
                     float load_cap)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms) {
      const Table1 *waveform = waveforms->voltageWaveformRaw(in_slew, load_cap);
      return waveform;
    }
  }
  return nullptr;
}

Table1
current_waveform(float in_slew,
                 float load_cap)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms) {
      Table1 waveform = waveforms->currentWaveform(in_slew, load_cap);
      return waveform;
    }
  }
  return Table1();
}

const Table1 *
current_waveform_raw(float in_slew,
                     float load_cap)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms) {
      const Table1 *waveform = waveforms->currentWaveformRaw(in_slew, load_cap);
      return waveform;
    }
  }
  return nullptr;
}

Table1
voltage_current_waveform(float in_slew,
                         float load_cap)
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms) {
      Table1 waveform = waveforms->voltageCurrentWaveform(in_slew, load_cap);
      return waveform;
    }
  }
  return Table1();
}

float
final_resistance()
{
  GateTableModel *gate_model = self->gateTableModel();
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms) {
      return waveforms->finalResistance();
    }
  }
  return 0.0;
}

} // TimingArc methods

%extend OperatingConditions {
float process() { return self->process(); }
float voltage() { return self->voltage(); }
float temperature() { return self->temperature(); }
}

%extend LibertyLibraryIterator {
bool has_next() { return self->hasNext(); }
LibertyLibrary *next() { return self->next(); }
void finish() { delete self; }
} // LibertyLibraryIterator methods

%extend LibertyCellPortIterator {
bool has_next() { return self->hasNext(); }
LibertyPort *next() { return self->next(); }
void finish() { delete self; }
} // LibertyCellPortIterator methods

%extend LibertyPortMemberIterator {
bool has_next() { return self->hasNext(); }
LibertyPort *next() { return self->next(); }
void finish() { delete self; }
} // LibertyPortMemberIterator methods
