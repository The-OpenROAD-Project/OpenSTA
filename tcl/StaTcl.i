%module sta

%{

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

////////////////////////////////////////////////////////////////
//
// Most of the TCL SWIG interface code is in this file.  This and any
// optional interface code is %included into a final interface file
// used by the application.
//
// Define TCL methods for each network object.  This works despite the
// fact that the underlying implementation does not have class methods
// corresponding to the TCL methods defined here.
//
// Note the function name changes from sta naming convention
// (lower/capitalize) to TCL naming convention (lower/underscore).
//
////////////////////////////////////////////////////////////////

#include "Machine.hh"
#include "StaConfig.hh"  // STA_VERSION
#include "Stats.hh"
#include "Report.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "PatternMatch.hh"
#include "MinMax.hh"
#include "Fuzzy.hh"
#include "FuncExpr.hh"
#include "Units.hh"
#include "Transition.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "Liberty.hh"
#include "LibertyWriter.hh"
#include "EquivCells.hh"
#include "Wireload.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Clock.hh"
#include "PortDelay.hh"
#include "ExceptionPath.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "DelayCalc.hh"
#include "DcalcAnalysisPt.hh"
#include "Corner.hh"
#include "PathVertex.hh"
#include "PathRef.hh"
#include "PathExpanded.hh"
#include "PathEnd.hh"
#include "PathGroup.hh"
#include "PathAnalysisPt.hh"
#include "Property.hh"
#include "WritePathSpice.hh"
#include "Search.hh"
#include "Sta.hh"
#include "search/Tag.hh"
#include "search/CheckTiming.hh"
#include "search/CheckMinPulseWidths.hh"
#include "search/Levelize.hh"
#include "search/ReportPath.hh"

namespace sta {

////////////////////////////////////////////////////////////////
//
// C++ helper functions used by the interface functions.
// These are not visible in the TCL API.
//
////////////////////////////////////////////////////////////////

typedef MinPulseWidthCheckSeq::Iterator MinPulseWidthCheckSeqIterator;

// Get the network for commands.
Network *
cmdNetwork()
{
  return Sta::sta()->cmdNetwork();
}

// Make sure the network has been read and linked.
// Throwing an error means the caller doesn't have to check the result.
Network *
cmdLinkedNetwork()
{
  Network *network = cmdNetwork();
  if (network->isLinked())
    return network;
  else {
    Report *report = Sta::sta()->report();
    report->error(1570, "no network has been linked.");
    return nullptr;
  }
}

// Make sure an editable network has been read and linked.
NetworkEdit *
cmdEditNetwork()
{
  Network *network = cmdLinkedNetwork();
  if (network->isEditable())
    return dynamic_cast<NetworkEdit*>(network);
  else {
    Report *report = Sta::sta()->report();
    report->error(1571, "network does not support edits.");
    return nullptr;
  }
}

// Get the graph for commands.
// Throw to cmd level on failure.
Graph *
cmdGraph()
{
  cmdLinkedNetwork();
  return Sta::sta()->ensureGraph();
}

} // namespace

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// Empty class definitions to make swig happy.
// Private constructor/destructor so swig doesn't emit them.
//
////////////////////////////////////////////////////////////////

class Library
{
private:
  Library();
  ~Library();
};

class LibraryIterator
{
private:
  LibraryIterator();
  ~LibraryIterator();
};

class Cell
{
private:
  Cell();
  ~Cell();
};

class CellPortIterator
{
private:
  CellPortIterator();
  ~CellPortIterator();
};

class LibertyCellPortIterator
{
private:
  LibertyCellPortIterator();
  ~LibertyCellPortIterator();
};

class Port
{
private:
  Port();
  ~Port();
};

class PortMemberIterator
{
private:
  PortMemberIterator();
  ~PortMemberIterator();
};

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

class Transition
{
private:
  Transition();
  ~Transition();
};

class Instance
{
private:
  Instance();
  ~Instance();
};

class Pin
{
private:
  Pin();
  ~Pin();
};

class Term
{
private:
  Term();
  ~Term();
};

class InstanceChildIterator
{
private:
  InstanceChildIterator();
  ~InstanceChildIterator();
};

class InstancePinIterator
{
private:
  InstancePinIterator();
  ~InstancePinIterator();
};

class InstanceNetIterator
{
private:
  InstanceNetIterator();
  ~InstanceNetIterator();
};

class LeafInstanceIterator
{
private:
  LeafInstanceIterator();
  ~LeafInstanceIterator();
};

class Net
{
private:
  Net();
  ~Net();
};

class NetPinIterator
{
private:
  NetPinIterator();
  ~NetPinIterator();
};

class NetTermIterator
{
private:
  NetTermIterator();
  ~NetTermIterator();
};

class NetConnectedPinIterator
{
private:
  NetConnectedPinIterator();
  ~NetConnectedPinIterator();
};

class PinConnectedPinIterator
{
private:
  PinConnectedPinIterator();
  ~PinConnectedPinIterator();
};

class Clock
{
private:
  Clock();
  ~Clock();
};

class ClockEdge
{
private:
  ClockEdge();
  ~ClockEdge();
};

class Vertex
{
private:
  Vertex();
  ~Vertex();
};

class Edge
{
private:
  Edge();
  ~Edge();
};

class VertexIterator
{
private:
  VertexIterator();
  ~VertexIterator();
};

class VertexInEdgeIterator
{
private:
  VertexInEdgeIterator();
  ~VertexInEdgeIterator();
};

class VertexOutEdgeIterator
{
private:
  VertexOutEdgeIterator();
  ~VertexOutEdgeIterator();
};

class PathRef
{
private:
  PathRef();
  ~PathRef();
};

class PathEnd
{
private:
  PathEnd();
  ~PathEnd();
};

class MinPulseWidthCheck
{
private:
  MinPulseWidthCheck();
  ~MinPulseWidthCheck();
};

class MinPulseWidthCheckSeq
{
private:
  MinPulseWidthCheckSeq();
  ~MinPulseWidthCheckSeq();
};

class MinPulseWidthCheckSeqIterator
{
private:
  MinPulseWidthCheckSeqIterator();
  ~MinPulseWidthCheckSeqIterator();
};

class VertexPathIterator
{
private:
  VertexPathIterator();
  ~VertexPathIterator();
};

class ExceptionFrom
{
private:
  ExceptionFrom();
  ~ExceptionFrom();
};

class ExceptionThru
{
private:
  ExceptionThru();
  ~ExceptionThru();
};

class ExceptionTo
{
private:
  ExceptionTo();
  ~ExceptionTo();
};

class OperatingConditions
{
private:
  OperatingConditions();
  ~OperatingConditions();
};

class Corner
{
private:
  Corner();
  ~Corner();
};

////////////////////////////////////////////////////////////////
//
// C++ functions visible as TCL functions.
//
////////////////////////////////////////////////////////////////

%inline %{

float float_inf = INF;
int group_count_max = PathGroup::group_count_max;

const char *
version()
{
  return STA_VERSION;
}

const char *
git_sha1()
{
  return STA_GIT_SHA1;
}

void
report_error(int id,
             const char *msg)
{
  Report *report = Sta::sta()->report();
  report->error(id, "%s", msg);
}

void
report_file_error(int id,
                  const char *filename,
                  int line,
                  const char *msg)
{
  Report *report = Sta::sta()->report();
  report->error(id, filename, line, "%s", msg);
}

void
report_warn(int id,
            const char *msg)
{
  Report *report = Sta::sta()->report();
  report->warn(id, "%s", msg);
}

void
report_file_warn(int id,
                 const char *filename,
                 int line,
                 const char *msg)
{
  Report *report = Sta::sta()->report();
  report->fileWarn(id, filename, line, "%s", msg);
}

void
report_line(const char *msg)
{
  Sta *sta = Sta::sta();
  if (sta)
    sta->report()->reportLineString(msg);
  else
    // After sta::delete_all_memory souce -echo prints the cmd file line
    printf("%s\n", msg);
}

void
fflush()
{
  fflush(stdout);
  fflush(stderr);
}

void
redirect_file_begin(const char *filename)
{
  Sta::sta()->report()->redirectFileBegin(filename);
}

void
redirect_file_append_begin(const char *filename)
{
  Sta::sta()->report()->redirectFileAppendBegin(filename);
}

void
redirect_file_end()
{
  Sta::sta()->report()->redirectFileEnd();
}

void
redirect_string_begin()
{
  Sta::sta()->report()->redirectStringBegin();
}

const char *
redirect_string_end()
{
  return Sta::sta()->report()->redirectStringEnd();
}

void
log_begin_cmd(const char *filename)
{
  Sta::sta()->report()->logBegin(filename);
}

void
log_end()
{
  Sta::sta()->report()->logEnd();
}

void
set_debug(const char *what,
	  int level)
{
  Sta::sta()->setDebugLevel(what, level);
}

bool
is_object(const char *obj)
{
  // _hexaddress_p_type
  const char *s = obj;
  char ch = *s++;
  if (ch != '_')
    return false;
  while (*s && isxdigit(*s))
    s++;
  if ((s - obj - 1) == sizeof(void*) * 2
      && *s && *s++ == '_'
      && *s && *s++ == 'p'
      && *s && *s++ == '_') {
    while (*s && *s != ' ')
      s++;
    return *s == '\0';
  }
  else
    return false;
}

// Assumes is_object is true.
const char *
object_type(const char *obj)
{
  return &obj[1 + sizeof(void*) * 2 + 3];
}

bool
is_object_list(const char *list,
	       const char *type)
{
  const char *s = list;
  while (s) {
    bool type_match;
    const char *next;
    objectListNext(s, type, type_match, next);
    if (type_match)
      s = next;
    else
      return false;
  }
  return true;
}

const char *
rise_short_name()
{
  return RiseFall::rise()->shortName();
}

const char *
fall_short_name()
{
  return RiseFall::fall()->shortName();
}

bool
pin_is_constrained(Pin *pin)
{
  return Sta::sta()->sdc()->isConstrained(pin);
}

bool
instance_is_constrained(Instance *inst)
{
  return Sta::sta()->sdc()->isConstrained(inst);
}

bool
net_is_constrained(Net *net)
{
  return Sta::sta()->sdc()->isConstrained(net);
}

bool
clk_thru_tristate_enabled()
{
  return Sta::sta()->clkThruTristateEnabled();
}

void
set_clk_thru_tristate_enabled(bool enabled)
{
  Sta::sta()->setClkThruTristateEnabled(enabled);
}

bool
network_is_linked()
{
  return Sta::sta()->cmdNetwork()->isLinked();
}

void
set_path_divider(char divider)
{
  cmdNetwork()->setPathDivider(divider);
}

void
set_current_instance(Instance *inst)
{
  Sta::sta()->setCurrentInstance(inst);
}

bool
read_liberty_cmd(char *filename,
		 Corner *corner,
		 const MinMaxAll *min_max,
		 bool infer_latches)
{
  LibertyLibrary *lib = Sta::sta()->readLiberty(filename, corner, min_max,
						infer_latches);
  return (lib != nullptr);
}

bool
set_min_library_cmd(char *min_filename,
		    char *max_filename)
{
  return Sta::sta()->setMinLibrary(min_filename, max_filename);
}

void
write_liberty_cmd(LibertyLibrary *library,
                  char *filename)
{
  writeLiberty(library, filename, Sta::sta());
}

Library *
find_library(const char *name)
{
  return cmdNetwork()->findLibrary(name);
}

LibraryIterator *
library_iterator()
{
  return cmdNetwork()->libraryIterator();
}

LibertyLibrary *
find_liberty(const char *name)
{
  return cmdNetwork()->findLiberty(name);
}

LibertyLibraryIterator *
liberty_library_iterator()
{
  return cmdNetwork()->libertyLibraryIterator();
}

LibertyCell *
find_liberty_cell(const char *name)
{
  return cmdNetwork()->findLibertyCell(name);
}

CellSeq
find_cells_matching(const char *pattern,
		    bool regexp,
		    bool nocase)
{
  Network *network = cmdNetwork();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  CellSeq matches;
  LibraryIterator *lib_iter = network->libraryIterator();
  while (lib_iter->hasNext()) {
    Library *lib = lib_iter->next();
    CellSeq lib_matches = network->findCellsMatching(lib, &matcher);
    for (Cell *match : lib_matches)
      matches.push_back(match);
  }
  delete lib_iter;
  return matches;
}

LibertyCellSeq *
find_library_buffers(LibertyLibrary *library)
{
  return library->buffers();
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

void
set_cmd_namespace_cmd(const char *namespc)
{
  if (stringEq(namespc, "sdc"))
    Sta::sta()->setCmdNamespace(CmdNamespace::sdc);
  else if (stringEq(namespc, "sta"))
    Sta::sta()->setCmdNamespace(CmdNamespace::sta);
  else
    criticalError(269, "unknown namespace");
}

bool
link_design_cmd(const char *top_cell_name)
{
  return Sta::sta()->linkDesign(top_cell_name);
}

bool
link_make_black_boxes()
{
  return Sta::sta()->linkMakeBlackBoxes();
}

void
set_link_make_black_boxes(bool make)
{
  Sta::sta()->setLinkMakeBlackBoxes(make);
}

Instance *
top_instance()
{
  return cmdLinkedNetwork()->topInstance();
}

const char *
liberty_port_direction(const LibertyPort *port)
{
  return port->direction()->name();
}
	     
const char *
port_direction(const Port *port)
{
  return cmdLinkedNetwork()->direction(port)->name();
}
	     
const char *
pin_direction(const Pin *pin)
{
  return cmdLinkedNetwork()->direction(pin)->name();
}

PortSeq
find_ports_matching(const char *pattern,
		    bool regexp,
		    bool nocase)
{
  Network *network = cmdLinkedNetwork();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  Cell *top_cell = network->cell(network->topInstance());
  PortSeq matches1 = network->findPortsMatching(top_cell, &matcher);
  // Expand bus/bundle ports.
  PortSeq matches;
  for (const Port *port : matches1) {
    if (network->isBus(port)
	|| network->isBundle(port)) {
      PortMemberIterator *member_iter = network->memberIterator(port);
      while (member_iter->hasNext()) {
	Port *member = member_iter->next();
	matches.push_back(member);
      }
      delete member_iter;
    }
    else
      matches.push_back(port);
  }
  return matches;
}

PinSeq
find_port_pins_matching(const char *pattern,
			bool regexp,
			bool nocase)
{
  Network *network = cmdLinkedNetwork();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  Instance *top_inst = network->topInstance();
  Cell *top_cell = network->cell(top_inst);
  PortSeq ports = network->findPortsMatching(top_cell, &matcher);
  PinSeq pins;
  for (const Port *port : ports) {
    if (network->isBus(port)
	|| network->isBundle(port)) {
      PortMemberIterator *member_iter = network->memberIterator(port);
      while (member_iter->hasNext()) {
	Port *member = member_iter->next();
	Pin *pin = network->findPin(top_inst, member);
	if (pin)
	  pins.push_back(pin);
      }
      delete member_iter;
    }
    else {
      Pin *pin = network->findPin(top_inst, port);
      if (pin)
	pins.push_back(pin);
    }
  }
  return pins;
}

Pin *
find_pin(const char *path_name)
{
  return cmdLinkedNetwork()->findPin(path_name);
}

Pin *
get_port_pin(const Port *port)
{
  Network *network = cmdLinkedNetwork();
  return network->findPin(network->topInstance(), port);
}

PinSeq
find_pins_matching(const char *pattern,
		   bool regexp,
		   bool nocase)
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  Instance *current_instance = sta->currentInstance();
  PinSeq matches = network->findPinsMatching(current_instance, &matcher);
  return matches;
}

PinSeq
find_pins_hier_matching(const char *pattern,
			bool regexp,
			bool nocase)
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  Instance *current_instance = sta->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  PinSeq matches = network->findPinsHierMatching(current_instance, &matcher);
  return matches;
}

Instance *
find_instance(char *path_name)
{
  return cmdLinkedNetwork()->findInstance(path_name);
}

InstanceSeq
network_leaf_instances()
{
  return cmdLinkedNetwork()->leafInstances();
}

InstanceSeq
find_instances_matching(const char *pattern,
			bool regexp,
			bool nocase)
{
  Sta *sta = Sta::sta();
  Instance *current_instance = sta->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, sta->tclInterp());
  Network *network = cmdLinkedNetwork();
  InstanceSeq matches = network->findInstancesMatching(current_instance, &matcher);
  return matches;
}

