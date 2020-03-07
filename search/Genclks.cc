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
#include "Stats.hh"
#include "Debug.hh"
#include "Report.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "ExceptionPath.hh"
#include "Clock.hh"
#include "StaState.hh"
#include "SearchPred.hh"
#include "Bfs.hh"
#include "TagGroup.hh"
#include "Corner.hh"
#include "PathAnalysisPt.hh"
#include "Levelize.hh"
#include "PathVertexRep.hh"
#include "Search.hh"
#include "Genclks.hh"

namespace sta {

using std::max;

class GenclkInfo
{
public:
  GenclkInfo(Clock *gclk,
	     Level gclk_level,
	     VertexSet *fanins,
	     FilterPath *src_filter);
  ~GenclkInfo();
  EdgeSet *fdbkEdges() const { return fdbk_edges_; }
  VertexSet *fanins() const { return fanins_; }
  Level gclkLevel() const { return gclk_level_; }
  FilterPath *srcFilter() const { return src_filter_; }
  FilterPath *pllFilter() const { return pll_filter_; }
  void setPllFilter(FilterPath *pll_filter);
  void setLatchFdbkEdges(EdgeSet *fdbk_edges);
  bool foundLatchFdbkEdges() const { return found_latch_fdbk_edges_; }
  void setFoundLatchFdbkEdges(bool found);

protected:
  DISALLOW_COPY_AND_ASSIGN(GenclkInfo);

