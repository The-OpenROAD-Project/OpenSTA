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
#include "DisallowCopyAssign.hh"
#include "TimingRole.hh"
#include "FuncExpr.hh"
#include "TimingArc.hh"
#include "Sequential.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "SearchPred.hh"
#include "Search.hh"
#include "Clock.hh"
#include "FindRegister.hh"

namespace sta {

static TimingSense
pathSenseThru(TimingSense from_sense,
	      TimingSense thru_sense);
static bool
hasMinPulseWidthCheck(LibertyPort *port);

// Predicate used for searching from clocks to find registers.
class FindRegClkPred : public SearchPred1
{
public:
  FindRegClkPred(Clock *clk,
		 const StaState *sta);
  virtual bool searchThru(Edge *edge);
  virtual bool searchFrom(const Vertex *from_vertex);

private:
  DISALLOW_COPY_AND_ASSIGN(FindRegClkPred);

  Clock *clk_;
};

FindRegClkPred::FindRegClkPred(Clock *clk,
			       const StaState *sta) :
  SearchPred1(sta),
  clk_(clk)
{
}

bool
FindRegClkPred::searchFrom(const Vertex *from_vertex)
{
  const Sdc *sdc = sta_->sdc();
  const Pin *from_pin = from_vertex->pin();
  return !sdc->clkStopPropagation(from_pin, clk_)
    && SearchPred1::searchFrom(from_vertex);
}

bool
FindRegClkPred::searchThru(Edge *edge)
{
  TimingRole *role = edge->role();
  return (role->isWire()
	  || role == TimingRole::combinational())
    && SearchPred1::searchThru(edge);
}

// Helper for "all_registers".
// Visit all register instances.
class FindRegVisitor : public StaState
{
public:
  FindRegVisitor(StaState *sta);
  virtual ~FindRegVisitor() {}
  void visitRegs(ClockSet *clks,
		 const RiseFallBoth *clk_rf,
		 bool edge_triggered,
		 bool latches);

private:
  DISALLOW_COPY_AND_ASSIGN(FindRegVisitor);
  void visitRegs(const Pin *clk_pin,
		 TimingSense clk_sense,
		 const RiseFallBoth *clk_rf,
		 bool edge_triggered,
		 bool latches);
  virtual void visitReg(Instance *inst) = 0;
  virtual void visitSequential(Instance *inst,
			       Sequential *seq) = 0;
  void visitFanoutRegs(Vertex *from_vertex,
		       TimingSense from_sense,
		       const RiseFallBoth *clk_rf,
		       bool edge_triggered,
		       bool latches,
		       SearchPred &clk_pred,
		       VertexSet &visited_vertices);
  void findSequential(const Pin *clk_pin,
		      Instance *inst,
		      LibertyCell *cell,
		      TimingSense clk_sense,
		      const RiseFallBoth *clk_rf,
		      bool edge_triggered,
		      bool latches,
		      bool &has_seqs,
		      bool &matches);
  bool findInferedSequential(LibertyCell *cell,
			     TimingSense clk_sense,
			     const RiseFallBoth *clk_rf,
			     bool edge_triggered,
			     bool latches);
  bool hasTimingCheck(LibertyCell *cell,
		      LibertyPort *clk,
		      LibertyPort *d);

};

FindRegVisitor::FindRegVisitor(StaState *sta) :
  StaState(sta)
{
}

void
FindRegVisitor::visitRegs(ClockSet *clks,
			  const RiseFallBoth *clk_rf,
			  bool edge_triggered,
			  bool latches)
{
  if (clks && !clks->empty()) {
    // Use DFS search to find all registers downstream of the clocks.
    ClockSet::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
      FindRegClkPred clk_pred(clk, this);
      VertexSet visited_vertices;
      for (Pin *pin : clk->leafPins()) {
	Vertex *vertex, *bidirect_drvr_vertex;
	graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
	visitFanoutRegs(vertex, TimingSense::positive_unate,
			clk_rf, edge_triggered,
			latches, clk_pred,
			visited_vertices);
	// Clocks defined on bidirect pins blow it out both ends.
	if (bidirect_drvr_vertex)
	  visitFanoutRegs(bidirect_drvr_vertex,
			  TimingSense::positive_unate,
			  clk_rf, edge_triggered,
			  latches, clk_pred,
			  visited_vertices);
      }
    }
  }
  else {
    VertexSet::ConstIterator reg_clk_iter(graph_->regClkVertices());
    while (reg_clk_iter.hasNext()) {
      Vertex *vertex = reg_clk_iter.next();
      visitRegs(vertex->pin(), TimingSense::positive_unate,
		RiseFallBoth::riseFall(),
		edge_triggered, latches);
    }
  }
}