InstanceSeq
find_instances_hier_matching(const char *pattern,
			     bool regexp,
			     bool nocase)
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  Instance *current_instance = sta->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, sta->tclInterp());
  InstanceSeq matches = network->findInstancesHierMatching(current_instance, &matcher);
  return matches;
}

InstanceSet
find_register_instances(ClockSet *clks,
			const RiseFallBoth *clk_tr,
			bool edge_triggered,
			bool latches)
{
  cmdLinkedNetwork();
  InstanceSet insts = Sta::sta()->findRegisterInstances(clks, clk_tr,
                                                        edge_triggered,
                                                        latches);
  delete clks;
  return insts;
}

PinSet
find_register_data_pins(ClockSet *clks,
			const RiseFallBoth *clk_tr,
			bool edge_triggered,
			bool latches)
{
  cmdLinkedNetwork();
  PinSet pins = Sta::sta()->findRegisterDataPins(clks, clk_tr,
                                                 edge_triggered, latches);
  delete clks;
  return pins;
}

PinSet
find_register_clk_pins(ClockSet *clks,
		       const RiseFallBoth *clk_tr,
		       bool edge_triggered,
		       bool latches)
{
  cmdLinkedNetwork();
  PinSet pins = Sta::sta()->findRegisterClkPins(clks, clk_tr,
                                                edge_triggered, latches);
  delete clks;
  return pins;
}

PinSet
find_register_async_pins(ClockSet *clks,
			 const RiseFallBoth *clk_tr,
			 bool edge_triggered,
			 bool latches)
{
  cmdLinkedNetwork();
  PinSet pins = Sta::sta()->findRegisterAsyncPins(clks, clk_tr,
                                                  edge_triggered, latches);
  delete clks;
  return pins;
}

PinSet
find_register_output_pins(ClockSet *clks,
			  const RiseFallBoth *clk_tr,
			  bool edge_triggered,
			  bool latches)
{
  cmdLinkedNetwork();
  PinSet pins = Sta::sta()->findRegisterOutputPins(clks, clk_tr,
                                                   edge_triggered, latches);
  delete clks;
  return pins;
}

Net *
find_net(char *path_name)
{
  return cmdLinkedNetwork()->findNet(path_name);
}

NetSeq
find_nets_matching(const char *pattern,
		   bool regexp,
		   bool nocase)
{
  Network *network = cmdLinkedNetwork();
  Instance *current_instance = Sta::sta()->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  NetSeq matches = network->findNetsMatching(current_instance, &matcher);
  return matches;
}

NetSeq
find_nets_hier_matching(const char *pattern,
			bool regexp,
			bool nocase)
{
  Network *network = cmdLinkedNetwork();
  Instance *current_instance = Sta::sta()->currentInstance();
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  NetSeq matches = network->findNetsHierMatching(current_instance, &matcher);
  return matches;
}

PortSeq
all_inputs_cmd(bool no_clocks)
{
  Sta *sta = Sta::sta();
  cmdLinkedNetwork();
  return sta->sdc()->allInputs(no_clocks);
}

PortSeq
all_outputs_cmd()
{
  Sta *sta = Sta::sta();
  cmdLinkedNetwork();
  return sta->sdc()->allOutputs();
}

PortSeq
filter_ports(const char *property,
	     const char *op,
	     const char *pattern,
	     PortSeq *ports)
{
  PortSeq filtered_ports;
  if (ports) {
    Sta *sta = Sta::sta();
    bool exact_match = stringEq(op, "==");
    bool pattern_match = stringEq(op, "=~");
    bool not_match = stringEq(op, "!=");
    for (const Port *port : *ports) {
      PropertyValue value(getProperty(port, property, sta));
      const char *prop = value.stringValue();
      if (prop &&
          ((exact_match && stringEq(prop, pattern))
           || (not_match && !stringEq(prop, pattern))
           || (pattern_match && patternMatch(pattern, prop))))
        filtered_ports.push_back(port);
    }
    delete ports;
  }
  return filtered_ports;
}

InstanceSeq
filter_insts(const char *property,
	     const char *op,
	     const char *pattern,
	     InstanceSeq *insts)
{
  InstanceSeq filtered_insts;
  if (insts) {
    Sta *sta = Sta::sta();
    cmdLinkedNetwork();
    bool exact_match = stringEq(op, "==");
    bool pattern_match = stringEq(op, "=~");
    bool not_match = stringEq(op, "!=");
    for (const Instance *inst : *insts) {
      PropertyValue value(getProperty(inst, property, sta));
      const char *prop = value.stringValue();
      if (prop &&
          ((exact_match && stringEq(prop, pattern))
           || (not_match && !stringEq(prop, pattern))
           || (pattern_match && patternMatch(pattern, prop))))
        filtered_insts.push_back(inst);
    }
    delete insts;
  }
  return filtered_insts;
}

PinSeq
filter_pins(const char *property,
	    const char *op,
	    const char *pattern,
	    PinSeq *pins)
{
  PinSeq filtered_pins;
  if (pins) {
    Sta *sta = Sta::sta();
    bool exact_match = stringEq(op, "==");
    bool pattern_match = stringEq(op, "=~");
    bool not_match = stringEq(op, "!=");
    for (const Pin *pin : *pins) {
      PropertyValue value(getProperty(pin, property, sta));
      const char *prop = value.asString(sta->sdcNetwork());
      if (prop &&
          ((exact_match && stringEq(prop, pattern))
           || (not_match && !stringEq(prop, pattern))
           || (pattern_match && patternMatch(pattern, prop))))
        filtered_pins.push_back(pin);
    }
    delete pins;
  }
  return filtered_pins;
}

PropertyValue
pin_property(const Pin *pin,
	     const char *property)
{
  cmdLinkedNetwork();
  return getProperty(pin, property, Sta::sta());
}

PropertyValue
instance_property(const Instance *inst,
		  const char *property)
{
  cmdLinkedNetwork();
  return getProperty(inst, property, Sta::sta());
}

PropertyValue
net_property(const Net *net,
	     const char *property)
{
  cmdLinkedNetwork();
  return getProperty(net, property, Sta::sta());
}

PropertyValue
port_property(const Port *port,
	      const char *property)
{
  return getProperty(port, property, Sta::sta());
}


PropertyValue
liberty_cell_property(const LibertyCell *cell,
		      const char *property)
{
  return getProperty(cell, property, Sta::sta());
}

PropertyValue
cell_property(const Cell *cell,
	      const char *property)
{
  return getProperty(cell, property, Sta::sta());
}

PropertyValue
liberty_port_property(const LibertyPort *port,
		      const char *property)
{
  return getProperty(port, property, Sta::sta());
}

PropertyValue
library_property(const Library *lib,
		 const char *property)
{
  return getProperty(lib, property, Sta::sta());
}

PropertyValue
liberty_library_property(const LibertyLibrary *lib,
			 const char *property)
{
  return getProperty(lib, property, Sta::sta());
}

PropertyValue
edge_property(Edge *edge,
	      const char *property)
{
  cmdGraph();
  return getProperty(edge, property, Sta::sta());
}

PropertyValue
clock_property(Clock *clk,
	       const char *property)
{
  cmdLinkedNetwork();
  return getProperty(clk, property, Sta::sta());
}

PropertyValue
path_end_property(PathEnd *end,
		  const char *property)
{
  cmdLinkedNetwork();
  return getProperty(end, property, Sta::sta());
}

PropertyValue
path_ref_property(PathRef *path,
		  const char *property)
{
  cmdLinkedNetwork();
  return getProperty(path, property, Sta::sta());
}

PropertyValue
timing_arc_set_property(TimingArcSet *arc_set,
			const char *property)
{
  cmdLinkedNetwork();
  return getProperty(arc_set, property, Sta::sta());
}

LeafInstanceIterator *
leaf_instance_iterator()
{
  return cmdLinkedNetwork()->leafInstanceIterator();
}

////////////////////////////////////////////////////////////////

void
define_corners_cmd(StringSet *corner_names)
{
  Sta *sta = Sta::sta();
  sta->makeCorners(corner_names);
  delete corner_names;
}

Corner *
cmd_corner()
{
  return Sta::sta()->cmdCorner();
}

void
set_cmd_corner(Corner *corner)
{
  Sta::sta()->setCmdCorner(corner);
}

Corner *
find_corner(const char *corner_name)
{
  return Sta::sta()->findCorner(corner_name);
}

Corners *
corners()
{
  return Sta::sta()->corners();
}

bool
multi_corner()
{
  return Sta::sta()->multiCorner();
}

////////////////////////////////////////////////////////////////

void
set_analysis_type_cmd(const char *analysis_type)
{
  AnalysisType type;
  if (stringEq(analysis_type, "single"))
    type = AnalysisType::single;
  else if (stringEq(analysis_type, "bc_wc"))
    type = AnalysisType::bc_wc;
  else if (stringEq(analysis_type, "on_chip_variation"))
    type = AnalysisType::ocv;
  else {
    criticalError(270, "unknown analysis type");
    type = AnalysisType::single;
  }
  Sta::sta()->setAnalysisType(type);
}

OperatingConditions *
operating_conditions(const MinMax *min_max)
{
  return Sta::sta()->operatingConditions(min_max);
}

void
set_operating_conditions_cmd(OperatingConditions *op_cond,
			     const MinMaxAll *min_max)
{
  Sta::sta()->setOperatingConditions(op_cond, min_max);
}