  Clock *gclk_;
  Level gclk_level_;
  VertexSet *fanins_;
  EdgeSet *fdbk_edges_;
  bool found_latch_fdbk_edges_;
  FilterPath *src_filter_;
  FilterPath *pll_filter_;
};

GenclkInfo::GenclkInfo(Clock *gclk,
		       Level gclk_level,
		       VertexSet *fanins,
		       FilterPath *src_filter) :
  gclk_(gclk),
  gclk_level_(gclk_level),
  fanins_(fanins),
  fdbk_edges_(nullptr),
  found_latch_fdbk_edges_(false),
  src_filter_(src_filter),
  pll_filter_(nullptr)
{
}

GenclkInfo::~GenclkInfo()
{
  delete fanins_;
  delete fdbk_edges_;
  delete src_filter_;
  delete pll_filter_;
}

void
GenclkInfo::setPllFilter(FilterPath *pll_filter)
{
  pll_filter_ = pll_filter;
}

void
GenclkInfo::setLatchFdbkEdges(EdgeSet *fdbk_edges)
{
  fdbk_edges_ = fdbk_edges;
}

void
GenclkInfo::setFoundLatchFdbkEdges(bool found)
{
  found_latch_fdbk_edges_ = found;
}

////////////////////////////////////////////////////////////////

Genclks::Genclks(StaState *sta) :
  StaState(sta),
  found_insertion_delays_(false)
{
}

Genclks::~Genclks()
{
  genclk_info_map_.deleteContentsClear();
  clearSrcPaths();
}

void
Genclks::clear()
{
  found_insertion_delays_ = false;
  genclk_info_map_.deleteContentsClear();
  clearSrcPaths();
}

VertexSet *
Genclks::fanins(const Clock *clk)
{
  GenclkInfo *genclk_info = genclkInfo(clk);
  return genclk_info->fanins();
}

Vertex *
Genclks::srcPathVertex(const Pin *pin) const
{
  bool is_bidirect = network_->direction(pin)->isBidirect();
  // Insertion delay is to the driver vertex for clks defined on
  // bidirect pins.
  if (is_bidirect && network_->isLeaf(pin))
    return graph_->pinDrvrVertex(pin);
  else
    // Insertion delay is to the load vertex for clks defined on
    // bidirect ports.
    return graph_->pinLoadVertex(pin);
}

Level
Genclks::clkPinMaxLevel(Clock *clk) const
{
  Level max_level = 0;
  for (Pin *pin : clk->leafPins()) {
    Vertex *vertex = srcPathVertex(pin);
    max_level = max(max_level, vertex->level());
  }
  return max_level;
}

class ClockPinMaxLevelLess
{
public:
  explicit ClockPinMaxLevelLess(const Genclks *genclks);
  bool operator()(Clock *clk1,
		  Clock *clk2) const;

protected:
  const Genclks *genclks_;
};

ClockPinMaxLevelLess::ClockPinMaxLevelLess(const Genclks *genclks) :
  genclks_(genclks)
{
}

bool
ClockPinMaxLevelLess::operator()(Clock *clk1,
				 Clock *clk2) const
{
  return genclks_->clkPinMaxLevel(clk1) < genclks_->clkPinMaxLevel(clk2);
}

// Generated clock source paths.
// The path between the source clock and generated clock is used
// to find the insertion delay (source latency) when the clock is
// propagated and for reporting path type "full_clock_expanded".
void
Genclks::ensureInsertionDelays()
{
  if (!found_insertion_delays_) {
    Stats stats(debug_);
    debugPrint0(debug_, "genclk", 1, "find generated clk insertion delays\n");

    ClockSeq gclks;
    for (auto clk : sdc_->clks()) {
      if (clk->isGenerated()) {
	checkMaster(clk);
	gclks.push_back(clk);
      }
    }

    clearSrcPaths();

    // Generated clocks derived from a generated clock inherit its
    // insertion delay, so sort the clocks by source pin level.
    sort(gclks, ClockPinMaxLevelLess(this));

    ClockSeq::Iterator gclk_iter(gclks);
    while (gclk_iter.hasNext()) {
      Clock *gclk = gclk_iter.next();
      if (gclk->masterClk()) {
	findInsertionDelays(gclk);
	if (gclk->pllOut())
	  findPllDelays(gclk);
	recordSrcPaths(gclk);
      }
    }

    stats.report("Find generated clk insertion delays");
    found_insertion_delays_ = true;
  }
}

// Similar to ClkTreeSearchPred but ignore constants.
class GenClkMasterSearchPred : public SearchPred
{
public:
  explicit GenClkMasterSearchPred(const StaState *sta);
  virtual bool searchFrom(const Vertex *from_vertex);
  virtual bool searchThru(Edge *edge);
  virtual bool searchTo(const Vertex *to_vertex);

protected:
  const StaState *sta_;

private:
  DISALLOW_COPY_AND_ASSIGN(GenClkMasterSearchPred);
};

GenClkMasterSearchPred::GenClkMasterSearchPred(const StaState *sta) :
  SearchPred(),
  sta_(sta)
{
}

bool
GenClkMasterSearchPred::searchFrom(const Vertex *from_vertex)
{
  return !from_vertex->isDisabledConstraint();
}

bool
GenClkMasterSearchPred::searchThru(Edge *edge)
{
  const Sdc *sdc = sta_->sdc();
  TimingRole *role = edge->role();
  // Propagate clocks through constants.
  return !(edge->isDisabledLoop()
	   || edge->isDisabledConstraint()
	   // Constants disable edge cond expression.
	   || edge->isDisabledCond()
	   || sdc->isDisabledCondDefault(edge)
	   // Register/latch preset/clr edges are disabled by default.
	   || (!sdc->presetClrArcsEnabled()
	       && role == TimingRole::regSetClr())
	   || (edge->isBidirectInstPath()
	       && !sdc->bidirectInstPathsEnabled())
	   || (edge->isBidirectNetPath()
	       && !sdc->bidirectNetPathsEnabled()));
}

bool
GenClkMasterSearchPred::searchTo(const Vertex *)
{
  return true;
}

void
Genclks::checkMaster(Clock *gclk)
{
  ensureMaster(gclk);
  if (gclk->masterClk() == nullptr)
    report_->warn("no master clock found for generated clock %s.\n",
		  gclk->name());
}

void
Genclks::ensureMaster(Clock *gclk)
{
  Clock *master_clk = gclk->masterClk();
  if (master_clk == nullptr) {
    int master_clk_count = 0;
    bool found_master = false;
    if (gclk->pllOut()) {
      // Search backward from generated clock source pin to a clock pin.
      GenClkMasterSearchPred pred(this);
      BfsBkwdIterator iter(BfsIndex::other, &pred, this);
      seedSrcPins(gclk, iter);
      while (iter.hasNext()) {
	Vertex *vertex = iter.next();
	Pin *pin = vertex->pin();
	if (sdc_->isLeafPinClock(pin)) {
	  ClockSet *master_clks = sdc_->findLeafPinClocks(pin);
	  if (master_clks) {
	    ClockSet::Iterator master_iter(master_clks);
	    while (master_iter.hasNext()) {
	      master_clk = master_iter.next();
	      // Master source pin can actually be a clock source pin.
	      if (master_clk != gclk) {
		gclk->setInferedMasterClk(master_clk);
		debugPrint2(debug_, "genclk", 2, " %s master clk %s\n",
			    gclk->name(),
			    master_clk->name());
		found_master = true;
		master_clk_count++;
	      }
	    }
	  }
	}
	if (found_master)
	  break;
	iter.enqueueAdjacentVertices(vertex);
      }
      if (master_clk_count > 1)
	report_->error("generated clock %s is in the fanout of multiple clocks.\n",
		       gclk->name());
    }
    else {
      Pin *src_pin = gclk->srcPin();
      ClockSet *master_clks = sdc_->findClocks(src_pin);
      ClockSet::Iterator master_iter(master_clks);
      if (master_iter.hasNext()) {
	while (master_iter.hasNext()) {
	  master_clk = master_iter.next();
	  // Master source pin can actually be a clock source pin.
	  if (master_clk != gclk) {
	    gclk->setInferedMasterClk(master_clk);
	    debugPrint2(debug_, "genclk", 2, " %s master clk %s\n",
			gclk->name(),
			master_clk->name());
	    found_master = true;
	    master_clk_count++;
	  }
	}
      }
      if (!found_master) {
	// Search backward from generated clock source pin to a clock pin.
	GenClkMasterSearchPred pred(this);
	BfsBkwdIterator iter(BfsIndex::other, &pred, this);
	seedSrcPins(gclk, iter);
	while (iter.hasNext()) {
	  Vertex *vertex = iter.next();
	  Pin *pin = vertex->pin();
	  if (sdc_->isLeafPinClock(pin)) {
	    ClockSet *master_clks = sdc_->findLeafPinClocks(pin);
	    if (master_clks) {
	      ClockSet::Iterator master_iter(master_clks);
	      if (master_iter.hasNext()) {
		master_clk = master_iter.next();
		// Master source pin can actually be a clock source pin.
		if (master_clk != gclk) {
		  gclk->setInferedMasterClk(master_clk);
		  debugPrint2(debug_, "genclk", 2, " %s master clk %s\n",
			      gclk->name(),
			      master_clk->name());
		  master_clk_count++;
		  break;
		}
	      }
	    }
	  }
	  iter.enqueueAdjacentVertices(vertex);
	}
      }
      if (master_clk_count > 1)
	report_->error("generated clock %s pin %s is in the fanout of multiple clocks.\n",
		       gclk->name(),
		       network_->pathName(src_pin));
    }
  }
}

void
Genclks::seedSrcPins(Clock *clk,
		     BfsBkwdIterator &iter)
{
  VertexSet src_vertices;
  clk->srcPinVertices(src_vertices, network_, graph_);
  VertexSet::Iterator vertex_iter(src_vertices);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    iter.enqueue(vertex);
  }
}