void
FindRegVisitor::visitFanoutRegs(Vertex *from_vertex,
				TimingSense from_sense,
				const RiseFallBoth *clk_rf,
				bool edge_triggered,
				bool latches,
				SearchPred &clk_pred,
				VertexSet &visited_vertices)
{
  if (!visited_vertices.hasKey(from_vertex)
      && clk_pred.searchFrom(from_vertex)) {
    visited_vertices.insert(from_vertex);
    VertexOutEdgeIterator edge_iter(from_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      const Pin *to_pin = to_vertex->pin();
      TimingSense to_sense = pathSenseThru(from_sense, edge->sense());
      if (to_vertex->isRegClk())
	visitRegs(to_pin, to_sense, clk_rf, edge_triggered, latches);
      // Even register clock pins can have combinational fanout arcs.
      if (clk_pred.searchThru(edge)
          && clk_pred.searchTo(to_vertex))
	visitFanoutRegs(to_vertex, to_sense, clk_rf, edge_triggered, latches,
			clk_pred, visited_vertices);
    }
  }
}

void
FindRegVisitor::visitRegs(const Pin *clk_pin,
			  TimingSense clk_sense,
			  const RiseFallBoth *clk_rf,
			  bool edge_triggered,
			  bool latches)
{
  Instance *inst = network_->instance(clk_pin);
  LibertyCell *cell = network_->libertyCell(inst);
  if (!edge_triggered || !latches
      || clk_rf != RiseFallBoth::riseFall()) {
    bool matches, has_seqs;
    findSequential(clk_pin, inst, cell, clk_sense, clk_rf,
		   edge_triggered, latches,
		   has_seqs, matches);
    if (!has_seqs)
      matches = findInferedSequential(cell, clk_sense, clk_rf,
				      edge_triggered, latches);
    if (matches)
      visitReg(inst);
  }
  else
    // Do not require sequentials to match if the search is
    // not specific to edge triggered, latches, or clock edge.
    visitReg(inst);
}

void
FindRegVisitor::findSequential(const Pin *clk_pin,
			       Instance *inst,
			       LibertyCell *cell,
			       TimingSense clk_sense,
			       const RiseFallBoth *clk_rf,
			       bool edge_triggered,
			       bool latches,
			       bool &has_seqs,
			       bool &matches)
{
  has_seqs = false;
  matches = false;
  LibertyCellSequentialIterator seq_iter(cell);
  while (seq_iter.hasNext()) {
    has_seqs = true;
    Sequential *seq = seq_iter.next();
    if ((seq->isRegister() && edge_triggered)
	|| (seq->isLatch() && latches)) {
      if (clk_rf == RiseFallBoth::riseFall()) {
	visitSequential(inst, seq);
	matches = true;
	break;
      }
      else {
	FuncExpr *clk_func = seq->clock();
	LibertyPort *port = network_->libertyPort(clk_pin);
	TimingSense port_sense = clk_func->portTimingSense(port);
	TimingSense path_sense = pathSenseThru(clk_sense, port_sense);
	if ((path_sense == TimingSense::positive_unate
	     && clk_rf == RiseFallBoth::rise())
	    || (path_sense == TimingSense::negative_unate
		&& clk_rf == RiseFallBoth::fall())) {
	  visitSequential(inst, seq);
	  matches = true;
	  break;
	}
      }
    }
  }
}