EdgeSeq
filter_timing_arcs(const char *property,
		   const char *op,
		   const char *pattern,
		   EdgeSeq *edges)
{
  Sta *sta = Sta::sta();
  EdgeSeq filtered_edges;
  bool exact_match = stringEq(op, "==");
  bool pattern_match = stringEq(op, "=~");
  bool not_match = stringEq(op, "!=");
  for (Edge *edge : *edges) {
    PropertyValue value(getProperty(edge, property, sta));
    const char *prop = value.stringValue();
    if (prop &&
	((exact_match && stringEq(prop, pattern))
         || (not_match && !stringEq(prop, pattern))
	 || (pattern_match && patternMatch(pattern, prop))))
      filtered_edges.push_back(edge);
  }
  delete edges;
  return filtered_edges;
}

const char *
operating_condition_analysis_type()
{
  switch (Sta::sta()->sdc()->analysisType()){
  case AnalysisType::single:
    return "single";
  case AnalysisType::bc_wc:
    return "bc_wc";
  case AnalysisType::ocv:
    return "on_chip_variation";
  }
  // Prevent warnings from lame compilers.
  return "?";
}

void
set_instance_pvt(Instance *inst,
		 const MinMaxAll *min_max,
		 float process,
		 float voltage,
		 float temperature)
{
  cmdLinkedNetwork();
  Pvt pvt(process, voltage, temperature);
  Sta::sta()->setPvt(inst, min_max, pvt);
}

float
port_ext_pin_cap(const Port *port,
                 const Corner *corner,
		 const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  int fanout;
  Sta::sta()->portExtCaps(port, corner, min_max, pin_cap, wire_cap, fanout);
  return pin_cap;
}

void
set_port_ext_pin_cap(const Port *port,
                     const RiseFallBoth *rf,
                     const Corner *corner,
                     const MinMaxAll *min_max,
                     float cap)
{
  Sta::sta()->setPortExtPinCap(port, rf, corner, min_max, cap);
}

float
port_ext_wire_cap(const Port *port,
                  const Corner *corner,
                  const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  int fanout;
  Sta::sta()->portExtCaps(port, corner, min_max, pin_cap, wire_cap, fanout);
  return wire_cap;
}

void
set_port_ext_wire_cap(const Port *port,
                      bool subtract_pin_cap,
                      const RiseFallBoth *rf,
                      const Corner *corner,
                      const MinMaxAll *min_max,
                      float cap)
{
  Sta::sta()->setPortExtWireCap(port, subtract_pin_cap, rf, corner, min_max, cap);
}

void
set_port_ext_fanout_cmd(const Port *port,
			int fanout,
                        const Corner *corner,
			const MinMaxAll *min_max)
{
  Sta::sta()->setPortExtFanout(port, fanout, corner, min_max);
}

float
port_ext_fanout(const Port *port,
                const Corner *corner,
                const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  int fanout;
  Sta::sta()->portExtCaps(port, corner, min_max, pin_cap, wire_cap, fanout);
  return fanout;
}

void
set_net_wire_cap(const Net *net,
		 bool subtract_pin_cap,
		 const Corner *corner,
		 const MinMaxAll *min_max,
		 float cap)
{
  Sta::sta()->setNetWireCap(net, subtract_pin_cap, corner, min_max, cap);
}

void
set_wire_load_mode_cmd(const char *mode_name)
{
  WireloadMode mode = stringWireloadMode(mode_name);
  if (mode == WireloadMode::unknown)
    criticalError(271, "unknown wire load mode");
  else
    Sta::sta()->setWireloadMode(mode);
}

void
set_net_resistance(Net *net,
		   const MinMaxAll *min_max,
		   float res)
{
  Sta::sta()->setResistance(net, min_max, res);
}

void
set_wire_load_cmd(Wireload *wireload,
		  const MinMaxAll *min_max)
{
  Sta::sta()->setWireload(wireload, min_max);
}

void
set_wire_load_selection_group_cmd(WireloadSelection *selection,
				  const MinMaxAll *min_max)
{
  Sta::sta()->setWireloadSelection(selection, min_max);
}

void
make_clock(const char *name,
	   PinSet *pins,
	   bool add_to_pins,
	   float period,
	   FloatSeq *waveform,
	   char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makeClock(name, pins, add_to_pins, period, waveform, comment);
}

void
make_generated_clock(const char *name,
		     PinSet *pins,
		     bool add_to_pins,
		     Pin *src_pin,
		     Clock *master_clk,
		     int divide_by,
		     int multiply_by,
		     float duty_cycle,
		     bool invert,
		     bool combinational,
		     IntSeq *edges,
		     FloatSeq *edge_shifts,
		     char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makeGeneratedClock(name, pins, add_to_pins,
				 src_pin, master_clk,
				 divide_by, multiply_by, duty_cycle, invert,
				 combinational, edges, edge_shifts,
				 comment);
}

void
remove_clock_cmd(Clock *clk)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClock(clk);
}

void
set_propagated_clock_cmd(Clock *clk)
{
  cmdLinkedNetwork();
  Sta::sta()->setPropagatedClock(clk);
}

void
set_propagated_clock_pin_cmd(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->setPropagatedClock(pin);
}

void
unset_propagated_clock_cmd(Clock *clk)
{
  cmdLinkedNetwork();
  Sta::sta()->removePropagatedClock(clk);
}

void
unset_propagated_clock_pin_cmd(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removePropagatedClock(pin);
}

void
set_clock_slew_cmd(Clock *clk,
		   const RiseFallBoth *rf,
		   const MinMaxAll *min_max,
		   float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockSlew(clk, rf, min_max, slew);
}

void
unset_clock_slew_cmd(Clock *clk)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockSlew(clk);
}

void
set_clock_latency_cmd(Clock *clk,
		      Pin *pin,
		      const RiseFallBoth *rf,
		      MinMaxAll *min_max, float delay)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockLatency(clk, pin, rf, min_max, delay);
}

void
set_clock_insertion_cmd(Clock *clk,
			Pin *pin,
			const RiseFallBoth *rf,
			const MinMaxAll *min_max,
			const EarlyLateAll *early_late,
			float delay)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockInsertion(clk, pin, rf, min_max, early_late, delay);
}

void
unset_clock_latency_cmd(Clock *clk,
			Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockLatency(clk, pin);
}

void
unset_clock_insertion_cmd(Clock *clk,
			  Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockInsertion(clk, pin);
}

void
set_clock_uncertainty_clk(Clock *clk,
			  const SetupHoldAll *setup_hold,
			  float uncertainty)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockUncertainty(clk, setup_hold, uncertainty);
}

void
unset_clock_uncertainty_clk(Clock *clk,
			    const SetupHoldAll *setup_hold)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockUncertainty(clk, setup_hold);
}

void
set_clock_uncertainty_pin(Pin *pin,
			  const MinMaxAll *min_max,
			  float uncertainty)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockUncertainty(pin, min_max, uncertainty);
}

void
unset_clock_uncertainty_pin(Pin *pin,
			    const MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockUncertainty(pin, min_max);
}

void
set_inter_clock_uncertainty(Clock *from_clk,
			    const RiseFallBoth *from_tr,
			    Clock *to_clk,
			    const RiseFallBoth *to_tr,
			    const MinMaxAll *min_max,
			    float uncertainty)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockUncertainty(from_clk, from_tr, to_clk, to_tr, min_max,
				  uncertainty);
}

void
unset_inter_clock_uncertainty(Clock *from_clk,
			      const RiseFallBoth *from_tr,
			      Clock *to_clk,
			      const RiseFallBoth *to_tr,
			      const MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockUncertainty(from_clk, from_tr, to_clk, to_tr, min_max);
}

void
set_clock_gating_check_cmd(const RiseFallBoth *rf,
			   const SetupHold *setup_hold,
			   float margin)
{
  Sta::sta()->setClockGatingCheck(rf, setup_hold, margin);
}

void
set_clock_gating_check_clk_cmd(Clock *clk,
			       const RiseFallBoth *rf,
			       const SetupHold *setup_hold,
			       float margin)
{
  Sta::sta()->setClockGatingCheck(clk, rf, setup_hold, margin);
}

void
set_clock_gating_check_pin_cmd(Pin *pin,
			       const RiseFallBoth *rf,
			       const SetupHold *setup_hold,
			       float margin,
			       LogicValue active_value)
{
  Sta::sta()->setClockGatingCheck(pin, rf, setup_hold, margin, active_value);
}

void
set_clock_gating_check_instance_cmd(Instance *inst,
				    const RiseFallBoth *rf,
				    const SetupHold *setup_hold,
				    float margin,
				    LogicValue active_value)
{
  Sta::sta()->setClockGatingCheck(inst, rf, setup_hold, margin, active_value);
}

void
set_data_check_cmd(Pin *from,
		   const RiseFallBoth *from_rf,
		   Pin *to,
		   const RiseFallBoth *to_rf,
		   Clock *clk,
		   const SetupHoldAll *setup_hold,
		   float margin)
{
  Sta::sta()->setDataCheck(from, from_rf, to, to_rf, clk, setup_hold, margin);
}

void
unset_data_check_cmd(Pin *from,
		     const RiseFallBoth *from_tr,
		     Pin *to,
		     const RiseFallBoth *to_tr,
		     Clock *clk,
		     const SetupHoldAll *setup_hold)
{
  Sta::sta()->removeDataCheck(from, from_tr, to, to_tr, clk, setup_hold);
}

void
set_input_delay_cmd(Pin *pin,
		    RiseFallBoth *rf,
		    Clock *clk,
		    RiseFall *clk_rf,
		    Pin *ref_pin,
		    bool source_latency_included,
		    bool network_latency_included,
		    MinMaxAll *min_max,
		    bool add,
		    float delay)
{
  cmdLinkedNetwork();
  Sta::sta()->setInputDelay(pin, rf, clk, clk_rf, ref_pin,
			    source_latency_included, network_latency_included,
			    min_max, add, delay);
}

void
unset_input_delay_cmd(Pin *pin,
		      RiseFallBoth *rf, 
		      Clock *clk,
		      RiseFall *clk_rf, 
		      MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->removeInputDelay(pin, rf, clk, clk_rf, min_max);
}

void
set_output_delay_cmd(Pin *pin,
		     const RiseFallBoth *rf,
		     Clock *clk,
		     const RiseFall *clk_rf,
		     Pin *ref_pin,
		     bool source_latency_included,
		     bool network_latency_included,
		     const MinMaxAll *min_max,
		     bool add,
		     float delay)
{
  cmdLinkedNetwork();
  Sta::sta()->setOutputDelay(pin, rf, clk, clk_rf, ref_pin,
			     source_latency_included, network_latency_included,
			     min_max, add, delay);
}

void
unset_output_delay_cmd(Pin *pin,
		       RiseFallBoth *rf, 
		       Clock *clk,
		       RiseFall *clk_rf, 
		       MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->removeOutputDelay(pin, rf, clk, clk_rf, min_max);
}

void
disable_cell(LibertyCell *cell,
	     LibertyPort *from,
	     LibertyPort *to)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(cell, from, to);
}

void
unset_disable_cell(LibertyCell *cell,
		   LibertyPort *from,
		   LibertyPort *to)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(cell, from, to);
}

void
disable_lib_port(LibertyPort *port)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(port);
}

void
unset_disable_lib_port(LibertyPort *port)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(port);
}

void
disable_port(Port *port)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(port);
}

void
unset_disable_port(Port *port)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(port);
}

void
disable_instance(Instance *instance,
		 LibertyPort *from,
		 LibertyPort *to)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(instance, from, to);
}

void
unset_disable_instance(Instance *instance,
		       LibertyPort *from,
		       LibertyPort *to)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(instance, from, to);
}

void
disable_pin(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(pin);
}

void
unset_disable_pin(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(pin);
}

void
disable_edge(Edge *edge)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(edge);
}

void
unset_disable_edge(Edge *edge)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(edge);
}

void
disable_timing_arc_set(TimingArcSet *arc_set)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(arc_set);
}

void
unset_disable_timing_arc_set(TimingArcSet *arc_set)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(arc_set);
}

void
disable_clock_gating_check_inst(Instance *inst)
{
  cmdLinkedNetwork();
  Sta::sta()->disableClockGatingCheck(inst);
}

void
disable_clock_gating_check_pin(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->disableClockGatingCheck(pin);
}

void
unset_disable_clock_gating_check_inst(Instance *inst)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisableClockGatingCheck(inst);
}

void
unset_disable_clock_gating_check_pin(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisableClockGatingCheck(pin);
}

void
make_false_path(ExceptionFrom *from,
		ExceptionThruSeq *thrus,
		ExceptionTo *to,
		const MinMaxAll *min_max,
		const char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makeFalsePath(from, thrus, to, min_max, comment);
}

void
make_multicycle_path(ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to,
		     const MinMaxAll *min_max,
		     bool use_end_clk,
		     int path_multiplier,
		     const char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makeMulticyclePath(from, thrus, to, min_max, use_end_clk,
				 path_multiplier, comment);
}

