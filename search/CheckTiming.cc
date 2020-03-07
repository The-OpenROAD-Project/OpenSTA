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

#include "Machine.hh"
#include "Error.hh"
#include "PortDirection.hh"
#include "TimingRole.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "Graph.hh"
#include "PortDelay.hh"
#include "ExceptionPath.hh"
#include "Sdc.hh"
#include "SearchPred.hh"
#include "Levelize.hh"
#include "Bfs.hh"
#include "Search.hh"
#include "Genclks.hh"
#include "PathVertex.hh"
#include "CheckTiming.hh"

namespace sta {

using std::string;

CheckTiming::CheckTiming(StaState *sta) :
  StaState(sta)
{
}

CheckTiming::~CheckTiming()
{
  deleteErrors();
}

void
CheckTiming::deleteErrors()
{
  CheckErrorSeq::Iterator error_iter(errors_);
  while (error_iter.hasNext()) {
    CheckError *error = error_iter.next();
    deleteContents(error);
    delete error;
  }
}

void
CheckTiming::clear()
{
  deleteErrors();
  errors_.clear();
}

CheckErrorSeq &
CheckTiming::check(bool no_input_delay,
		   bool no_output_delay,
		   bool reg_multiple_clks,
		   bool reg_no_clks,
		   bool unconstrained_endpoints,
		   bool loops,
		   bool generated_clks)
{
  clear();
  if (no_input_delay)
    checkNoInputDelay();
  if (no_output_delay)
    checkNoOutputDelay();
  if (reg_multiple_clks || reg_no_clks)
    checkRegClks(reg_multiple_clks, reg_no_clks);
  if (unconstrained_endpoints)
    checkUnconstrainedEndpoints();
  if (loops)
    checkLoops();
  if (generated_clks)
    checkGeneratedClocks();
  return errors_;
}

// Make sure there is a set_input_delay for each input/bidirect.
void
CheckTiming::checkNoInputDelay()
{
  PinSet no_arrival;
  Instance *top_inst = network_->topInstance();
  InstancePinIterator *pin_iter = network_->pinIterator(top_inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (!sdc_->isClock(pin)) {
      PortDirection *dir = network_->direction(pin);
      if (dir->isAnyInput()
	  && !sdc_->hasInputDelay(pin))
	no_arrival.insert(pin);
    }
  }
  delete pin_iter;
  pushPinErrors("Warning: There %is %d input port%s missing set_input_delay.",
		no_arrival);
}

void
CheckTiming::checkNoOutputDelay()
{
  PinSet no_departure;
  checkNoOutputDelay(no_departure);
  pushPinErrors("Warning: There %is %d output port%s missing set_output_delay.",
		no_departure);
}

void
CheckTiming::checkNoOutputDelay(PinSet &no_departure)
{
  Instance *top_inst = network_->topInstance();
  InstancePinIterator *pin_iter = network_->pinIterator(top_inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    PortDirection *dir = network_->direction(pin);
    if (dir->isAnyOutput()
	&& !sdc_->hasOutputDelay(pin))
      no_departure.insert(pin);
  }
  delete pin_iter;
}

bool
CheckTiming::hasClkedCheck(Vertex *vertex)
{
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->role() == TimingRole::setup()
	&& search_->isClock(edge->from(graph_)))
      return true;
  }
  return false;
}

// Search incrementally maintains register/latch clock pins, so use it.
void
CheckTiming::checkRegClks(bool reg_multiple_clks,
			  bool reg_no_clks)
{
  PinSet no_clk_pins, multiple_clk_pins;
  VertexSet::ConstIterator reg_clk_iter(graph_->regClkVertices());
  while (reg_clk_iter.hasNext()) {
    Vertex *vertex = reg_clk_iter.next();
    Pin *pin = vertex->pin();
    ClockSet clks;
    search_->clocks(vertex, clks);
    if (reg_no_clks && clks.empty())
      no_clk_pins.insert(pin);
    if (reg_multiple_clks && clks.size() > 1)
      multiple_clk_pins.insert(pin);
  }
  pushPinErrors("Warning: There %is %d unclocked register/latch pin%s.",
		no_clk_pins);
  pushPinErrors("Warning: There %is %d register/latch pin%s with multiple clocks.",
		multiple_clk_pins);
}