bool
FindRegVisitor::findInferedSequential(LibertyCell *cell,
				      TimingSense clk_sense,
				      const RiseFallBoth *clk_rf,
				      bool edge_triggered,
				      bool latches)
{
  bool matches = false;
  const RiseFall *clk_rf1 = clk_rf->asRiseFall();
  LibertyCellTimingArcSetIterator set_iter(cell);
  while (set_iter.hasNext()) {
    TimingArcSet *set = set_iter.next();
    TimingArcSetArcIterator arc_iter(set);
    TimingArc *arc = arc_iter.next();
    RiseFall *arc_clk_rf = arc->fromTrans()->asRiseFall();
    bool tr_matches = (clk_rf == RiseFallBoth::riseFall()
		       || (arc_clk_rf == clk_rf1
			   && clk_sense == TimingSense::positive_unate)
		       || (arc_clk_rf == clk_rf1->opposite()
			   && clk_sense == TimingSense::negative_unate));
    TimingRole *role = set->role();
    if (tr_matches
	&& ((role == TimingRole::regClkToQ()
	     && edge_triggered)
	    || (role == TimingRole::latchEnToQ()
		&& latches))) {
      matches = true;
      break;
    }
  }
  return matches;
}

bool
FindRegVisitor::hasTimingCheck(LibertyCell *cell,
			       LibertyPort *clk,
			       LibertyPort *d)
{
  LibertyCellTimingArcSetIterator set_iter(cell, clk, d);
  while (set_iter.hasNext()) {
    TimingArcSet *set = set_iter.next();
    TimingRole *role = set->role();
    if (role->isTimingCheck())
      return true;
  }
  return false;
}

class FindRegInstances : public FindRegVisitor
{
public:
  explicit FindRegInstances(StaState *sta);
  InstanceSet *findRegs(ClockSet *clks,
			const RiseFallBoth *clk_rf,
			bool edge_triggered,
			bool latches);

private:
  DISALLOW_COPY_AND_ASSIGN(FindRegInstances);
  virtual void visitReg(Instance *inst);
  virtual void visitSequential(Instance *inst,
			       Sequential *seq);

  InstanceSet *regs_;
};

FindRegInstances::FindRegInstances(StaState *sta) :
  FindRegVisitor(sta),
  regs_(nullptr)
{
}

InstanceSet *
FindRegInstances::findRegs(ClockSet *clks,
			   const RiseFallBoth *clk_rf,
			   bool edge_triggered,
			   bool latches)
{
  regs_ = new InstanceSet;
  visitRegs(clks, clk_rf, edge_triggered, latches);
  return regs_;
}

void
FindRegInstances::visitSequential(Instance *,
				  Sequential *)
{
}

void
FindRegInstances::visitReg(Instance *inst)
{
  regs_->insert(inst);
}

InstanceSet *
findRegInstances(ClockSet *clks,
		 const RiseFallBoth *clk_rf,
		 bool edge_triggered,
		 bool latches,
		 StaState *sta)
{
  FindRegInstances find_regs(sta);
  return find_regs.findRegs(clks, clk_rf, edge_triggered, latches);
}

////////////////////////////////////////////////////////////////

class FindRegPins : public FindRegVisitor
{
public:
  explicit FindRegPins(StaState *sta);
  PinSet *findPins(ClockSet *clks,
		   const RiseFallBoth *clk_rf,
		   bool edge_triggered,
		   bool latches);

protected:
  DISALLOW_COPY_AND_ASSIGN(FindRegPins);
  virtual void visitReg(Instance *inst);
  virtual void visitSequential(Instance *inst,
			       Sequential *seq);
  virtual bool matchPin(Pin *pin);
  void visitExpr(FuncExpr *expr,
		 Instance *inst,
		 Sequential *seq);
  // Sequential expressions to find instance pins.
  virtual FuncExpr *seqExpr1(Sequential *seq) = 0;
  virtual FuncExpr *seqExpr2(Sequential *seq) = 0;