void
make_path_delay(ExceptionFrom *from,
		ExceptionThruSeq *thrus,
		ExceptionTo *to,
		const MinMax *min_max,
		bool ignore_clk_latency,
		float delay,
		const char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makePathDelay(from, thrus, to, min_max, 
			    ignore_clk_latency, delay, comment);
}

void
reset_path_cmd(ExceptionFrom *
	       from, ExceptionThruSeq *thrus,
	       ExceptionTo *to,
	       const MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->resetPath(from, thrus, to, min_max);
  // from/to and thru are owned and deleted by the caller.
  // ExceptionThruSeq thrus arg is made by TclListSeqExceptionThru
  // in the swig converter so it is deleted here.
  delete thrus;
}

void
make_group_path(const char *name,
		bool is_default,
		ExceptionFrom *from,
		ExceptionThruSeq *thrus,
		ExceptionTo *to,
		const char *comment)
{
  cmdLinkedNetwork();
  if (name[0] == '\0')
    name = nullptr;
  Sta::sta()->makeGroupPath(name, is_default, from, thrus, to, comment);
}

bool
is_path_group_name(const char *name)
{
  cmdLinkedNetwork();
  return Sta::sta()->isGroupPathName(name);
}

ExceptionFrom *
make_exception_from(PinSet *from_pins,
		    ClockSet *from_clks,
		    InstanceSet *from_insts,
		    const RiseFallBoth *from_tr)
{
  cmdLinkedNetwork();
  return Sta::sta()->makeExceptionFrom(from_pins, from_clks, from_insts,
				       from_tr);
}

void
delete_exception_from(ExceptionFrom *from)
{
  Sta::sta()->deleteExceptionFrom(from);
}

void
check_exception_from_pins(ExceptionFrom *from,
			  const char *file,
			  int line)
{
  Sta::sta()->checkExceptionFromPins(from, file, line);
}

ExceptionThru *
make_exception_thru(PinSet *pins,
		    NetSet *nets,
		    InstanceSet *insts,
		    const RiseFallBoth *rf)
{
  cmdLinkedNetwork();
  return Sta::sta()->makeExceptionThru(pins, nets, insts, rf);
}

void
delete_exception_thru(ExceptionThru *thru)
{
  Sta::sta()->deleteExceptionThru(thru);
}

ExceptionTo *
make_exception_to(PinSet *to_pins,
		  ClockSet *to_clks,
		  InstanceSet *to_insts,
		  const RiseFallBoth *rf,
 		  RiseFallBoth *end_rf)
{
  cmdLinkedNetwork();
  return Sta::sta()->makeExceptionTo(to_pins, to_clks, to_insts, rf, end_rf);
}

void
delete_exception_to(ExceptionTo *to)
{
  Sta::sta()->deleteExceptionTo(to);
}

void
check_exception_to_pins(ExceptionTo *to,
			const char *file,
			int line)
{
  Sta::sta()->checkExceptionToPins(to, file, line);
}

void
set_input_slew_cmd(Port *port,
		   const RiseFallBoth *rf,
		   const MinMaxAll *min_max,
		   float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setInputSlew(port, rf, min_max, slew);
}

void
set_drive_cell_cmd(LibertyLibrary *library,
		   LibertyCell *cell,
		   Port *port,
		   LibertyPort *from_port,
		   float from_slew_rise,
		   float from_slew_fall,
		   LibertyPort *to_port,
		   const RiseFallBoth *rf,
		   const MinMaxAll *min_max)
{
  float from_slews[RiseFall::index_count];
  from_slews[RiseFall::riseIndex()] = from_slew_rise;
  from_slews[RiseFall::fallIndex()] = from_slew_fall;
  Sta::sta()->setDriveCell(library, cell, port, from_port, from_slews,
			   to_port, rf, min_max);
}

void
set_drive_resistance_cmd(Port *port,
			 const RiseFallBoth *rf,
			 const MinMaxAll *min_max,
			 float res)
{
  cmdLinkedNetwork();
  Sta::sta()->setDriveResistance(port, rf, min_max, res);
}

void
set_slew_limit_clk(Clock *clk,
		   const RiseFallBoth *rf,
		   PathClkOrData clk_data,
		   const MinMax *min_max,
		   float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setSlewLimit(clk, rf, clk_data, min_max, slew);
}

void
set_slew_limit_port(Port *port,
		    const MinMax *min_max,
		    float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setSlewLimit(port, min_max, slew);
}

void
set_slew_limit_cell(Cell *cell,
		    const MinMax *min_max,
		    float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setSlewLimit(cell, min_max, slew);
}

void
set_port_capacitance_limit(Port *port,
			   const MinMax *min_max,
			   float cap)
{
  Sta::sta()->setCapacitanceLimit(port, min_max, cap);
}

void
set_pin_capacitance_limit(Pin *pin,
			  const MinMax *min_max,
			  float cap)
{
  Sta::sta()->setCapacitanceLimit(pin, min_max, cap);
}

void
set_cell_capacitance_limit(Cell *cell,
			   const MinMax *min_max,
			   float cap)
{
  Sta::sta()->setCapacitanceLimit(cell, min_max, cap);
}

void
set_latch_borrow_limit_pin(Pin *pin,
			   float limit)
{
  Sta::sta()->setLatchBorrowLimit(pin, limit);
}

void
set_latch_borrow_limit_inst(Instance *inst,
			    float limit)
{
  Sta::sta()->setLatchBorrowLimit(inst, limit);
}

void
set_latch_borrow_limit_clk(Clock *clk, float limit)
{
  Sta::sta()->setLatchBorrowLimit(clk, limit);
}

void
set_min_pulse_width_global(const RiseFallBoth *rf,
			   float min_width)
{
  Sta::sta()->setMinPulseWidth(rf, min_width);
}

void
set_min_pulse_width_pin(Pin *pin,
			const RiseFallBoth *rf,
			float min_width)
{
  Sta::sta()->setMinPulseWidth(pin, rf, min_width);
}

void
set_min_pulse_width_clk(Clock *clk,
			const RiseFallBoth *rf,
			float min_width)
{
  Sta::sta()->setMinPulseWidth(clk, rf, min_width);
}

void
set_min_pulse_width_inst(Instance *inst,
			 const RiseFallBoth *rf,
			 float min_width)
{
  Sta::sta()->setMinPulseWidth(inst, rf, min_width);
}

void
set_max_area_cmd(float area)
{
  Sta::sta()->setMaxArea(area);
}

void
set_port_fanout_limit(Port *port,
		      const MinMax *min_max,
		      float fanout)
{
  Sta::sta()->setFanoutLimit(port, min_max, fanout);
}

void
set_cell_fanout_limit(Cell *cell,
		      const MinMax *min_max,
		      float fanout)
{
  Sta::sta()->setFanoutLimit(cell, min_max, fanout);
}

void
set_logic_value_cmd(Pin *pin,
		    LogicValue value)
{
  Sta::sta()->setLogicValue(pin, value);
}

void
set_case_analysis_cmd(Pin *pin,
		      LogicValue value)
{
  Sta::sta()->setCaseAnalysis(pin, value);
}

void
unset_case_analysis_cmd(Pin *pin)
{
  Sta::sta()->removeCaseAnalysis(pin);
}

void
set_timing_derate_cmd(TimingDerateType type,
		      PathClkOrData clk_data,
		      const RiseFallBoth *rf,
		      const EarlyLate *early_late,
		      float derate)
{
  Sta::sta()->setTimingDerate(type, clk_data, rf, early_late, derate);
}

void
set_timing_derate_net_cmd(const Net *net,
			  PathClkOrData clk_data,
			  const RiseFallBoth *rf,
			  const EarlyLate *early_late,
			  float derate)
{
  Sta::sta()->setTimingDerate(net, clk_data, rf, early_late, derate);
}

void
set_timing_derate_inst_cmd(const Instance *inst,
			   TimingDerateCellType type,
			   PathClkOrData clk_data,
			   const RiseFallBoth *rf,
			   const EarlyLate *early_late,
			   float derate)
{
  Sta::sta()->setTimingDerate(inst, type, clk_data, rf, early_late, derate);
}

void
set_timing_derate_cell_cmd(const LibertyCell *cell,
			   TimingDerateCellType type,
			   PathClkOrData clk_data,
			   const RiseFallBoth *rf,
			   const EarlyLate *early_late,
			   float derate)
{
  Sta::sta()->setTimingDerate(cell, type, clk_data, rf, early_late, derate);
}

void
unset_timing_derate_cmd()
{
  Sta::sta()->unsetTimingDerate();
}

Clock *
find_clock(const char *name)
{
  cmdLinkedNetwork();
  return Sta::sta()->sdc()->findClock(name);
}

bool
is_clock_src(const Pin *pin)
{
  return Sta::sta()->isClockSrc(pin);
}

Clock *
default_arrival_clock()
{
  return Sta::sta()->sdc()->defaultArrivalClock();
}

ClockSeq
find_clocks_matching(const char *pattern,
		     bool regexp,
		     bool nocase)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  PatternMatch matcher(pattern, regexp, nocase, sta->tclInterp());
  return sdc->findClocksMatching(&matcher);
}

void
update_generated_clks()
{
  cmdLinkedNetwork();
  Sta::sta()->updateGeneratedClks();
}

bool
is_clock(Pin *pin)
{
  Sta *sta = Sta::sta();
  return sta->isClock(pin);
}

bool
is_ideal_clock(Pin *pin)
{
  Sta *sta = Sta::sta();
  return sta->isIdealClock(pin);
}

bool
is_clock_search(const Pin *pin)
{
  Sta *sta = Sta::sta();
  Graph *graph = sta->graph();
  Search *search = sta->search();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  return search->isClock(vertex);
}

bool
is_genclk_src(const Pin *pin)
{
  Sta *sta = Sta::sta();
  Graph *graph = sta->graph();
  Search *search = sta->search();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  return search->isGenClkSrc(vertex);
}

// format_unit functions print with fixed digits and suffix.
// Pass value arg as string to support NaNs.
const char *
format_time(const char *value,
	    int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->timeUnit()->asString(value1, digits);
}

const char *
format_capacitance(const char *value,
		   int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->capacitanceUnit()->asString(value1, digits);
}

const char *
format_resistance(const char *value,
		  int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->resistanceUnit()->asString(value1, digits);
}

const char *
format_voltage(const char *value,
	       int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->voltageUnit()->asString(value1, digits);
}

const char *
format_current(const char *value,
	       int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->currentUnit()->asString(value1, digits);
}

const char *
format_power(const char *value,
	     int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->powerUnit()->asString(value1, digits);
}

const char *
format_distance(const char *value,
		int digits)
{
  float value1 = strtof(value, nullptr);
  Unit *dist_unit = Sta::sta()->units()->distanceUnit();
  return dist_unit->asString(value1, digits);
}

const char *
format_area(const char *value,
	    int digits)
{
  float value1 = strtof(value, nullptr);
  Unit *dist_unit = Sta::sta()->units()->distanceUnit();
  return dist_unit->asString(value1 / dist_unit->scale(), digits);
}

////////////////////////////////////////////////////////////////

// <unit>_sta_ui conversion from sta units to user interface units.
// <unit>_ui_sta conversion from user interface units to sta units.

double
time_ui_sta(double value)
{
  return Sta::sta()->units()->timeUnit()->userToSta(value);
}

double
time_sta_ui(double value)
{
  return Sta::sta()->units()->timeUnit()->staToUser(value);
}

double
capacitance_ui_sta(double value)
{
  return Sta::sta()->units()->capacitanceUnit()->userToSta(value);
}

double
capacitance_sta_ui(double value)
{
  return Sta::sta()->units()->capacitanceUnit()->staToUser(value);
}

double
resistance_ui_sta(double value)
{
  return Sta::sta()->units()->resistanceUnit()->userToSta(value);
}

double
resistance_sta_ui(double value)
{
  return Sta::sta()->units()->resistanceUnit()->staToUser(value);
}

double
voltage_ui_sta(double value)
{
  return Sta::sta()->units()->voltageUnit()->userToSta(value);
}

double
voltage_sta_ui(double value)
{
  return Sta::sta()->units()->voltageUnit()->staToUser(value);
}

double
current_ui_sta(double value)
{
  return Sta::sta()->units()->currentUnit()->userToSta(value);
}

double
current_sta_ui(double value)
{
  return Sta::sta()->units()->currentUnit()->staToUser(value);
}

double
power_ui_sta(double value)
{
  return Sta::sta()->units()->powerUnit()->userToSta(value);
}

double
power_sta_ui(double value)
{
  return Sta::sta()->units()->powerUnit()->staToUser(value);
}

double
distance_ui_sta(double value)
{
  return Sta::sta()->units()->distanceUnit()->userToSta(value);
}

double
distance_sta_ui(double value)
{
  return Sta::sta()->units()->distanceUnit()->staToUser(value);
}

double
area_ui_sta(double value)
{
  double scale = Sta::sta()->units()->distanceUnit()->scale();
  return value * scale * scale;
}

double
area_sta_ui(double value)
{
  double scale = Sta::sta()->units()->distanceUnit()->scale();
  return value / (scale * scale);
}