////////////////////////////////////////////////////////////////

// Similar to ClkTreeSearchPred but
//  search thru constants
//  respect generated clock combinational attribute
//  search thru disabled loop arcs
class GenClkFaninSrchPred : public GenClkMasterSearchPred
{
public:
  explicit GenClkFaninSrchPred(Clock *gclk,
			       const StaState *sta);
  virtual bool searchFrom(const Vertex *from_vertex);
  virtual bool searchThru(Edge *edge);
  virtual bool searchTo(const Vertex *to_vertex);

private:
  DISALLOW_COPY_AND_ASSIGN(GenClkFaninSrchPred);

  bool combinational_;
};

GenClkFaninSrchPred::GenClkFaninSrchPred(Clock *gclk,
					 const StaState *sta) :
  GenClkMasterSearchPred(sta),
  combinational_(gclk->combinational())
{
}

bool
GenClkFaninSrchPred::searchFrom(const Vertex *from_vertex)
{
  return !from_vertex->isDisabledConstraint();
}

bool
GenClkFaninSrchPred::searchThru(Edge *edge)
{
  const TimingRole *role = edge->role();
  return GenClkMasterSearchPred::searchThru(edge)
    && (role == TimingRole::combinational()
	  || role == TimingRole::wire()
	  || !combinational_);
}

bool
GenClkFaninSrchPred::searchTo(const Vertex *)
{
  return true;
}

void
Genclks::findFanin(Clock *gclk,
		   // Return value.
		   VertexSet *fanins)
{
  // Search backward from generated clock source pin to a clock pin.
  GenClkFaninSrchPred srch_pred(gclk, this);
  BfsBkwdIterator iter(BfsIndex::other, &srch_pred, this);
  seedClkVertices(gclk, iter, fanins);
  while (iter.hasNext()) {
    Vertex *vertex = iter.next();
    if (!fanins->hasKey(vertex)) {
      fanins->insert(vertex);
      debugPrint2(debug_, "genclk", 2, "gen clk %s fanin %s\n",
		  gclk->name(), vertex->name(sdc_network_));
      iter.enqueueAdjacentVertices(vertex);
    }
  }
}

void
Genclks::seedClkVertices(Clock *clk,
			 BfsBkwdIterator &iter,
			 VertexSet *fanins)
{
  for (Pin *pin : clk->leafPins()) {
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    fanins->insert(vertex);
    iter.enqueueAdjacentVertices(vertex);
    if (bidirect_drvr_vertex) {
      fanins->insert(bidirect_drvr_vertex);
      iter.enqueueAdjacentVertices(bidirect_drvr_vertex);
    }
  }
}

////////////////////////////////////////////////////////////////

class GenClkInsertionSearchPred : public SearchPred0, public DynLoopSrchPred
{
public:
  GenClkInsertionSearchPred(Clock *gclk,
			    TagGroupBldr *tag_bldr,
			    GenclkInfo *genclk_info,
			    const StaState *sta);
  virtual bool searchThru(Edge *edge);
  virtual bool searchTo(const Vertex *to_vertex);

private:
  DISALLOW_COPY_AND_ASSIGN(GenClkInsertionSearchPred);
  bool isNonGeneratedClkPin(const Pin *pin) const;

  Clock *gclk_;
  GenclkInfo *genclk_info_;
};

GenClkInsertionSearchPred::GenClkInsertionSearchPred(Clock *gclk,
						     TagGroupBldr *tag_bldr,
						     GenclkInfo *genclk_info,
						     const StaState *sta) :
  SearchPred0(sta),
  DynLoopSrchPred(tag_bldr),
  gclk_(gclk),
  genclk_info_(genclk_info)
{
}

bool
GenClkInsertionSearchPred::searchThru(Edge *edge)
{
  const Graph *graph = sta_->graph();
  const Sdc *sdc = sta_->sdc();
  Search *search = sta_->search();
  const TimingRole *role = edge->role();
  EdgeSet *fdbk_edges = genclk_info_->fdbkEdges();
  return SearchPred0::searchThru(edge)
    && !role->isTimingCheck()
    && (sdc->clkThruTristateEnabled()
	|| !(role == TimingRole::tristateEnable()
	     || role == TimingRole::tristateDisable()))
    && !(fdbk_edges && fdbk_edges->hasKey(edge))
    && loopEnabled(edge, sdc, graph, search);
}

bool
GenClkInsertionSearchPred::searchTo(const Vertex *to_vertex)
{
  Pin *to_pin = to_vertex->pin();
  return SearchPred0::searchTo(to_vertex)
    // Propagate through other generated clock roots but not regular
    // clock roots.
    && !(!gclk_->leafPins().hasKey(to_pin)
	 && isNonGeneratedClkPin(to_pin))
    && genclk_info_->fanins()->hasKey(const_cast<Vertex*>(to_vertex));
}