  PinSet *pins_;
};

FindRegPins::FindRegPins(StaState *sta) :
  FindRegVisitor(sta),
  pins_(nullptr)
{
}

PinSet *
FindRegPins::findPins(ClockSet *clks,
		      const RiseFallBoth *clk_rf,
		      bool edge_triggered,
		      bool latches)
{
  pins_ = new PinSet;
  visitRegs(clks, clk_rf, edge_triggered, latches);
  return pins_;
}

void
FindRegPins::visitSequential(Instance *inst,
			     Sequential *seq)
{
  visitExpr(seqExpr1(seq), inst, seq);
  visitExpr(seqExpr2(seq), inst, seq);
}

void
FindRegPins::visitExpr(FuncExpr *expr,
		       Instance *inst,
		       Sequential *)
{
  if (expr) {
    FuncExprPortIterator port_iter(expr);
    while (port_iter.hasNext()) {
      LibertyPort *port = port_iter.next();
      Pin *pin = network_->findPin(inst, port);
      if (pin)
	pins_->insert(pin);
    }
  }
}

void
FindRegPins::visitReg(Instance *inst)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (matchPin(pin))
      pins_->insert(pin);
  }
  delete pin_iter;
}

bool
FindRegPins::matchPin(Pin *)
{
  return true;
}