////////////////////////////////////////////////////////////////

void
set_cmd_unit_scale(const char *unit_name,
		   float scale)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    unit->setScale(scale);
}

void
set_cmd_unit_digits(const char *unit_name,
		    int digits)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    unit->setDigits(digits);
}

void
set_cmd_unit_suffix(const char *unit_name,
		    const char *suffix)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit) {
    unit->setSuffix(suffix);
  }
}

const char *
unit_scale_abbreviation (const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->scaleAbbreviation();
  else
    return "";
}

const char *
unit_suffix(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->suffix();
  else
    return "";
}

const char *
unit_scaled_suffix(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->scaledSuffix();
  else
    return "";
}

////////////////////////////////////////////////////////////////

VertexIterator *
vertex_iterator()
{
  return new VertexIterator(cmdGraph());
}

void
set_arc_delay(Edge *edge,
	      TimingArc *arc,
	      const Corner *corner,
	      const MinMaxAll *min_max,
	      float delay)
{
  cmdGraph();
  Sta::sta()->setArcDelay(edge, arc, corner, min_max, delay);
}

void
set_annotated_slew(Vertex *vertex,
		   const Corner *corner,
		   const MinMaxAll *min_max,
		   const RiseFallBoth *rf,
		   float slew)
{
  cmdGraph();
  Sta::sta()->setAnnotatedSlew(vertex, corner, min_max, rf, slew);
}

// Remove all delay and slew annotations.
void
remove_delay_slew_annotations()
{
  cmdGraph();
  Sta::sta()->removeDelaySlewAnnotations();
}

CheckErrorSeq &
check_timing_cmd(bool no_input_delay,
		 bool no_output_delay,
		 bool reg_multiple_clks,
		 bool reg_no_clks,
		 bool unconstrained_endpoints,
		 bool loops,
		 bool generated_clks)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkTiming(no_input_delay, no_output_delay,
				 reg_multiple_clks, reg_no_clks,
				 unconstrained_endpoints,
				 loops, generated_clks);
}

bool
crpr_enabled()
{
  return Sta::sta()->crprEnabled();
}

void
set_crpr_enabled(bool enabled)
{
  return Sta::sta()->setCrprEnabled(enabled);
}

const char *
crpr_mode()
{
  switch (Sta::sta()->crprMode()) {
  case CrprMode::same_transition:
    return "same_transition";
  case CrprMode::same_pin:
    return "same_pin";
  default:
    return "";
  }
}

void
set_crpr_mode(const char *mode)
{
  Sta *sta = Sta::sta();
  if (stringEq(mode, "same_pin"))
    Sta::sta()->setCrprMode(CrprMode::same_pin);
  else if (stringEq(mode, "same_transition"))
    Sta::sta()->setCrprMode(CrprMode::same_transition);
  else
    sta->report()->critical(1573, "unknown common clk pessimism mode.");
}

bool
pocv_enabled()
{
  return Sta::sta()->pocvEnabled();
}

void
set_pocv_enabled(bool enabled)
{
#if !SSTA
  if (enabled)
    Sta::sta()->report()->error(1574, "POCV support requires compilation with SSTA=1.");
#endif
  return Sta::sta()->setPocvEnabled(enabled);
}

float
pocv_sigma_factor()
{
  return Sta::sta()->sigmaFactor();
}

void
set_pocv_sigma_factor(float factor)
{
  Sta::sta()->setSigmaFactor(factor);
}

bool
propagate_gated_clock_enable()
{
  return Sta::sta()->propagateGatedClockEnable();
}

void
set_propagate_gated_clock_enable(bool enable)
{
  Sta::sta()->setPropagateGatedClockEnable(enable);
}

bool
preset_clr_arcs_enabled()
{
  return Sta::sta()->presetClrArcsEnabled();
}

void
set_preset_clr_arcs_enabled(bool enable)
{
  Sta::sta()->setPresetClrArcsEnabled(enable);
}

bool
cond_default_arcs_enabled()
{
  return Sta::sta()->condDefaultArcsEnabled();
}

void
set_cond_default_arcs_enabled(bool enabled)
{
  Sta::sta()->setCondDefaultArcsEnabled(enabled);
}

bool
bidirect_inst_paths_enabled()
{
  return Sta::sta()->bidirectInstPathsEnabled();
}

void
set_bidirect_inst_paths_enabled(bool enabled)
{
  Sta::sta()->setBidirectInstPathsEnabled(enabled);
}

bool
bidirect_net_paths_enabled()
{
  return Sta::sta()->bidirectNetPathsEnabled();
}

void
set_bidirect_net_paths_enabled(bool enabled)
{
  Sta::sta()->setBidirectNetPathsEnabled(enabled);
}

bool
recovery_removal_checks_enabled()
{
  return Sta::sta()->recoveryRemovalChecksEnabled();
}

void
set_recovery_removal_checks_enabled(bool enabled)
{
  Sta::sta()->setRecoveryRemovalChecksEnabled(enabled);
}

bool
gated_clk_checks_enabled()
{
  return Sta::sta()->gatedClkChecksEnabled();
}

void
set_gated_clk_checks_enabled(bool enabled)
{
  Sta::sta()->setGatedClkChecksEnabled(enabled);
}

bool
dynamic_loop_breaking()
{
  return Sta::sta()->dynamicLoopBreaking();
}

void
set_dynamic_loop_breaking(bool enable)
{
  Sta::sta()->setDynamicLoopBreaking(enable);
}

bool
use_default_arrival_clock()
{
  return Sta::sta()->useDefaultArrivalClock();
}

void
set_use_default_arrival_clock(bool enable)
{
  return Sta::sta()->setUseDefaultArrivalClock(enable);
}

bool
propagate_all_clocks()
{
  return Sta::sta()->propagateAllClocks();
}

void
set_propagate_all_clocks(bool prop)
{
  Sta::sta()->setPropagateAllClocks(prop);
}

////////////////////////////////////////////////////////////////

PathEndSeq
find_path_ends(ExceptionFrom *from,
	       ExceptionThruSeq *thrus,
	       ExceptionTo *to,
	       bool unconstrained,
	       Corner *corner,
	       const MinMaxAll *delay_min_max,
	       int group_count,
	       int endpoint_count,
	       bool unique_pins,
	       float slack_min,
	       float slack_max,
	       bool sort_by_slack,
	       PathGroupNameSet *groups,
	       bool setup,
	       bool hold,
	       bool recovery,
	       bool removal,
	       bool clk_gating_setup,
	       bool clk_gating_hold)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  PathEndSeq ends = sta->findPathEnds(from, thrus, to, unconstrained,
                                      corner, delay_min_max,
                                      group_count, endpoint_count, unique_pins,
                                      slack_min, slack_max,
                                      sort_by_slack,
                                      groups->size() ? groups : nullptr,
                                      setup, hold,
                                      recovery, removal,
                                      clk_gating_setup, clk_gating_hold);
  delete groups;
  return ends;
}

void
report_path_end_header()
{
  Sta::sta()->reportPathEndHeader();
}

void
report_path_end_footer()
{
  Sta::sta()->reportPathEndFooter();
}

void
report_path_end(PathEnd *end)
{
  Sta::sta()->reportPathEnd(end);
}

void
report_path_end2(PathEnd *end,
		 PathEnd *prev_end)
{
  Sta::sta()->reportPathEnd(end, prev_end);
}

void
set_report_path_format(ReportPathFormat format)
{
  Sta::sta()->setReportPathFormat(format);
}
    
void
set_report_path_field_order(StringSeq *field_names)
{
  Sta::sta()->setReportPathFieldOrder(field_names);
  delete field_names;
}

void
set_report_path_fields(bool report_input_pin,
		       bool report_net,
		       bool report_cap,
		       bool report_slew,
                       bool report_fanout)
{
  Sta::sta()->setReportPathFields(report_input_pin,
				  report_net,
				  report_cap,
				  report_slew,
                                  report_fanout);
}

void
set_report_path_field_properties(const char *field_name,
				 const char *title,
				 int width,
				 bool left_justify)
{
  Sta *sta = Sta::sta();
  ReportField *field = sta->findReportPathField(field_name);
  if (field)
    field->setProperties(title, width, left_justify);
  else
    sta->report()->error(1575, "unknown report path field %s", field_name);
}

void
set_report_path_field_width(const char *field_name,
			    int width)
{
  Sta *sta = Sta::sta();
  ReportField *field = sta->findReportPathField(field_name);
  if (field)
    field->setWidth(width);
  else
    sta->report()->error(1576, "unknown report path field %s", field_name);
}

void
set_report_path_digits(int digits)
{
  Sta::sta()->setReportPathDigits(digits);
}

void
set_report_path_no_split(bool no_split)
{
  Sta::sta()->setReportPathNoSplit(no_split);
}

void
set_report_path_sigmas(bool report_sigmas)
{
  Sta::sta()->setReportPathSigmas(report_sigmas);
}

void
delete_path_ref(PathRef *path)
{
  delete path;
}

void
remove_constraints()
{
  Sta::sta()->removeConstraints();
}

void
report_path_cmd(PathRef *path)
{
  Sta::sta()->reportPath(path);
}

////////////////////////////////////////////////////////////////

void
report_clk_skew(ConstClockSeq clks,
		const Corner *corner,
		const SetupHold *setup_hold,
                bool include_internal_latency,
		int digits)
{
  cmdLinkedNetwork();
  Sta::sta()->reportClkSkew(clks, corner, setup_hold,
                            include_internal_latency, digits);
}

void
report_clk_latency(ConstClockSeq clks,
                   const Corner *corner,
                   bool include_internal_latency,
                   int digits)
{
  cmdLinkedNetwork();
  Sta::sta()->reportClkLatency(clks, corner, include_internal_latency, digits);
}

float
worst_clk_skew_cmd(const SetupHold *setup_hold,
                   bool include_internal_latency)
{
  cmdLinkedNetwork();
  return Sta::sta()->findWorstClkSkew(setup_hold, include_internal_latency);
}

////////////////////////////////////////////////////////////////

PinSet
startpoints()
{
  return Sta::sta()->startpointPins();
}

PinSet
endpoints()
{
  return Sta::sta()->endpointPins();
}

size_t
endpoint_count()
{
  return Sta::sta()->endpointPins().size();
}

PinSet
group_path_pins(const char *group_path_name)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  if (sdc->isGroupPathName(group_path_name))
    return sta->findGroupPathPins(group_path_name);
  else
    return PinSet(sta->network());
}

////////////////////////////////////////////////////////////////

MinPulseWidthCheckSeq &
min_pulse_width_violations(const Corner *corner)
{
  cmdLinkedNetwork();
  return Sta::sta()->minPulseWidthViolations(corner);
}

MinPulseWidthCheckSeq &
min_pulse_width_check_pins(PinSeq *pins,
			   const Corner *corner)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  MinPulseWidthCheckSeq &checks = sta->minPulseWidthChecks(pins, corner);
  delete pins;
  return checks;
}

MinPulseWidthCheckSeq &
min_pulse_width_checks(const Corner *corner)
{
  cmdLinkedNetwork();
  return Sta::sta()->minPulseWidthChecks(corner);
}

MinPulseWidthCheck *
min_pulse_width_check_slack(const Corner *corner)
{
  cmdLinkedNetwork();
  return Sta::sta()->minPulseWidthSlack(corner);
}

void
report_mpw_checks(MinPulseWidthCheckSeq *checks,
		  bool verbose)
{
  Sta::sta()->reportMpwChecks(checks, verbose);
}