void
CheckTiming::checkLoops()
{
  // These may not need to be sorted because the graph roots are
  // sorted during levelization so the discovery should be consistent.
  GraphLoopSeq *loops = levelize_->loops();
  // Count the combinational loops.
  int loop_count = 0;
  GraphLoopSeq::Iterator loop_iter1(loops);
  while (loop_iter1.hasNext()) {
    GraphLoop *loop = loop_iter1.next();
    if (loop->isCombinational())
      loop_count++;
  }
  if (loop_count > 0) {
   string error_msg;
   errorMsgSubst("Warning: There %is %d combinational loop%s in the design.",
		  loop_count, error_msg);
    CheckError *error = new CheckError;
    error->push_back(stringCopy(error_msg.c_str()));

    GraphLoopSeq::Iterator loop_iter2(loops);
    while (loop_iter2.hasNext()) {
      GraphLoop *loop = loop_iter2.next();
      if (loop->isCombinational()) {
	EdgeSeq::Iterator edge_iter(loop->edges());
	Edge *last_edge = nullptr;
	while (edge_iter.hasNext()) {
	  Edge *edge = edge_iter.next();
	  Pin *pin = edge->from(graph_)->pin();
	  const char *pin_name = stringCopy(sdc_network_->pathName(pin));
	  error->push_back(pin_name);
	  last_edge = edge;
	}
	error->push_back(stringCopy("| loop cut point"));
	const Pin *pin = last_edge->to(graph_)->pin();
	const char *pin_name = stringCopy(sdc_network_->pathName(pin));
	error->push_back(pin_name);

	// Separator between loops.
	error->push_back(stringCopy("--------------------------------"));
      }
    }
    errors_.push_back(error);
  }
}

void
CheckTiming::checkUnconstrainedEndpoints()
{
  PinSet unconstrained_ends;
  checkUnconstraintedOutputs(unconstrained_ends);
  checkUnconstrainedSetups(unconstrained_ends);
  pushPinErrors("Warning: There %is %d unconstrained endpoint%s.",
		unconstrained_ends);
}

void
CheckTiming::checkUnconstraintedOutputs(PinSet &unconstrained_ends)
{
  Instance *top_inst = network_->topInstance();
  InstancePinIterator *pin_iter = network_->pinIterator(top_inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    PortDirection *dir = network_->direction(pin);
    if (dir->isAnyOutput()
	&& !((hasClkedDepature(pin)
	      && hasClkedArrival(graph_->pinLoadVertex(pin)))
	     || hasMaxDelay(pin)))
      unconstrained_ends.insert(pin);
  }
  delete pin_iter;
}

bool
CheckTiming::hasClkedDepature(Pin *pin)
{
  OutputDelaySet *output_delays = sdc_->outputDelaysLeafPin(pin);
  if (output_delays) {
    for (OutputDelay *output_delay : *output_delays) {
      if (output_delay->clkEdge() != nullptr)
	return true;
    }
  }
  return false;
}

// Check for max delay exception that ends at pin.
bool
CheckTiming::hasMaxDelay(Pin *pin)
{
  ExceptionPathSet *exceptions = sdc_->exceptions();
  ExceptionPathSet::Iterator exception_iter(exceptions);
  while (exception_iter.hasNext()) {
    ExceptionPath *exception = exception_iter.next();
    ExceptionTo *to = exception->to();
    if (exception->isPathDelay()
	&& exception->minMax() == MinMaxAll::max()
	&& to
	&& to->hasPins()
	&& to->pins()->hasKey(pin))
      return true;
  }
  return false;
}

void
CheckTiming::checkUnconstrainedSetups(PinSet &unconstrained_ends)
{
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role() == TimingRole::setup()
	  && (!search_->isClock(edge->from(graph_))
	       || !hasClkedArrival(edge->to(graph_)))) {
	unconstrained_ends.insert(vertex->pin());
	break;
      }
    }
  }
}