bool
GenClkInsertionSearchPred::isNonGeneratedClkPin(const Pin *pin) const
{
  const Sdc *sdc = sta_->sdc();
  ClockSet *clks = sdc->findLeafPinClocks(pin);
  if (clks) {
    ClockSet::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      const Clock *clk = clk_iter.next();
      if (!clk->isGenerated())
	return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////

void
Genclks::findInsertionDelays(Clock *gclk)
{
  debugPrint1(debug_, "genclk", 2, "find gen clk %s insertion\n",
	      gclk->name());
  GenclkInfo *genclk_info = makeGenclkInfo(gclk);
  FilterPath *src_filter = genclk_info->srcFilter();
  GenClkInsertionSearchPred srch_pred(gclk, nullptr, genclk_info, this);
  BfsFwdIterator insert_iter(BfsIndex::other, &srch_pred, this);
  seedSrcPins(gclk, src_filter, insert_iter);
  // Propagate arrivals to generated clk root pin level.
  findSrcArrivals(gclk, insert_iter, genclk_info);
  // Unregister the filter so that it is not triggered by other searches.
  // The exception itself has to stick around because the source path
  // tags reference it.
  sdc_->unrecordException(src_filter);
}

GenclkInfo *
Genclks::makeGenclkInfo(Clock *gclk)
{
  FilterPath *src_filter = makeSrcFilter(gclk);
  Level gclk_level = clkPinMaxLevel(gclk);
  VertexSet *fanins = new VertexSet;
  findFanin(gclk, fanins);
  GenclkInfo *genclk_info = new GenclkInfo(gclk, gclk_level, fanins,
					    src_filter);
  genclk_info_map_.insert(gclk, genclk_info);
  return genclk_info;
}

GenclkInfo *
Genclks::genclkInfo(const Clock *gclk) const
{
  return genclk_info_map_.findKey(const_cast<Clock*>(gclk));
}

FilterPath *
Genclks::srcFilter(Clock *gclk)
{
  GenclkInfo *genclk_info = genclk_info_map_.findKey(gclk);
  if (genclk_info)
    return genclk_info->srcFilter();
  else
    return nullptr;
}

EdgeSet *
Genclks::latchFdbkEdges(const Clock *clk)
{
  GenclkInfo *genclk_info = genclkInfo(clk);
  return genclk_info->fdbkEdges();
}

void
Genclks::findLatchFdbkEdges(const Clock *clk)
{
  GenclkInfo *genclk_info = genclkInfo(clk);
  if (!genclk_info->foundLatchFdbkEdges())
    findLatchFdbkEdges(clk, genclk_info);
}

// Generated clock insertion delays propagate through latch D->Q.
// This exposes loops through latches that are not discovered and
// flagged by levelization.  Find these loops with a depth first
// search from the master clock source pins and record them to prevent
// the clock insertion search from searching through them.
//
// Because this is relatively expensive to search and it is rare to
// find latches in the clock network it is only called when a latch
// D to Q edge is encountered in the BFS arrival search.
void
Genclks::findLatchFdbkEdges(const Clock *gclk,
			    GenclkInfo *genclk_info)
{
  Level gclk_level = genclk_info->gclkLevel();
  EdgeSet *fdbk_edges = nullptr;
  for (Pin *pin : gclk->masterClk()->leafPins()) {
    Vertex *vertex = graph_->pinDrvrVertex(pin);
    VertexSet path_vertices;
    VertexSet visited_vertices;
    SearchPred1 srch_pred(this);
    findLatchFdbkEdges(vertex, gclk_level, srch_pred, path_vertices,
		       visited_vertices, fdbk_edges);
  }
  genclk_info->setLatchFdbkEdges(fdbk_edges);
  genclk_info->setFoundLatchFdbkEdges(true);
}

void
Genclks::findLatchFdbkEdges(Vertex *from_vertex,
			    Level gclk_level,
			    SearchPred &srch_pred,
			    VertexSet &path_vertices,
			    VertexSet &visited_vertices,
			    EdgeSet *&fdbk_edges)
{
  if (!visited_vertices.hasKey(from_vertex)) {
    visited_vertices.insert(from_vertex);
    path_vertices.insert(from_vertex);
    VertexOutEdgeIterator edge_iter(from_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (path_vertices.hasKey(to_vertex)) {
	debugPrint2(debug_, "genclk", 2, " found feedback edge %s -> %s\n",
		    from_vertex->name(sdc_network_),
		    to_vertex->name(sdc_network_));
	if (fdbk_edges == nullptr)
	  fdbk_edges = new EdgeSet;
	fdbk_edges->insert(edge);
      }
      else if (srch_pred.searchThru(edge)
	       && srch_pred.searchTo(to_vertex)
	       && to_vertex->level() <= gclk_level)
	findLatchFdbkEdges(to_vertex, gclk_level, srch_pred,
			   path_vertices, visited_vertices, fdbk_edges);
    }
    path_vertices.erase(from_vertex);
  }
}

FilterPath *
Genclks::makeSrcFilter(Clock *gclk)
{
  ClockSet *from_clks = new ClockSet;
  from_clks->insert(gclk->masterClk());
  const RiseFallBoth *rf = RiseFallBoth::riseFall();
  ExceptionFrom *from = sdc_->makeExceptionFrom(nullptr,from_clks,nullptr,rf);

  PinSet *thru_pins = new PinSet;
  thru_pins->insert(gclk->srcPin());
  ExceptionThru *thru = sdc_->makeExceptionThru(thru_pins,nullptr,nullptr,rf);
  ExceptionThruSeq *thrus = new ExceptionThruSeq;
  thrus->push_back(thru);

  ClockSet *to_clks = new ClockSet;
  to_clks->insert(gclk);
  ExceptionTo *to = sdc_->makeExceptionTo(nullptr, to_clks, nullptr, rf, rf);

  return sdc_->makeFilterPath(from, thrus, to);
}

void
Genclks::seedSrcPins(Clock *gclk,
		     FilterPath *src_filter,
		     BfsFwdIterator &insert_iter)
{
  Clock *master_clk = gclk->masterClk();
  for (Pin *master_pin : master_clk->leafPins()) {
    Vertex *vertex = graph_->pinDrvrVertex(master_pin);
    debugPrint1(debug_, "genclk", 2, " seed src pin %s\n",
		network_->pathName(master_pin));
    TagGroupBldr tag_bldr(true, this);
    tag_bldr.init(vertex);
    copyGenClkSrcPaths(vertex, &tag_bldr);
    for (auto path_ap : corners_->pathAnalysisPts()) {
      const MinMax *min_max = path_ap->pathMinMax();
      const EarlyLate *early_late = min_max;
      for (auto tr : RiseFall::range()) {
	Tag *tag = makeTag(gclk, master_clk, master_pin, tr, src_filter,
			   path_ap);
	Arrival insert = search_->clockInsertion(master_clk, master_pin, tr,
						 min_max, early_late, path_ap);
	tag_bldr.setArrival(tag, insert, nullptr);
      }
    }
    search_->setVertexArrivals(vertex, &tag_bldr);
    insert_iter.enqueueAdjacentVertices(vertex);
  }
}

Tag *
Genclks::makeTag(const Clock *gclk,
		 const Clock *master_clk,
		 const Pin *master_pin,
		 const RiseFall *master_rf,
		 FilterPath *src_filter,
		 const PathAnalysisPt *path_ap)
{
  ExceptionState *state = src_filter->firstState();
  // If the src pin is one of the master pins the filter is active
  // from the get go.
  if (master_pin == gclk->srcPin())
    state = state->nextState();
  ExceptionStateSet *states = new ExceptionStateSet;
  states->insert(state);
  ClkInfo *clk_info = search_->findClkInfo(master_clk->edge(master_rf),
					   master_pin, true, nullptr, true,
					   nullptr, 0.0, 0.0, nullptr,
					   path_ap, nullptr);
  return search_->findTag(master_rf, path_ap, clk_info, false, nullptr, false,
			  states, true);
}

class GenClkArrivalSearchPred : public EvalPred
{
public:
  GenClkArrivalSearchPred(Clock *gclk,
			  const StaState *sta);
  bool searchThru(Edge *edge);
  virtual bool searchTo(const Vertex *to_vertex);

private:
  DISALLOW_COPY_AND_ASSIGN(GenClkArrivalSearchPred);

  bool combinational_;
};

GenClkArrivalSearchPred::GenClkArrivalSearchPred(Clock *gclk,
						 const StaState *sta) :
  EvalPred(sta),
  combinational_(gclk->combinational())
{
}

bool
GenClkArrivalSearchPred::searchThru(Edge *edge)
{
  const Sdc *sdc = sta_->sdc();
  const TimingRole *role = edge->role();
  return EvalPred::searchThru(edge)
    && (role == TimingRole::combinational()
	|| role->isWire()
	|| !combinational_)
    && (sdc->clkThruTristateEnabled()
	|| !(role == TimingRole::tristateEnable()
	     || role == TimingRole::tristateDisable()));
}

// Override EvalPred::searchTo to search to generated clock pin.
bool
GenClkArrivalSearchPred::searchTo(const Vertex *to_vertex)
{
  return SearchPred0::searchTo(to_vertex);
}

class GenclkSrcArrivalVisitor : public ArrivalVisitor
{
public:
  GenclkSrcArrivalVisitor(Clock *gclk,
			  BfsFwdIterator *insert_iter,
			  GenclkInfo *genclk_info,
			  const StaState *sta);
  virtual VertexVisitor *copy();
  virtual void visit(Vertex *vertex);

protected:
  GenclkSrcArrivalVisitor(Clock *gclk,
			  BfsFwdIterator *insert_iter,
			  GenclkInfo *genclk_info,
			  bool always_to_endpoints,
			  SearchPred *pred,
			  const StaState *sta);

  Clock *gclk_;
  BfsFwdIterator *insert_iter_;
  GenclkInfo *genclk_info_;
  GenClkInsertionSearchPred srch_pred_;
};

GenclkSrcArrivalVisitor::GenclkSrcArrivalVisitor(Clock *gclk,
						 BfsFwdIterator *insert_iter,
						 GenclkInfo *genclk_info,
						 const StaState *sta):
  ArrivalVisitor(sta),
  gclk_(gclk),
  insert_iter_(insert_iter),
  genclk_info_(genclk_info),
  srch_pred_(gclk_, tag_bldr_, genclk_info, sta)
{
}

// Copy constructor.
GenclkSrcArrivalVisitor::GenclkSrcArrivalVisitor(Clock *gclk,
						 BfsFwdIterator *insert_iter,
						 GenclkInfo *genclk_info,
						 bool always_to_endpoints,
						 SearchPred *pred,
						 const StaState *sta) :
  ArrivalVisitor(always_to_endpoints, pred, sta),
  gclk_(gclk),
  insert_iter_(insert_iter),
  genclk_info_(genclk_info),
  srch_pred_(gclk, tag_bldr_, genclk_info, sta)
{
}

VertexVisitor *
GenclkSrcArrivalVisitor::copy()
{
  return new GenclkSrcArrivalVisitor(gclk_, insert_iter_, genclk_info_,
				     always_to_endpoints_, pred_, sta_);
}

void
GenclkSrcArrivalVisitor::visit(Vertex *vertex)
{
  const Debug *debug = sta_->debug();
  const Network *sdc_network = sta_->sdcNetwork();
  const Graph *graph = sta_->graph();
  Search *search = sta_->search();
  Genclks *genclks = search->genclks();
  debugPrint1(debug, "genclk", 2, "find gen clk insert arrival %s\n",
	      vertex->name(sdc_network));
  tag_bldr_->init(vertex);
  has_fanin_one_ = graph->hasFaninOne(vertex);
  genclks->copyGenClkSrcPaths(vertex, tag_bldr_);
  visitFaninPaths(vertex);
  // Propagate beyond the clock tree to reach generated clk roots.
  insert_iter_->enqueueAdjacentVertices(vertex, &srch_pred_);
  search->setVertexArrivals(vertex, tag_bldr_);
}

void
Genclks::findSrcArrivals(Clock *gclk,
			 BfsFwdIterator &insert_iter,
			 GenclkInfo *genclk_info)
{
  GenClkArrivalSearchPred eval_pred(gclk, this);
  GenclkSrcArrivalVisitor arrival_visitor(gclk, &insert_iter,
					  genclk_info, this);
  arrival_visitor.init(true, &eval_pred);
  // This cannot restrict the search level because loops in the clock tree
  // can circle back to the generated clock src pin.
  // Parallel visit is slightly slower (at last check).
  insert_iter.visit(levelize_->maxLevel(), &arrival_visitor);
}

// Copy existing generated clock source paths from vertex to tag_bldr.
void
Genclks::copyGenClkSrcPaths(Vertex *vertex,
			    TagGroupBldr *tag_bldr)
{
  Arrival *arrivals = graph_->arrivals(vertex);
  if (arrivals) {
    PathVertexRep *prev_paths = graph_->prevPaths(vertex);
    TagGroup *tag_group = search_->tagGroup(vertex);
    ArrivalMap::Iterator arrival_iter(tag_group->arrivalMap());
    while (arrival_iter.hasNext()) {
      Tag *tag;
      int arrival_index;
      arrival_iter.next(tag, arrival_index);
      if (tag->isGenClkSrcPath()) {
	Arrival arrival = arrivals[arrival_index];
	PathVertexRep *prev_path = prev_paths
	  ? &prev_paths[arrival_index]
	  : nullptr;
	tag_bldr->setArrival(tag, arrival, prev_path);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
Genclks::clearSrcPaths()
{
  genclk_src_paths_.deleteArrayContents();
  genclk_src_paths_.clear();
}

int
Genclks::srcPathIndex(const RiseFall *clk_rf,
		      const PathAnalysisPt *path_ap) const
{
  return path_ap->index() * RiseFall::index_count + clk_rf->index();
}

void
Genclks::recordSrcPaths(Clock *gclk)
{
  int path_count = RiseFall::index_count
    * corners_->pathAnalysisPtCount();

  bool divide_by_1 = gclk->isDivideByOneCombinational();
  bool invert = gclk->invert();
  bool has_edges = gclk->edges() != nullptr;

  for (Pin *gclk_pin : gclk->leafPins()) {
    PathVertexRep *src_paths = new PathVertexRep[path_count];
    genclk_src_paths_.insert(ClockPinPair(gclk, gclk_pin), src_paths);

    Vertex *gclk_vertex = srcPathVertex(gclk_pin);
    bool found_src_paths = false;
    VertexPathIterator path_iter(gclk_vertex, this);
    while (path_iter.hasNext()) {
      PathVertex *path = path_iter.next();
      ClockEdge *src_clk_edge = path->clkEdge(this);
      if (src_clk_edge
	  && matchesSrcFilter(path, gclk)) {
	const EarlyLate *early_late = path->minMax(this);
	RiseFall *src_clk_rf = src_clk_edge->transition();
	const RiseFall *rf = path->transition(this);
	bool inverting_path = (rf != src_clk_rf);
	const PathAnalysisPt *path_ap = path->pathAnalysisPt(this);
	int path_index = srcPathIndex(rf, path_ap);
	PathVertexRep &src_path = src_paths[path_index];
	if ((!divide_by_1
		|| (inverting_path == invert))
	    && (!has_edges
		|| src_clk_rf == gclk->masterClkEdgeTr(rf))
	    && (src_path.isNull()
		|| fuzzyGreater(path->arrival(this),
				     src_path.arrival(this),
				     early_late))) {
	  debugPrint4(debug_, "genclk", 2, "  %s insertion %s %s %s\n",
		      network_->pathName(gclk_pin),
		      early_late->asString(),
		      rf->asString(),
		      delayAsString(path->arrival(this), this));
	  src_path.init(path, this);
	  found_src_paths = true;
	}
      }
    }
    if (!found_src_paths
	// Don't warn if the master clock is ideal.
	&& gclk->masterClk()
	&& gclk->masterClk()->isPropagated())
      report_->warn("generated clock %s source pin %s missing paths from master clock %s.\n",
		    gclk->name(),
		    network_->pathName(gclk_pin),
		    gclk->masterClk()->name());
  }
}

bool
Genclks::matchesSrcFilter(Path *path,
			  const Clock *gclk) const
{
  Tag *tag = path->tag(this);
  const ExceptionStateSet *states = tag->states();
  if (tag->isGenClkSrcPath()
      && states) {
    ExceptionStateSet::ConstIterator state_iter(states);
    while (state_iter.hasNext()) {
      ExceptionState *state = state_iter.next();
      ExceptionPath *except = state->exception();
      if (except->isFilter()
	  && state->nextThru() == nullptr
	  && except->to()
	  && except->to()->matches(gclk))
	return true;
    }
  }
  return false;
}

void
Genclks::srcPath(Path *clk_path,
		 // Return value.
		 PathVertex &src_path) const
{
  const Pin *src_pin = clk_path->pin(this);
  ClockEdge *clk_edge = clk_path->clkEdge(this);
  const PathAnalysisPt *path_ap = clk_path->pathAnalysisPt(this);
  const EarlyLate *early_late = clk_path->minMax(this);
  PathAnalysisPt *insert_ap = path_ap->insertionAnalysisPt(early_late);
  srcPath(clk_edge->clock(), src_pin, clk_edge->transition(),
	  insert_ap, src_path);
}

void
Genclks::srcPath(const ClockEdge *clk_edge,
		 const Pin *src_pin,
		 const PathAnalysisPt *path_ap,
		 // Return value.
		 PathVertex &src_path) const
{
  srcPath(clk_edge->clock(), src_pin, clk_edge->transition(),
	  path_ap, src_path);
}

void
Genclks::srcPath(const Clock *gclk,
		 const Pin *src_pin,
		 const RiseFall *rf,
		 const PathAnalysisPt *path_ap,
		 // Return value.
		 PathVertex &src_path) const
{
  PathVertexRep *src_paths =
    genclk_src_paths_.findKey(ClockPinPair(gclk, src_pin));
  if (src_paths) {
    int path_index = srcPathIndex(rf, path_ap);
    src_path.init(src_paths[path_index], this);
  }
  else
    src_path.init();
}

Arrival
Genclks::insertionDelay(const Clock *clk,
			const Pin *pin,
			const RiseFall *rf,
			const EarlyLate *early_late,
			const PathAnalysisPt *path_ap) const
{
  PathVertex src_path;
  PathAnalysisPt *insert_ap = path_ap->insertionAnalysisPt(early_late);
  srcPath(clk, pin, rf, insert_ap, src_path);
  if (clk->pllFdbk()) {
    const MinMax *min_max = path_ap->pathMinMax();
    PathAnalysisPt *pll_ap = path_ap->insertionAnalysisPt(min_max->opposite());
    Arrival pll_delay = pllDelay(clk, rf, pll_ap);
    if (src_path.isNull())
      return -pll_delay;
    else {
      PathRef prev;
      src_path.prevPath(this, prev);
      if (prev.isNull())
	return -pll_delay;
      else
	// PLL delay replaces clkin->clkout delay.
	return prev.arrival(this) - pll_delay;
    }
  }
  else if (!src_path.isNull())
    return src_path.arrival(this);
  else
    return 0.0;
}

////////////////////////////////////////////////////////////////

void
Genclks::findPllDelays(Clock *gclk)
{
  debugPrint1(debug_, "genclk", 2, "find gen clk %s pll delay\n",
	      gclk->name());
  FilterPath *pll_filter = makePllFilter(gclk);
  GenclkInfo *genclk_info = genclkInfo(gclk);
  genclk_info->setPllFilter(pll_filter);
  ClkTreeSearchPred srch_pred(this);
  BfsFwdIterator pll_iter(BfsIndex::other, &srch_pred, this);
  seedPllPin(gclk, pll_filter, pll_iter);
  // Propagate arrivals to pll feedback pin level.
  findPllArrivals(gclk, pll_iter);
  sdc_->unrecordException(pll_filter);
}

FilterPath *
Genclks::makePllFilter(const Clock *gclk)
{
  PinSet *from_pins = new PinSet;
  from_pins->insert(gclk->pllOut());
  RiseFallBoth *rf = RiseFallBoth::riseFall();
  ExceptionFrom *from = sdc_->makeExceptionFrom(from_pins,nullptr,nullptr,rf);

  PinSet *to_pins = new PinSet;
  to_pins->insert(gclk->pllFdbk());
  ExceptionTo *to = sdc_->makeExceptionTo(to_pins, nullptr, nullptr, rf, rf);

  return sdc_->makeFilterPath(from, nullptr, to);
}

void
Genclks::seedPllPin(const Clock *gclk,
		    FilterPath *pll_filter,
		    BfsFwdIterator &pll_iter)
{
  Pin *pll_out_pin = gclk->pllOut();
  Vertex *vertex = graph_->pinDrvrVertex(pll_out_pin);
  debugPrint1(debug_, "genclk", 2, " seed pllout pin %s\n",
	      network_->pathName(pll_out_pin));
  TagGroupBldr tag_bldr(true, this);
  tag_bldr.init(vertex);
  copyGenClkSrcPaths(vertex, &tag_bldr);
  for (auto path_ap : corners_->pathAnalysisPts()) {
    for (auto tr : RiseFall::range()) {
      Tag *tag = makeTag(gclk, gclk, pll_out_pin, tr, pll_filter, path_ap);
      tag_bldr.setArrival(tag, 0.0, nullptr);
    }
  }
  search_->setVertexArrivals(vertex, &tag_bldr);
  pll_iter.enqueueAdjacentVertices(vertex);
}

class PllEvalPred : public EvalPred
{
public:
  explicit PllEvalPred(const StaState *sta);
  // Override EvalPred::searchTo to allow eval at generated clk root.
  virtual bool searchTo(const Vertex *to_vertex);
};

PllEvalPred::PllEvalPred(const StaState *sta) :
  EvalPred(sta)
{
}

bool
PllEvalPred::searchTo(const Vertex *)
{
  return true;
}

class PllArrivalVisitor : public ArrivalVisitor
{
public:
  PllArrivalVisitor(const StaState *sta,
		    BfsFwdIterator &pll_iter);
  virtual void visit(Vertex *vertex);

protected:
  BfsFwdIterator &pll_iter_;
};

PllArrivalVisitor::PllArrivalVisitor(const StaState *sta,
				     BfsFwdIterator &pll_iter) :
  ArrivalVisitor(sta),
  pll_iter_(pll_iter)
{
}

void
PllArrivalVisitor::visit(Vertex *vertex)
{
  const Debug *debug = sta_->debug();
  const Network *sdc_network = sta_->network();
  Graph *graph = sta_->graph();
  Search *search = sta_->search();
  Genclks *genclks = search->genclks();
  debugPrint1(debug, "genclk", 2, "find gen clk pll arrival %s\n",
	      vertex->name(sdc_network));
  tag_bldr_->init(vertex);
  genclks->copyGenClkSrcPaths(vertex, tag_bldr_);
  has_fanin_one_ = graph->hasFaninOne(vertex);
  visitFaninPaths(vertex);
  pll_iter_.enqueueAdjacentVertices(vertex);
  search->setVertexArrivals(vertex, tag_bldr_);
}

void
Genclks::findPllArrivals(const Clock *gclk,
			 BfsFwdIterator &pll_iter)
{
  Pin *fdbk_pin = gclk->pllFdbk();
  Vertex *fdbk_vertex = graph_->pinLoadVertex(fdbk_pin);
  Level fdbk_level = fdbk_vertex->level();
  PllArrivalVisitor arrival_visitor(this, pll_iter);
  PllEvalPred eval_pred(this);
  arrival_visitor.init(true, &eval_pred);
  while (pll_iter.hasNext(fdbk_level)) {
    Vertex *vertex = pll_iter.next();
    arrival_visitor.visit(vertex);
  }
}

Arrival
Genclks::pllDelay(const Clock *clk,
		  const RiseFall *rf,
		  const PathAnalysisPt *path_ap) const
{
  Vertex *fdbk_vertex = graph_->pinLoadVertex(clk->pllFdbk());
  GenclkInfo *genclk_info = genclkInfo(clk);
  if (genclk_info) {
    FilterPath *pll_filter = genclk_info->pllFilter();
    VertexPathIterator fdbk_path_iter(fdbk_vertex, rf, path_ap, this);
    while (fdbk_path_iter.hasNext()) {
      Path *path = fdbk_path_iter.next();
      if (matchesPllFilter(path, pll_filter))
	return path->arrival(this);
    }
  }
  return delay_zero;
}

bool
Genclks::matchesPllFilter(Path *path,
			  FilterPath *pll_filter) const
{
  Tag *tag = path->tag(this);
  const ExceptionStateSet *states = tag->states();
  if (tag->isGenClkSrcPath()
      && states) {
    ExceptionStateSet::ConstIterator state_iter(states);
    while (state_iter.hasNext()) {
      ExceptionState *state = state_iter.next();
      ExceptionPath *except = state->exception();
      if (except == pll_filter)
	return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////

bool
ClockPinPairLess::operator()(const ClockPinPair &pair1,
			     const ClockPinPair &pair2) const

{
  const Clock *clk1 = pair1.first;
  const Clock *clk2 = pair2.first;
  int clk_index1 = clk1->index();
  int clk_index2 = clk2->index();
  const Pin *pin1 = pair1.second;
  const Pin *pin2 = pair2.second;
  return (clk_index1 < clk_index2
	  || (clk_index1 == clk_index2
	      && pin1 < pin2));
}

class ClockPinPairHash
{
public:
  size_t operator()(const ClockPinPair &pair) const;
};

size_t
ClockPinPairHash::operator()(const ClockPinPair &pair) const
{
  return hashSum(pair.first->index(), hashPtr(pair.second));
}

class ClockPinPairEqual
{
public:
  bool operator()(const ClockPinPair &pair1,
		  const ClockPinPair &pair2) const;
};

bool
ClockPinPairEqual::operator()(const ClockPinPair &pair1,
			      const ClockPinPair &pair2) const

{
  return pair1.first == pair2.first
    && pair1.second == pair2.second;
}

} // namespace