void
report_mpw_check(MinPulseWidthCheck *check,
		 bool verbose)
{
  Sta::sta()->reportMpwCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

MinPeriodCheckSeq &
min_period_violations()
{
  cmdLinkedNetwork();
  return Sta::sta()->minPeriodViolations();
}

MinPeriodCheck *
min_period_check_slack()
{
  cmdLinkedNetwork();
  return Sta::sta()->minPeriodSlack();
}

void
report_min_period_checks(MinPeriodCheckSeq *checks,
			 bool verbose)
{
  Sta::sta()->reportChecks(checks, verbose);
}

void
report_min_period_check(MinPeriodCheck *check,
			bool verbose)
{
  Sta::sta()->reportCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

MaxSkewCheckSeq &
max_skew_violations()
{
  cmdLinkedNetwork();
  return Sta::sta()->maxSkewViolations();
}

MaxSkewCheck *
max_skew_check_slack()
{
  cmdLinkedNetwork();
  return Sta::sta()->maxSkewSlack();
}

void
report_max_skew_checks(MaxSkewCheckSeq *checks,
		       bool verbose)
{
  Sta::sta()->reportChecks(checks, verbose);
}

void
report_max_skew_check(MaxSkewCheck *check,
		      bool verbose)
{
  Sta::sta()->reportCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

void
find_timing_cmd(bool full)
{
  cmdLinkedNetwork();
  Sta::sta()->updateTiming(full);
}

void
find_requireds()
{
  cmdLinkedNetwork();
  Sta::sta()->findRequireds();
}

void
find_delays()
{
  cmdLinkedNetwork();
  Sta::sta()->findDelays();
}

Slack
total_negative_slack_cmd(const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  return sta->totalNegativeSlack(min_max);
}

Slack
total_negative_slack_corner_cmd(const Corner *corner,
				const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  return sta->totalNegativeSlack(corner, min_max);
}

Slack
worst_slack_cmd(const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  Slack worst_slack;
  Vertex *worst_vertex;
  sta->worstSlack(min_max, worst_slack, worst_vertex);
  return worst_slack;
}

Vertex *
worst_slack_vertex(const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  Slack worst_slack;
  Vertex *worst_vertex;
  sta->worstSlack(min_max, worst_slack, worst_vertex);
  return worst_vertex;;
}

Slack
worst_slack_corner(const Corner *corner,
		   const MinMax *min_max)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  Slack worst_slack;
  Vertex *worst_vertex;
  sta->worstSlack(corner, min_max, worst_slack, worst_vertex);
  return worst_slack;
}

PathRef *
vertex_worst_arrival_path(Vertex *vertex,
			  const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  PathRef path = sta->vertexWorstArrivalPath(vertex, min_max);
  if (!path.isNull())
    return new PathRef(path);
  else
    return nullptr;
}

PathRef *
vertex_worst_arrival_path_rf(Vertex *vertex,
			     const RiseFall *rf,
			     MinMax *min_max)
{
  Sta *sta = Sta::sta();
  PathRef path = sta->vertexWorstArrivalPath(vertex, rf, min_max);
  if (!path.isNull())
    return new PathRef(path);
  else
    return nullptr;
}

PathRef *
vertex_worst_slack_path(Vertex *vertex,
			const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  PathRef path = sta->vertexWorstSlackPath(vertex, min_max);
  if (!path.isNull())
    return new PathRef(path);
  else
    return nullptr;
}

Slack
find_clk_min_period(const Clock *clk,
                    bool ignore_port_paths)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  return sta->findClkMinPeriod(clk, ignore_port_paths);
}

////////////////////////////////////////////////////////////////

PinSeq
check_slew_limits(Net *net,
                  bool violators,
                  const Corner *corner,
                  const MinMax *min_max)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkSlewLimits(net, violators, corner, min_max);
}

size_t
max_slew_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkSlewLimits(nullptr, true, nullptr, MinMax::max()).size();
}

float
max_slew_check_slack()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  Slew slew;
  float slack;
  float limit;
  sta->maxSlewCheck(pin, slew, slack, limit);
  return sta->units()->timeUnit()->staToUser(slack);
}

float
max_slew_check_limit()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  Slew slew;
  float slack;
  float limit;
  sta->maxSlewCheck(pin, slew, slack, limit);
  return sta->units()->timeUnit()->staToUser(limit);
}

void
report_slew_limit_short_header()
{
  Sta::sta()->reportSlewLimitShortHeader();
}

void
report_slew_limit_short(Pin *pin,
			const Corner *corner,
			const MinMax *min_max)
{
  Sta::sta()->reportSlewLimitShort(pin, corner, min_max);
}

void
report_slew_limit_verbose(Pin *pin,
			  const Corner *corner,
			  const MinMax *min_max)
{
  Sta::sta()->reportSlewLimitVerbose(pin, corner, min_max);
}

////////////////////////////////////////////////////////////////

PinSeq
check_fanout_limits(Net *net,
                    bool violators,
                    const MinMax *min_max)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkFanoutLimits(net, violators, min_max);
}

size_t
max_fanout_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkFanoutLimits(nullptr, true, MinMax::max()).size();
}

float
max_fanout_check_slack()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  float fanout;
  float slack;
  float limit;
  sta->maxFanoutCheck(pin, fanout, slack, limit);
  return slack;;
}

float
max_fanout_check_limit()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  float fanout;
  float slack;
  float limit;
  sta->maxFanoutCheck(pin, fanout, slack, limit);
  return limit;;
}

void
report_fanout_limit_short_header()
{
  Sta::sta()->reportFanoutLimitShortHeader();
}

void
report_fanout_limit_short(Pin *pin,
			  const MinMax *min_max)
{
  Sta::sta()->reportFanoutLimitShort(pin, min_max);
}

void
report_fanout_limit_verbose(Pin *pin,
			    const MinMax *min_max)
{
  Sta::sta()->reportFanoutLimitVerbose(pin, min_max);
}

////////////////////////////////////////////////////////////////

PinSeq
check_capacitance_limits(Net *net,
                         bool violators,
                         const Corner *corner,
                         const MinMax *min_max)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkCapacitanceLimits(net, violators, corner, min_max);
}

size_t
max_capacitance_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkCapacitanceLimits(nullptr, true,nullptr,MinMax::max()).size();
}

float
max_capacitance_check_slack()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  float capacitance;
  float slack;
  float limit;
  sta->maxCapacitanceCheck(pin, capacitance, slack, limit);
  return sta->units()->capacitanceUnit()->staToUser(slack);
}

float
max_capacitance_check_limit()
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  const Pin *pin;
  float capacitance;
  float slack;
  float limit;
  sta->maxCapacitanceCheck(pin, capacitance, slack, limit);
  return sta->units()->capacitanceUnit()->staToUser(limit);
}

void
report_capacitance_limit_short_header()
{
  Sta::sta()->reportCapacitanceLimitShortHeader();
}

void
report_capacitance_limit_short(Pin *pin,
			       const Corner *corner,
			       const MinMax *min_max)
{
  Sta::sta()->reportCapacitanceLimitShort(pin, corner, min_max);
}

void
report_capacitance_limit_verbose(Pin *pin,
				 const Corner *corner,
				 const MinMax *min_max)
{
  Sta::sta()->reportCapacitanceLimitVerbose(pin, corner, min_max);
}

////////////////////////////////////////////////////////////////

EdgeSeq
disabled_edges_sorted()
{
  cmdLinkedNetwork();
  return Sta::sta()->disabledEdgesSorted();
}

void
write_sdc_cmd(const char *filename,
	      bool leaf,
	      bool compatible,
	      int digits,
              bool gzip,
	      bool no_timestamp)
{
  cmdLinkedNetwork();
  Sta::sta()->writeSdc(filename, leaf, compatible, digits, gzip, no_timestamp);
}

void
write_path_spice_cmd(PathRef *path,
		     const char *spice_filename,
		     const char *subckt_filename,
		     const char *lib_subckt_filename,
		     const char *model_filename,
		     const char *power_name,
		     const char *gnd_name,
                     CircuitSim ckt_sim)
{
  Sta *sta = Sta::sta();
  writePathSpice(path, spice_filename, subckt_filename,
		 lib_subckt_filename, model_filename,
		 power_name, gnd_name, ckt_sim, sta);
}

void
write_timing_model_cmd(const char *lib_name,
                       const char *cell_name,
                       const char *filename,
                       const Corner *corner)
{
  Sta::sta()->writeTimingModel(lib_name, cell_name, filename, corner);
}

////////////////////////////////////////////////////////////////

bool
liberty_supply_exists(const char *supply_name)
{
  auto network = Sta::sta()->network();
  auto lib = network->defaultLibertyLibrary();
  return lib && lib->supplyExists(supply_name);
}

float
unit_scale(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->scale();
  else
    return 1.0F;
}

bool
fuzzy_equal(float value1,
	    float value2)
{
  return fuzzyEqual(value1, value2);
}

char
pin_sim_logic_value(const Pin *pin)
{
  return logicValueString(Sta::sta()->simLogicValue(pin));
}

char
pin_case_logic_value(const Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  LogicValue value = LogicValue::unknown;
  bool exists;
  sdc->caseLogicValue(pin, value, exists);
  return logicValueString(value);
}

char
pin_logic_value(const Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  LogicValue value = LogicValue::unknown;
  bool exists;
  sdc->logicValue(pin, value, exists);
  return logicValueString(value);
}

InstanceSeq
slow_drivers(int count)
{
  return Sta::sta()->slowDrivers(count);
}

bool
timing_arc_disabled(Edge *edge,
		    TimingArc *arc)
{
  Graph *graph = Sta::sta()->graph();
  return !searchThru(edge, arc, graph);
}

ClockGroups *
make_clock_groups(const char *name,
		  bool logically_exclusive,
		  bool physically_exclusive,
		  bool asynchronous,
		  bool allow_paths,
		  const char *comment)
{
  return Sta::sta()->makeClockGroups(name, logically_exclusive,
				     physically_exclusive, asynchronous,
				     allow_paths, comment);
}

void
clock_groups_make_group(ClockGroups *clk_groups,
			ClockSet *clks)
{
  Sta::sta()->makeClockGroup(clk_groups, clks);
}

void
unset_clock_groups_logically_exclusive(const char *name)
{
  Sta::sta()->removeClockGroupsLogicallyExclusive(name);
}

void
unset_clock_groups_physically_exclusive(const char *name)
{
  Sta::sta()->removeClockGroupsPhysicallyExclusive(name);
}

void
unset_clock_groups_asynchronous(const char *name)
{
  Sta::sta()->removeClockGroupsAsynchronous(name);
}

// Debugging function.
bool
same_clk_group(Clock *clk1,
	       Clock *clk2)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  return sdc->sameClockGroupExplicit(clk1, clk2);
}

void
set_clock_sense_cmd(PinSet *pins,
		    ClockSet *clks,
		    bool positive,
		    bool negative,
		    bool stop_propagation)
{
  Sta *sta = Sta::sta();
  if (positive)
    sta->setClockSense(pins, clks, ClockSense::positive);
  else if (negative)
    sta->setClockSense(pins, clks, ClockSense::negative);
  else if (stop_propagation)
    sta->setClockSense(pins, clks, ClockSense::stop);
  else
    sta->report()->critical(1577, "unknown clock sense");
}

bool
timing_role_is_check(TimingRole *role)
{
  return role->isTimingCheck();
}

////////////////////////////////////////////////////////////////

PinSet
find_fanin_pins(PinSeq *to,
		bool flat,
		bool startpoints_only,
		int inst_levels,
		int pin_levels,
		bool thru_disabled,
		bool thru_constants)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  PinSet fanin = sta->findFaninPins(to, flat, startpoints_only,
                                    inst_levels, pin_levels,
                                    thru_disabled, thru_constants);
  delete to;
  return fanin;
}

InstanceSet
find_fanin_insts(PinSeq *to,
		 bool flat,
		 bool startpoints_only,
		 int inst_levels,
		 int pin_levels,
		 bool thru_disabled,
		 bool thru_constants)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  InstanceSet fanin = sta->findFaninInstances(to, flat, startpoints_only,
                                              inst_levels, pin_levels,
                                              thru_disabled, thru_constants);
  delete to;
  return fanin;
}

PinSet
find_fanout_pins(PinSeq *from,
		 bool flat,
		 bool endpoints_only,
		 int inst_levels,
		 int pin_levels,
		 bool thru_disabled,
		 bool thru_constants)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  PinSet fanout = sta->findFanoutPins(from, flat, endpoints_only,
                                      inst_levels, pin_levels,
                                      thru_disabled, thru_constants);
  delete from;
  return fanout;
}

InstanceSet
find_fanout_insts(PinSeq *from,
		  bool flat,
		  bool endpoints_only,
		  int inst_levels,
		  int pin_levels,
		  bool thru_disabled,
		  bool thru_constants)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  InstanceSet fanout = sta->findFanoutInstances(from, flat, endpoints_only,
                                                inst_levels, pin_levels,
                                                thru_disabled, thru_constants);
  delete from;
  return fanout;
}

PinSet
net_load_pins(Net *net)
{
  Network *network = cmdLinkedNetwork();
  PinSet pins(network);
  NetConnectedPinIterator *pin_iter = network->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network->isLoad(pin))
      pins.insert(pin);
  }
  delete pin_iter;
  return pins;
}

PinSet
net_driver_pins(Net *net)
{
  Network *network = cmdLinkedNetwork();
  PinSet pins(network);
  NetConnectedPinIterator *pin_iter = network->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network->isDriver(pin))
      pins.insert(pin);
  }
  delete pin_iter;
  return pins;
}

////////////////////////////////////////////////////////////////

void
report_loops()
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  Graph *graph = cmdGraph();
  Report *report = sta->report();
  for (GraphLoop *loop : *sta->graphLoops()) {
    loop->report(report, network, graph);
    report->reportLineString("");
  }
}

// Includes top instance.
int
network_instance_count()
{
  Network *network = cmdNetwork();
  return network->instanceCount();
}

int
network_pin_count()
{
  Network *network = cmdNetwork();
  return network->pinCount();
}

int
network_net_count()
{
  Network *network = cmdNetwork();
  return network->netCount();
}

int
network_leaf_instance_count()
{
  Network *network = cmdNetwork();
  return network->leafInstanceCount();
}

int
network_leaf_pin_count()
{
  Network *network = cmdNetwork();
  return network->leafPinCount();
}

int
graph_vertex_count()
{
  return cmdGraph()->vertexCount();
}

int
graph_edge_count()
{
  return cmdGraph()->edgeCount();
}