bool
CheckTiming::hasClkedArrival(Vertex *vertex)
{
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    if (path->clock(this))
      return true;
  }
  return false;
}

void
CheckTiming::checkGeneratedClocks()
{
  ClockSet gen_clk_errors;
  for (auto clk : sdc_->clks()) {
    if (clk->isGenerated()) {
      search_->genclks()->checkMaster(clk);
      bool found_clk = false;
      VertexSet src_vertices;
      clk->srcPinVertices(src_vertices, network_, graph_);
      VertexSet::Iterator vertex_iter(src_vertices);
      while (vertex_iter.hasNext()) {
	Vertex *vertex = vertex_iter.next();
	if (search_->isClock(vertex)) {
	  found_clk = true;
	  break;
	}
      }
      if (!found_clk)
	gen_clk_errors.insert(clk);
    }
  }
  pushClkErrors("Warning: There %is %d generated clock%s that %is not connected to a clock source.",
		gen_clk_errors);
}

// Report the "msg" error for each pin in "pins".
//
// Substitutions in msg are done as follows if the pin count is one
// or greater than one.
// %is - is/are
// %d  - pin count
// %s  - s/""
// %a  - a/""
void
CheckTiming::pushPinErrors(const char *msg,
			   PinSet &pins)
{
  if (!pins.empty()) {
    CheckError *error = new CheckError;
    string error_msg;
    errorMsgSubst(msg, pins.size(), error_msg);
    // Copy the error strings because the error deletes them when it
    // is deleted.
    error->push_back(stringCopy(error_msg.c_str()));
    // Sort the error pins so the output is independent of the order
    // the the errors are discovered.
    PinSeq pin_seq;
    sortPinSet(&pins, network_, pin_seq);
    PinSeq::Iterator pin_iter(pin_seq);
    while (pin_iter.hasNext()) {
      const Pin *pin = pin_iter.next();
      const char *pin_name = stringCopy(sdc_network_->pathName(pin));
      error->push_back(pin_name);
    }
    errors_.push_back(error);
  }
}

void
CheckTiming::pushClkErrors(const char *msg,
			   ClockSet &clks)
{
  if (!clks.empty()) {
    CheckError *error = new CheckError;
    string error_msg;
    errorMsgSubst(msg, clks.size(), error_msg);
    // Copy the error strings because the error deletes them when it
    // is deleted.
    error->push_back(stringCopy(error_msg.c_str()));
    // Sort the error clks so the output is independent of the order
    // the the errors are discovered.
    ClockSeq clk_seq;
    sortClockSet(&clks, clk_seq);
    ClockSeq::Iterator clk_iter(clk_seq);
    while (clk_iter.hasNext()) {
      const Clock *clk = clk_iter.next();
      const char *clk_name = stringCopy(clk->name());
      error->push_back(clk_name);
    }
    errors_.push_back(error);
  }
}

// Copy msg making substitutions for singular/plurals.
void
CheckTiming::errorMsgSubst(const char *msg,
			   int obj_count,
			   string &error_msg)
{
  for (const char *s = msg; *s; s++) {
    char ch = *s;
    if (ch == '%') {
      char flag = s[1];
      if (flag == 'i') {
	if (obj_count > 1)
	  error_msg += "are";
	else
	  error_msg += "is";
	s += 2;
      }
      else if (flag == 'a') {
	if (obj_count == 1) {
	  error_msg += 'a';
	  s++;
	}
	else
	  // Skip space after %a.
	  s += 2;
      }
      else if (flag == 's') {
	if (obj_count > 1)
	  error_msg += 's';
	s++;
      }
      else if (flag == 'd') {
	const char *obj_str = integerString(obj_count);
	error_msg += obj_str;
	stringDelete(obj_str);
	s++;
      }
      else
	internalError("unknown print flag");
    }
    else
      error_msg += ch;
  }
}

} // namespace