class FindRegDataPins : public FindRegPins
{
public:
  explicit FindRegDataPins(StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(FindRegDataPins);
  virtual bool matchPin(Pin *pin);
  virtual FuncExpr *seqExpr1(Sequential *seq);
  virtual FuncExpr *seqExpr2(Sequential *seq);
};

FindRegDataPins::FindRegDataPins(StaState *sta) :
  FindRegPins(sta)
{
}

FuncExpr *
FindRegDataPins::seqExpr1(Sequential *seq)
{
  return seq->data();
}

FuncExpr *
FindRegDataPins::seqExpr2(Sequential *)
{
  return nullptr;
}

bool
FindRegDataPins::matchPin(Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  Vertex *vertex = graph_->pinLoadVertex(pin);
  float ignore;
  bool has_min_period;
  port->minPeriod(ignore, has_min_period);
  return vertex && vertex->hasChecks()
    && !has_min_period
    && !hasMinPulseWidthCheck(port);
}

static bool
hasMinPulseWidthCheck(LibertyPort *port)
{
  float ignore;
  bool exists;
  port->minPulseWidth(RiseFall::rise(), ignore, exists);
  if (exists)
    return true;
  port->minPulseWidth(RiseFall::fall(), ignore, exists);
  return exists;
}

PinSet *
findRegDataPins(ClockSet *clks,
		const RiseFallBoth *clk_rf,
		bool edge_triggered,
		bool latches,
		StaState *sta)
{
  FindRegDataPins find_regs(sta);
  return find_regs.findPins(clks, clk_rf, edge_triggered, latches);
}

////////////////////////////////////////////////////////////////

class FindRegClkPins : public FindRegPins
{
public:
  explicit FindRegClkPins(StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(FindRegClkPins);
  virtual bool matchPin(Pin *pin);
  virtual FuncExpr *seqExpr1(Sequential *seq);
  virtual FuncExpr *seqExpr2(Sequential *seq);
};

FindRegClkPins::FindRegClkPins(StaState *sta) :
  FindRegPins(sta)
{
}

bool
FindRegClkPins::matchPin(Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  LibertyCell *cell = port->libertyCell();
  LibertyCellTimingArcSetIterator set_iter(cell, port, nullptr);
  while (set_iter.hasNext()) {
    TimingArcSet *set = set_iter.next();
    TimingRole *role = set->role();
    if (role->isTimingCheck())
      return true;
  }
  return false;
}


FuncExpr *
FindRegClkPins::seqExpr1(Sequential *seq)
{
  return seq->clock();
}

FuncExpr *
FindRegClkPins::seqExpr2(Sequential *)
{
  return nullptr;
}

PinSet *
findRegClkPins(ClockSet *clks,
	       const RiseFallBoth *clk_rf,
	       bool edge_triggered,
	       bool latches,
	       StaState *sta)
{
  FindRegClkPins find_regs(sta);
  return find_regs.findPins(clks, clk_rf, edge_triggered, latches);
}

////////////////////////////////////////////////////////////////

class FindRegAsyncPins : public FindRegPins
{
public:
  explicit FindRegAsyncPins(StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(FindRegAsyncPins);
  virtual bool matchPin(Pin *pin);
  virtual FuncExpr *seqExpr1(Sequential *seq) { return seq->clear(); }
  virtual FuncExpr *seqExpr2(Sequential *seq) { return seq->preset(); }
};

FindRegAsyncPins::FindRegAsyncPins(StaState *sta) :
  FindRegPins(sta)
{
}

bool
FindRegAsyncPins::matchPin(Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  LibertyCell *cell = port->libertyCell();
  LibertyCellTimingArcSetIterator set_iter(cell, port, nullptr);
  while (set_iter.hasNext()) {
    TimingArcSet *set = set_iter.next();
    TimingRole *role = set->role();
    if (role == TimingRole::regSetClr())
      return true;
  }
  return false;
}

PinSet *
findRegAsyncPins(ClockSet *clks,
		 const RiseFallBoth *clk_rf,
		 bool edge_triggered,
		 bool latches,
		 StaState *sta)
{
  FindRegAsyncPins find_regs(sta);
  return find_regs.findPins(clks, clk_rf, edge_triggered, latches);
}

////////////////////////////////////////////////////////////////

class FindRegOutputPins : public FindRegPins
{
public:
  explicit FindRegOutputPins(StaState *sta);

private:
  DISALLOW_COPY_AND_ASSIGN(FindRegOutputPins);
  virtual bool matchPin(Pin *pin);
  virtual void visitSequential(Instance *inst,
			       Sequential *seq);
  void visitOutput(LibertyPort *port,
		   Instance *inst);
  // Unused.
  virtual FuncExpr *seqExpr1(Sequential *seq);
  virtual FuncExpr *seqExpr2(Sequential *seq);
};

FindRegOutputPins::FindRegOutputPins(StaState *sta) :
  FindRegPins(sta)
{
}

bool
FindRegOutputPins::matchPin(Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  LibertyCell *cell = port->libertyCell();
  LibertyCellTimingArcSetIterator set_iter(cell, nullptr, port);
  while (set_iter.hasNext()) {
    TimingArcSet *set = set_iter.next();
    TimingRole *role = set->role();
    if (role == TimingRole::regClkToQ()
	|| role == TimingRole::latchEnToQ()
	|| role == TimingRole::latchDtoQ())
      return true;
  }
  return false;
}

void
FindRegOutputPins::visitSequential(Instance *inst,
				   Sequential *seq)
{
  visitOutput(seq->output(), inst);
  visitOutput(seq->outputInv(), inst);
}

void
FindRegOutputPins::visitOutput(LibertyPort *port,
			       Instance *inst)
{
  if (port) {
    // Sequential outputs are internal ports.
    // Find the output port that is connected to the internal port
    // by uses it as its function.
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      LibertyPort *pin_port = network_->libertyPort(pin);
      FuncExpr *func = pin_port->function();
      if (func
	  && func->port()
	  && func->port() == port)
	pins_->insert(pin);
    }
    delete pin_iter;
  }
}

FuncExpr *
FindRegOutputPins::seqExpr1(Sequential *)
{
  return nullptr;
}

FuncExpr *
FindRegOutputPins::seqExpr2(Sequential *)
{
  return nullptr;
}

PinSet *
findRegOutputPins(ClockSet *clks,
		  const RiseFallBoth *clk_rf,
		  bool edge_triggered,
		  bool latches,
		  StaState *sta)
{
  FindRegOutputPins find_regs(sta);
  return find_regs.findPins(clks, clk_rf, edge_triggered, latches);
}

////////////////////////////////////////////////////////////////

static TimingSense path_sense_thru[timing_sense_count][timing_sense_count];

static void
initPathSenseThru1(TimingSense from,
		   TimingSense thru,
		   TimingSense to)
{
  path_sense_thru[int(from)][int(thru)] = to;
}

void
initPathSenseThru()
{
  initPathSenseThru1(TimingSense::positive_unate, TimingSense::positive_unate,
		     TimingSense::positive_unate);
  initPathSenseThru1(TimingSense::positive_unate, TimingSense::negative_unate,
		     TimingSense::negative_unate);
  initPathSenseThru1(TimingSense::positive_unate, TimingSense::non_unate,
		     TimingSense::non_unate);
  initPathSenseThru1(TimingSense::positive_unate, TimingSense::none,
		     TimingSense::none);
  initPathSenseThru1(TimingSense::positive_unate, TimingSense::unknown,
		     TimingSense::unknown);
  initPathSenseThru1(TimingSense::negative_unate, TimingSense::positive_unate,
		     TimingSense::negative_unate);
  initPathSenseThru1(TimingSense::negative_unate, TimingSense::negative_unate,
		     TimingSense::positive_unate);
  initPathSenseThru1(TimingSense::negative_unate, TimingSense::non_unate,
		     TimingSense::non_unate);
  initPathSenseThru1(TimingSense::negative_unate, TimingSense::none,
		     TimingSense::none);
  initPathSenseThru1(TimingSense::negative_unate, TimingSense::unknown,
		     TimingSense::unknown);

  initPathSenseThru1(TimingSense::non_unate, TimingSense::positive_unate,
		     TimingSense::non_unate);
  initPathSenseThru1(TimingSense::non_unate, TimingSense::negative_unate,
		     TimingSense::non_unate);
  initPathSenseThru1(TimingSense::non_unate, TimingSense::non_unate,
		     TimingSense::non_unate);
  initPathSenseThru1(TimingSense::non_unate, TimingSense::none,
		     TimingSense::none);
  initPathSenseThru1(TimingSense::non_unate, TimingSense::unknown,
		     TimingSense::unknown);

  initPathSenseThru1(TimingSense::none, TimingSense::positive_unate,
		     TimingSense::none);
  initPathSenseThru1(TimingSense::none, TimingSense::negative_unate,
		     TimingSense::none);
  initPathSenseThru1(TimingSense::none, TimingSense::non_unate,
		     TimingSense::none);
  initPathSenseThru1(TimingSense::none, TimingSense::none,
		     TimingSense::none);
  initPathSenseThru1(TimingSense::none, TimingSense::unknown,
		     TimingSense::unknown);

  initPathSenseThru1(TimingSense::unknown, TimingSense::positive_unate,
		     TimingSense::unknown);
  initPathSenseThru1(TimingSense::unknown, TimingSense::negative_unate,
		     TimingSense::unknown);
  initPathSenseThru1(TimingSense::unknown, TimingSense::non_unate,
		     TimingSense::unknown);
  initPathSenseThru1(TimingSense::unknown, TimingSense::none,
		     TimingSense::unknown);
  initPathSenseThru1(TimingSense::unknown, TimingSense::unknown,
		     TimingSense::unknown);
}

static TimingSense
pathSenseThru(TimingSense from_sense,
	      TimingSense thru_sense)
{
  return path_sense_thru[int(from_sense)][int(thru_sense)];
}

} // namespace