int
graph_arc_count()
{
  return cmdGraph()->arcCount();
}

int
tag_group_count()
{
  return Sta::sta()->tagGroupCount();
}

void
report_tag_groups()
{
  Sta::sta()->search()->reportTagGroups();
}

void
report_tag_arrivals_cmd(Vertex *vertex)
{
  Sta::sta()->search()->reportArrivals(vertex);
}

void
report_arrival_count_histogram()
{
  Sta::sta()->search()->reportArrivalCountHistogram();
}

int
tag_count()
{
  return Sta::sta()->tagCount();
}

void
report_tags()
{
  Sta::sta()->search()->reportTags();
}

void
report_clk_infos()
{
  Sta::sta()->search()->reportClkInfos();
}

int
clk_info_count()
{
  return Sta::sta()->clkInfoCount();
}

int
arrival_count()
{
  return Sta::sta()->arrivalCount();
}

int
required_count()
{
  return Sta::sta()->requiredCount();
}

int
graph_arrival_count()
{
  return Sta::sta()->graph()->arrivalCount();
}

int
graph_required_count()
{
  return Sta::sta()->graph()->requiredCount();
}

void
delete_all_memory()
{
  deleteAllMemory();
}

Tcl_Interp *
tcl_interp()
{
  return Sta::sta()->tclInterp();
}

// Initialize sta after delete_all_memory.
void
init_sta()
{
  initSta();
}

void
clear_sta()
{
  Sta::sta()->clear();
}

void
make_sta(Tcl_Interp *interp)
{
  Sta *sta = new Sta;
  Sta::setSta(sta);
  sta->makeComponents();
  sta->setTclInterp(interp);
}

void
clear_network()
{
  Sta *sta = Sta::sta();
  sta->network()->clear();
}

// Elapsed run time (in seconds).
double
elapsed_run_time()
{
  return elapsedRunTime();
}

// User run time (in seconds).
double
user_run_time()
{
  return userRunTime();
}

// User run time (in seconds).
unsigned long
cputime()
{
  return static_cast<unsigned long>(userRunTime() + .5);
}

// Peak memory usage in bytes.
unsigned long
memory_usage()
{
  return memoryUsage();
}

int
processor_count()
{
  return processorCount();
}

int
thread_count()
{
  return Sta::sta()->threadCount();
}

void
set_thread_count(int count)
{
  Sta::sta()->setThreadCount(count);
}

void
arrivals_invalid()
{
  Sta *sta = Sta::sta();
  sta->arrivalsInvalid();
}

void
delays_invalid()
{
  Sta *sta = Sta::sta();
  sta->delaysInvalid();
}

const char *
pin_location(const Pin *pin)
{
  Network *network = cmdNetwork();
  double x, y;
  bool exists;
  network->location(pin, x, y, exists);
  // return x/y as tcl list
  if (exists)
    return sta::stringPrintTmp("%f %f", x, y);
  else
    return "";
}

const char *
port_location(const Port *port)
{
  Network *network = cmdNetwork();
  const Pin *pin = network->findPin(network->topInstance(), port);
  return pin_location(pin);
}

int
endpoint_violation_count(const MinMax *min_max)
{
  return  Sta::sta()->endpointViolationCount(min_max);
}

void
set_voltage_global(const MinMax *min_max,
                   float voltage)
{
  Sta::sta()->setVoltage(min_max, voltage);
}

void
set_voltage_net(const Net *net,
                const MinMax *min_max,
                float voltage)
{
  Sta::sta()->setVoltage(net, min_max, voltage);
}

%} // inline

////////////////////////////////////////////////////////////////
//
// Object Methods
//
////////////////////////////////////////////////////////////////

%extend Library {
const char *name()
{
  return cmdNetwork()->name(self);
}

Cell *
find_cell(const char *name)
{
  return cmdNetwork()->findCell(self, name);
}

CellSeq
find_cells_matching(const char *pattern,
		    bool regexp,
		    bool nocase)
{
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  CellSeq matches = cmdNetwork()->findCellsMatching(self, &matcher);
  return matches;
}

} // Library methods

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

%extend LibraryIterator {
bool has_next() { return self->hasNext(); }
Library *next() { return self->next(); }
void finish() { delete self; }
} // LibraryIterator methods

%extend LibertyLibraryIterator {
bool has_next() { return self->hasNext(); }
LibertyLibrary *next() { return self->next(); }
void finish() { delete self; }
} // LibertyLibraryIterator methods

%extend Cell {
const char *name() { return cmdNetwork()->name(self); }
Library *library() { return cmdNetwork()->library(self); }
LibertyCell *liberty_cell() { return cmdNetwork()->libertyCell(self); }
bool is_leaf() { return cmdNetwork()->isLeaf(self); }
CellPortIterator *
port_iterator() { return cmdNetwork()->portIterator(self); }
string get_attribute(const char *key) { return cmdNetwork()->getAttribute(self, key); }

Port *
find_port(const char *name)
{
  return cmdNetwork()->findPort(self, name);
}

PortSeq
find_ports_matching(const char *pattern,
		    bool regexp,
		    bool nocase)
{
  PatternMatch matcher(pattern, regexp, nocase, Sta::sta()->tclInterp());
  return cmdNetwork()->findPortsMatching(self, &matcher);
}

} // Cell methods

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

} // LibertyCell methods

%extend CellPortIterator {
bool has_next() { return self->hasNext(); }
Port *next() { return self->next(); }
void finish() { delete self; }
} // CellPortIterator methods

%extend LibertyCellPortIterator {
bool has_next() { return self->hasNext(); }
LibertyPort *next() { return self->next(); }
void finish() { delete self; }
} // LibertyCellPortIterator methods

%extend Port {
const char *bus_name() { return cmdNetwork()->busName(self); }
Cell *cell() { return cmdNetwork()->cell(self); }
LibertyPort *liberty_port() { return cmdNetwork()->libertyPort(self); }
bool is_bus() { return cmdNetwork()->isBus(self); }
PortMemberIterator *
member_iterator() { return cmdNetwork()->memberIterator(self); }

} // Port methods

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

%extend OperatingConditions {
float process() { return self->process(); }
float voltage() { return self->voltage(); }
float temperature() { return self->temperature(); }
}

%extend PortMemberIterator {
bool has_next() { return self->hasNext(); }
Port *next() { return self->next(); }
void finish() { delete self; }
} // PortMemberIterator methods

%extend LibertyPortMemberIterator {
bool has_next() { return self->hasNext(); }
LibertyPort *next() { return self->next(); }
void finish() { delete self; }
} // LibertyPortMemberIterator methods

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

Table1
voltage_waveform(float in_slew,
                 float load_cap)
{
  GateTableModel *gate_model = dynamic_cast<GateTableModel*>(self->model());
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
current_waveform(float in_slew,
                 float load_cap)
{
  GateTableModel *gate_model = dynamic_cast<GateTableModel*>(self->model());
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms) {
      const Table1 *waveform = waveforms->currentWaveform(in_slew, load_cap);
      return waveform;
    }
  }
  return nullptr;
}

float
voltage_current(float in_slew,
                float load_cap,
                float voltage)
{
  GateTableModel *gate_model = dynamic_cast<GateTableModel*>(self->model());
  if (gate_model) {
    OutputWaveforms *waveforms = gate_model->outputWaveforms();
    if (waveforms) {
      float current = waveforms->voltageCurrent(in_slew, load_cap, voltage);
      return current;
    }
  }
  return 0.0;
}

} // TimingArc methods

%extend Instance {
Instance *parent() { return cmdLinkedNetwork()->parent(self); }
Cell *cell() { return cmdLinkedNetwork()->cell(self); }
LibertyCell *liberty_cell() { return cmdLinkedNetwork()->libertyCell(self); }
bool is_leaf() { return cmdLinkedNetwork()->isLeaf(self); }
InstanceChildIterator *
child_iterator() { return cmdLinkedNetwork()->childIterator(self); }
InstancePinIterator *
pin_iterator() { return cmdLinkedNetwork()->pinIterator(self); }
InstanceNetIterator *
net_iterator() { return cmdLinkedNetwork()->netIterator(self); }
Pin *
find_pin(const char *name)
{
  return cmdLinkedNetwork()->findPin(self, name);
}
string get_attribute(const char *key) { return cmdNetwork()->getAttribute(self, key); }
} // Instance methods

%extend InstanceChildIterator {
bool has_next() { return self->hasNext(); }
Instance *next() { return self->next(); }
void finish() { delete self; }
} // InstanceChildIterator methods

%extend LeafInstanceIterator {
bool has_next() { return self->hasNext(); }
Instance *next() { return self->next(); }
void finish() { delete self; }
} // LeafInstanceIterator methods

%extend InstancePinIterator {
bool has_next() { return self->hasNext(); }
Pin *next() { return self->next(); }
void finish() { delete self; }
} // InstancePinIterator methods

%extend InstanceNetIterator {
bool has_next() { return self->hasNext(); }
Net *next() { return self->next(); }
void finish() { delete self; }
} // InstanceNetIterator methods

%extend Pin {
const char *port_name() { return cmdLinkedNetwork()->portName(self); }
Instance *instance() { return cmdLinkedNetwork()->instance(self); }
Net *net() { return cmdLinkedNetwork()->net(self); }
Port *port() { return cmdLinkedNetwork()->port(self); }
Term *term() { return cmdLinkedNetwork()->term(self); }
LibertyPort *liberty_port() { return cmdLinkedNetwork()->libertyPort(self); }
bool is_driver() { return cmdLinkedNetwork()->isDriver(self); }
bool is_load() { return cmdLinkedNetwork()->isLoad(self); }
bool is_leaf() { return cmdLinkedNetwork()->isLeaf(self); }
bool is_hierarchical() { return cmdLinkedNetwork()->isHierarchical(self); }
bool is_top_level_port() { return cmdLinkedNetwork()->isTopLevelPort(self); }
PinConnectedPinIterator *connected_pin_iterator()
{ return cmdLinkedNetwork()->connectedPinIterator(self); }

Vertex **
vertices()
{
  Vertex *vertex, *vertex_bidirect_drvr;
  static Vertex *vertices[3];

  cmdGraph()->pinVertices(self, vertex, vertex_bidirect_drvr);
  vertices[0] = vertex;
  vertices[1] = vertex_bidirect_drvr;
  vertices[2] = nullptr;
  return vertices;
}

} // Pin methods

%extend PinConnectedPinIterator {
bool has_next() { return self->hasNext(); }
const Pin *next() { return self->next(); }
void finish() { delete self; }
} // PinConnectedPinIterator methods

%extend Term {
Net *net() { return cmdLinkedNetwork()->net(self); }
Pin *pin() { return cmdLinkedNetwork()->pin(self); }
} // Term methods

%extend Net {
Instance *instance() { return cmdLinkedNetwork()->instance(self); }
const Net *highest_connected_net()
{ return cmdLinkedNetwork()->highestConnectedNet(self); }
NetPinIterator *pin_iterator() { return cmdLinkedNetwork()->pinIterator(self);}
NetTermIterator *term_iterator() {return cmdLinkedNetwork()->termIterator(self);}
NetConnectedPinIterator *connected_pin_iterator()
{ return cmdLinkedNetwork()->connectedPinIterator(self); }
bool is_power() { return cmdLinkedNetwork()->isPower(self);}
bool is_ground() { return cmdLinkedNetwork()->isGround(self);}

float
capacitance(Corner *corner,
	    const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  Sta::sta()->connectedCap(self, corner, min_max, pin_cap, wire_cap);
  return pin_cap + wire_cap;
}

float
pin_capacitance(Corner *corner,
		const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  Sta::sta()->connectedCap(self, corner, min_max, pin_cap, wire_cap);
  return pin_cap;
}

float
wire_capacitance(Corner *corner,
		 const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  Sta::sta()->connectedCap(self, corner, min_max, pin_cap, wire_cap);
  return wire_cap;
}

// get_ports -of_objects net
PortSeq
ports()
{
  Network *network = cmdLinkedNetwork();
  PortSeq ports;
  if (network->isTopInstance(network->instance(self))) {
    NetTermIterator *term_iter = network->termIterator(self);
    while (term_iter->hasNext()) {
      Term *term = term_iter->next();
      Port *port = network->port(network->pin(term));
      ports.push_back(port);
    }
    delete term_iter;
  }
  return ports;
}

} // Net methods

%extend NetPinIterator {
bool has_next() { return self->hasNext(); }
const Pin *next() { return self->next(); }
void finish() { delete self; }
} // NetPinIterator methods

%extend NetTermIterator {
bool has_next() { return self->hasNext(); }
const Term *next() { return self->next(); }
void finish() { delete self; }
} // NetTermIterator methods

%extend NetConnectedPinIterator {
bool has_next() { return self->hasNext(); }
const Pin *next() { return self->next(); }
void finish() { delete self; }
} // NetConnectedPinIterator methods

%extend Clock {
float period() { return self->period(); }
FloatSeq *waveform() { return self->waveform(); }
float time(RiseFall *rf) { return self->edge(rf)->time(); }
bool is_generated() { return self->isGenerated(); }
bool waveform_valid() { return self->waveformValid(); }
bool is_virtual() { return self->isVirtual(); }
bool is_propagated() { return self->isPropagated(); }
const PinSet &sources() { return self->pins(); }

float
slew(const RiseFall *rf,
     const MinMax *min_max)
{
  return self->slew(rf, min_max);
}

}

%extend ClockEdge {
Clock *clock() { return self->clock(); }
RiseFall *transition() { return self->transition(); }
float time() { return self->time(); }
}

%extend Vertex {
Pin *pin() { return self->pin(); }
bool is_bidirect_driver() { return self->isBidirectDriver(); }
int level() { return Sta::sta()->vertexLevel(self); }
int tag_group_index() { return self->tagGroupIndex(); }

Slew
slew(const RiseFall *rf,
     const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  return sta->vertexSlew(self, rf, min_max);
}

Slew
slew_corner(const RiseFall *rf,
            const Corner *corner,
            const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  return sta->vertexSlew(self, rf, corner, min_max);
}

VertexOutEdgeIterator *
out_edge_iterator()
{
  return new VertexOutEdgeIterator(self, Sta::sta()->graph());
}

VertexInEdgeIterator *
in_edge_iterator()
{
  return new VertexInEdgeIterator(self, Sta::sta()->graph());
}

FloatSeq
arrivals_clk(const RiseFall *rf,
	     Clock *clk,
	     const RiseFall *clk_rf)
{
  Sta *sta = Sta::sta();
  FloatSeq arrivals;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    arrivals.push_back(delayAsFloat(sta->vertexArrival(self, rf, clk_edge,
                                                       path_ap, nullptr)));
  }
  return arrivals;
}

StringSeq
arrivals_clk_delays(const RiseFall *rf,
		    Clock *clk,
		    const RiseFall *clk_rf,
		    int digits)
{
  Sta *sta = Sta::sta();
  StringSeq arrivals;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    arrivals.push_back(delayAsString(sta->vertexArrival(self, rf, clk_edge,
                                                        path_ap, nullptr),
                                     sta, digits));
  }
  return arrivals;
}

FloatSeq
requireds_clk(const RiseFall *rf,
	      Clock *clk,
	      const RiseFall *clk_rf)
{
  Sta *sta = Sta::sta();
  FloatSeq reqs;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    reqs.push_back(delayAsFloat(sta->vertexRequired(self, rf, clk_edge,
                                                    path_ap)));
  }
  return reqs;
}

StringSeq
requireds_clk_delays(const RiseFall *rf,
		     Clock *clk,
		     const RiseFall *clk_rf,
		     int digits)
{
  Sta *sta = Sta::sta();
  StringSeq reqs;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    reqs.push_back(delayAsString(sta->vertexRequired(self, rf, clk_edge, path_ap),
                                 sta, digits));
  }
  return reqs;
}

Slack
slack(MinMax *min_max)
{
  Sta *sta = Sta::sta();
  return sta->vertexSlack(self, min_max);
}

FloatSeq
slacks(RiseFall *rf)
{
  Sta *sta = Sta::sta();
  FloatSeq slacks;
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    slacks.push_back(delayAsFloat(sta->vertexSlack(self, rf, path_ap)));
  }
  return slacks;
}

// Slack with respect to a clock rise/fall edge.
FloatSeq
slacks_clk(const RiseFall *rf,
	   Clock *clk,
	   const RiseFall *clk_rf)
{
  Sta *sta = Sta::sta();
  FloatSeq slacks;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    slacks.push_back(delayAsFloat(sta->vertexSlack(self, rf, clk_edge,
                                                   path_ap)));
  }
  return slacks;
}

StringSeq
slacks_clk_delays(const RiseFall *rf,
		  Clock *clk,
		  const RiseFall *clk_rf,
		  int digits)
{
  Sta *sta = Sta::sta();
  StringSeq slacks;
  const ClockEdge *clk_edge = nullptr;
  if (clk)
    clk_edge = clk->edge(clk_rf);
  for (auto path_ap : sta->corners()->pathAnalysisPts()) {
    slacks.push_back(delayAsString(sta->vertexSlack(self, rf, clk_edge,
                                                    path_ap),
                                   sta, digits));
  }
  return slacks;
}

VertexPathIterator *
path_iterator(const RiseFall *rf,
	      const MinMax *min_max)
{
  return Sta::sta()->vertexPathIterator(self, rf, min_max);
}

bool
has_downstream_clk_pin()
{
  return self->hasDownstreamClkPin();
}

bool
is_clock()
{
  Sta *sta = Sta::sta();
  Search *search = sta->search();
  return search->isClock(self);
}

bool is_disabled_constraint() { return self->isDisabledConstraint(); }

} // Vertex methods

%extend Edge {
Vertex *from() { return self->from(Sta::sta()->graph()); }
Vertex *to() { return self->to(Sta::sta()->graph()); }
Pin *from_pin() { return self->from(Sta::sta()->graph())->pin(); }
Pin *to_pin() { return self->to(Sta::sta()->graph())->pin(); }
TimingRole *role() { return self->role(); }
const char *sense() { return timingSenseString(self->sense()); }
TimingArcSeq &
timing_arcs() { return self->timingArcSet()->arcs(); }
bool is_disabled_loop() { return Sta::sta()->isDisabledLoop(self); }
bool is_disabled_constraint() { return Sta::sta()->isDisabledConstraint(self);}
bool is_disabled_constant() { return Sta::sta()->isDisabledConstant(self); }
bool is_disabled_cond_default()
{ return Sta::sta()->isDisabledCondDefault(self); }
PinSet
disabled_constant_pins() { return Sta::sta()->disabledConstantPins(self); }
bool is_disabled_bidirect_inst_path()
{ return Sta::sta()->isDisabledBidirectInstPath(self); }
bool is_disabled_bidirect_net_path()
{ return Sta::sta()->isDisabledBidirectNetPath(self); }
bool is_disabled_preset_clear()
{ return Sta::sta()->isDisabledPresetClr(self); }
const char *
sim_timing_sense(){return timingSenseString(Sta::sta()->simTimingSense(self));}

FloatSeq
arc_delays(TimingArc *arc)
{
  Sta *sta = Sta::sta();
  FloatSeq delays;
  for (auto dcalc_ap : sta->corners()->dcalcAnalysisPts())
    delays.push_back(delayAsFloat(sta->arcDelay(self, arc, dcalc_ap)));
  return delays;
}

StringSeq
arc_delay_strings(TimingArc *arc,
		  int digits)
{
  Sta *sta = Sta::sta();
  StringSeq delays;
  for (auto dcalc_ap : sta->corners()->dcalcAnalysisPts())
    delays.push_back(delayAsString(sta->arcDelay(self, arc, dcalc_ap),
                                   sta, digits));
  return delays;
}

bool
delay_annotated(TimingArc *arc,
		const Corner *corner,
		const MinMax *min_max)
{
  DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  return Sta::sta()->arcDelayAnnotated(self, arc, dcalc_ap);
}

float
arc_delay(TimingArc *arc,
	  const Corner *corner,
	  const MinMax *min_max)
{
  DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  return delayAsFloat(Sta::sta()->arcDelay(self, arc, dcalc_ap));
}

const char *
cond()
{
  FuncExpr *cond = self->timingArcSet()->cond();
  if (cond)
    return cond->asString();
  else
    return nullptr;
}

const char *
mode_name()
{
  return self->timingArcSet()->modeName();
}

const char *
mode_value()
{
  return self->timingArcSet()->modeValue();
}

const char *
latch_d_to_q_en()
{
  if (self->role() == TimingRole::latchDtoQ()) {
    Sta *sta = Sta::sta();
    const Network *network = sta->cmdNetwork();
    const Graph *graph = sta->graph();
    Pin *from_pin = self->from(graph)->pin();
    Instance *inst = network->instance(from_pin);
    LibertyCell *lib_cell = network->libertyCell(inst);
    TimingArcSet *d_q_set = self->timingArcSet();
    LibertyPort *enable_port;
    FuncExpr *enable_func;
    const RiseFall *enable_rf;
    lib_cell->latchEnable(d_q_set, enable_port, enable_func, enable_rf);
    if (enable_port)
      return stringPrintTmp("%s %s", enable_port->name(), enable_rf->asString());
  }
  return "";
}

} // Edge methods

%extend VertexIterator {
bool has_next() { return self->hasNext(); }
Vertex *next() { return self->next(); }
void finish() { delete self; }
}

%extend VertexInEdgeIterator {
bool has_next() { return self->hasNext(); }
Edge *next() { return self->next(); }
void finish() { delete self; }
}

%extend VertexOutEdgeIterator {
bool has_next() { return self->hasNext(); }
Edge *next() { return self->next(); }
void finish() { delete self; }
}

%extend PathEnd {
bool is_unconstrained() { return self->isUnconstrained(); }
bool is_check() { return self->isCheck(); }
bool is_latch_check() { return self->isLatchCheck(); }
bool is_data_check() { return self->isDataCheck(); }
bool is_output_delay() { return self->isOutputDelay(); }
bool is_path_delay() { return self->isPathDelay(); }
bool is_gated_clock() { return self->isGatedClock(); }
Vertex *vertex() { return self->vertex(Sta::sta()); }
PathRef *path() { return &self->pathRef(); }
RiseFall *end_transition()
{ return const_cast<RiseFall*>(self->path()->transition(Sta::sta())); }
Slack slack() { return self->slack(Sta::sta()); }
ArcDelay margin() { return self->margin(Sta::sta()); }
Required data_required_time() { return self->requiredTimeOffset(Sta::sta()); }
Arrival data_arrival_time() { return self->dataArrivalTimeOffset(Sta::sta()); }
TimingRole *check_role() { return self->checkRole(Sta::sta()); }
MinMax *min_max() { return const_cast<MinMax*>(self->minMax(Sta::sta())); }
float source_clk_offset() { return self->sourceClkOffset(Sta::sta()); }
Arrival source_clk_latency() { return self->sourceClkLatency(Sta::sta()); }
Arrival source_clk_insertion_delay()
{ return self->sourceClkInsertionDelay(Sta::sta()); }
const Clock *target_clk() { return self->targetClk(Sta::sta()); }
const ClockEdge *target_clk_edge() { return self->targetClkEdge(Sta::sta()); }
Path *target_clk_path() { return self->targetClkPath(); }
float target_clk_time() { return self->targetClkTime(Sta::sta()); }
float target_clk_offset() { return self->targetClkOffset(Sta::sta()); }
float target_clk_mcp_adjustment()
{ return self->targetClkMcpAdjustment(Sta::sta()); }
Arrival target_clk_delay() { return self->targetClkDelay(Sta::sta()); }
Arrival target_clk_insertion_delay()
{ return self->targetClkInsertionDelay(Sta::sta()); }
float target_clk_uncertainty()
{ return self->targetNonInterClkUncertainty(Sta::sta()); }
float inter_clk_uncertainty()
{ return self->interClkUncertainty(Sta::sta()); }
Arrival target_clk_arrival() { return self->targetClkArrival(Sta::sta()); }
bool path_delay_margin_is_external()
{ return self->pathDelayMarginIsExternal();}
Crpr common_clk_pessimism() { return self->commonClkPessimism(Sta::sta()); }
RiseFall *target_clk_end_trans()
{ return const_cast<RiseFall*>(self->targetClkEndTrans(Sta::sta())); }
Delay clk_skew() { return self->clkSkew(Sta::sta()); }

}

%extend MinPulseWidthCheckSeqIterator {
bool has_next() { return self->hasNext(); }
MinPulseWidthCheck *next() { return self->next(); }
void finish() { delete self; }
} // MinPulseWidthCheckSeqIterator methods

%extend PathRef {
float
arrival()
{
  Sta *sta = Sta::sta();
  return delayAsFloat(self->arrival(sta));
}

float
required()
{
  Sta *sta = Sta::sta();
  return delayAsFloat(self->required(sta));
}

float
slack()
{
  Sta *sta = Sta::sta();
  return delayAsFloat(self->slack(sta));
}

const Pin *
pin()
{
  Sta *sta = Sta::sta();
  return self->pin(sta);
}

const char *
tag()
{
  Sta *sta = Sta::sta();
  return self->tag(sta)->asString(sta);
}

// mea_opt3
PinSeq
pins()
{
  Sta *sta = Sta::sta();
  PinSeq pins;
  PathRef path1(self);
  while (!path1.isNull()) {
    pins.push_back(path1.vertex(sta)->pin());
    PathRef prev_path;
    path1.prevPath(sta, prev_path);
    path1.init(prev_path);
  }
  return pins;
}

}

%extend VertexPathIterator {
bool has_next() { return self->hasNext(); }
PathRef *
next()
{
  Path *path = self->next();
  return new PathRef(path);
}

void finish() { delete self; }
}

%extend Corner {
const char *name() { return self->name(); }
}

// Local Variables:
// mode:c++
// End:
