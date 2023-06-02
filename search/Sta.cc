// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include "Sta.hh"

#include "Machine.hh"
#include "DispatchQueue.hh"
#include "ReportTcl.hh"
#include "Debug.hh"
#include "Stats.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "FuncExpr.hh"
#include "EquivCells.hh"
#include "Liberty.hh"
#include "liberty/LibertyReader.hh"
#include "LibertyWriter.hh"
#include "SdcNetwork.hh"
#include "MakeConcreteNetwork.hh"
#include "PortDirection.hh"
#include "VerilogReader.hh"
#include "Graph.hh"
#include "GraphCmp.hh"
#include "Sdc.hh"
#include "WriteSdc.hh"
#include "ExceptionPath.hh"
#include "MakeConcreteParasitics.hh"
#include "Parasitics.hh"
#include "parasitics/SpefReader.hh"
#include "parasitics/ReportParasiticAnnotation.hh"
#include "DelayCalc.hh"
#include "ArcDelayCalc.hh"
#include "dcalc/GraphDelayCalc1.hh"
#include "sdf/SdfWriter.hh"
#include "Levelize.hh"
#include "Sim.hh"
#include "ClkInfo.hh"
#include "TagGroup.hh"
#include "PathAnalysisPt.hh"
#include "Corner.hh"
#include "Search.hh"
#include "Latches.hh"
#include "PathGroup.hh"
#include "CheckTiming.hh"
#include "CheckSlewLimits.hh"
#include "CheckFanoutLimits.hh"
#include "CheckCapacitanceLimits.hh"
#include "CheckMinPulseWidths.hh"
#include "CheckMinPeriods.hh"
#include "CheckMaxSkews.hh"
#include "ClkSkew.hh"
#include "FindRegister.hh"
#include "ReportPath.hh"
#include "VisitPathGroupVertices.hh"
#include "Genclks.hh"
#include "ClkNetwork.hh"
#include "power/Power.hh"
#include "VisitPathEnds.hh"
#include "PathExpanded.hh"
#include "MakeTimingModel.hh"

namespace sta {

using std::min;
using std::max;

static const ClockEdge *clk_edge_wildcard = reinterpret_cast<ClockEdge*>(1);

static bool
libertyPortCapsEqual(const LibertyPort *port1,
		     const LibertyPort *port2);
static bool
hasDisabledArcs(Edge *edge,
		Graph *graph);
static InstanceSet
pinInstances(PinSet &pins,
	     const Network *network);

////////////////////////////////////////////////////////////////
//
// Observers are used to propagate updates from a component
// to other components.
//
////////////////////////////////////////////////////////////////

// When an incremental change is made the delay calculation
// changes downstream.  This invalidates the required times
// for all vertices upstream of the changes.
class StaDelayCalcObserver : public DelayCalcObserver
{
public:
  explicit StaDelayCalcObserver(Search *search);
  virtual void delayChangedFrom(Vertex *vertex);
  virtual void delayChangedTo(Vertex *vertex);
  virtual void checkDelayChangedTo(Vertex *vertex);

private:
  Search *search_;
};

StaDelayCalcObserver::StaDelayCalcObserver(Search *search) :
  DelayCalcObserver(),
  search_(search)
{
}

void
StaDelayCalcObserver::delayChangedFrom(Vertex *vertex)
{
  search_->requiredInvalid(vertex);
}

void
StaDelayCalcObserver::delayChangedTo(Vertex *vertex)
{
  search_->arrivalInvalid(vertex);
}

void
StaDelayCalcObserver::checkDelayChangedTo(Vertex *vertex)
{
  search_->requiredInvalid(vertex);
}

////////////////////////////////////////////////////////////////

class StaSimObserver : public SimObserver
{
public:
  StaSimObserver(GraphDelayCalc *graph_delay_calc,
		 Levelize *levelize,
		 Search *search);
  virtual void valueChangeAfter(Vertex *vertex);
  virtual void faninEdgesChangeAfter(Vertex *vertex);
  virtual void fanoutEdgesChangeAfter(Vertex *vertex);

private:
  GraphDelayCalc *graph_delay_calc_;
  Levelize *levelize_;
  Search *search_;
};

StaSimObserver::StaSimObserver(GraphDelayCalc *graph_delay_calc,
			       Levelize *levelize,
			       Search *search) :
  SimObserver(),
  graph_delay_calc_(graph_delay_calc),
  levelize_(levelize),
  search_(search)
{
}

// When pins with constant values are incrementally connected to a net
// the downstream delays and arrivals will not be updated (removed)
// because the search predicate does not search through constants.
// This observer makes sure the delays and arrivals are invalidated.
void
StaSimObserver::valueChangeAfter(Vertex *vertex)
{
  graph_delay_calc_->delayInvalid(vertex);
  search_->arrivalInvalid(vertex);
  search_->requiredInvalid(vertex);
  search_->endpointInvalid(vertex);
  levelize_->invalidFrom(vertex);
}

void
StaSimObserver::faninEdgesChangeAfter(Vertex *vertex)
{
  graph_delay_calc_->delayInvalid(vertex);
  search_->arrivalInvalid(vertex);
  search_->endpointInvalid(vertex);
}

void
StaSimObserver::fanoutEdgesChangeAfter(Vertex *vertex)
{
  search_->requiredInvalid(vertex);
  search_->endpointInvalid(vertex);
}

////////////////////////////////////////////////////////////////

class StaLevelizeObserver : public LevelizeObserver
{
public:
  StaLevelizeObserver(Search *search);
  virtual void levelChangedBefore(Vertex *vertex);

private:
  Search *search_;
};

StaLevelizeObserver::StaLevelizeObserver(Search *search) :
  search_(search)
{
}

void
StaLevelizeObserver::levelChangedBefore(Vertex *vertex)
{
  search_->levelChangedBefore(vertex);
}

////////////////////////////////////////////////////////////////

void
initSta()
{
  initElapsedTime();
  TimingRole::init();
  PortDirection::init();
  initTmpStrings();
  initLiberty();
  initDelayConstants();
  registerDelayCalcs();
  initPathSenseThru();
}

void
deleteAllMemory()
{
  // Verilog modules refer to the network in the sta so it has
  // to deleted before the sta.
  deleteVerilogReader();
  Sta *sta = Sta::sta();
  if (sta) {
    delete sta;
    Sta::setSta(nullptr);
  }
  deleteDelayCalcs();
  deleteTmpStrings();
  TimingRole::destroy();
  PortDirection::destroy();
  deleteLiberty();
}

////////////////////////////////////////////////////////////////

// Singleton used by TCL commands.
Sta *Sta::sta_;

Sta::Sta() :
  StaState(),
  current_instance_(nullptr),
  check_timing_(nullptr),
  check_slew_limits_(nullptr),
  check_fanout_limits_(nullptr),
  check_capacitance_limits_(nullptr),
  check_min_pulse_widths_(nullptr),
  check_min_periods_(nullptr),
  check_max_skews_(nullptr),
  clk_skews_(nullptr),
  report_path_(nullptr),
  power_(nullptr),
  link_make_black_boxes_(true),
  update_genclks_(false),
  equiv_cells_(nullptr),
  graph_sdc_annotated_(false),
  // Default to same parasitics for each corner min/max.
  parasitics_per_corner_(false),
  parasitics_per_min_max_(false)
{
}

void
Sta::makeComponents()
{
  makeReport();
  makeDebug();
  makeUnits();
  makeNetwork();
  makeSdc();
  makeLevelize();
  makeParasitics();
  makeCorners();
  makeArcDelayCalc();
  makeGraphDelayCalc();
  makeSim();
  makeSearch();
  makeLatches();
  makeClkNetwork();
  makeSdcNetwork();
  makeReportPath();
  makePower();
  setCmdNamespace1(CmdNamespace::sdc);
  setThreadCount1(defaultThreadCount());
  updateComponentsState();

  makeObservers();
  // This must follow updateComponentsState.
  makeParasiticAnalysisPts();
}

void
Sta::makeObservers()
{
  graph_delay_calc_->setObserver(new StaDelayCalcObserver(search_));
  sim_->setObserver(new StaSimObserver(graph_delay_calc_, levelize_, search_));
  levelize_->setObserver(new StaLevelizeObserver(search_));
}

int
Sta::defaultThreadCount() const
{
  return 1;
}

void
Sta::setThreadCount(int thread_count)
{
  setThreadCount1(thread_count);
  updateComponentsState();
}

void
Sta::setThreadCount1(int thread_count)
{
  thread_count_ = thread_count;
  if (dispatch_queue_)
    dispatch_queue_->setThreadCount(thread_count);
  else if (thread_count > 1)
    dispatch_queue_ = new DispatchQueue(thread_count);
}

void
Sta::updateComponentsState()
{
  network_->copyState(this);
  cmd_network_->copyState(this);
  sdc_network_->copyState(this);
  if (graph_)
    graph_->copyState(this);
  sdc_->copyState(this);
  corners_->copyState(this);
  levelize_->copyState(this);
  parasitics_->copyState(this);
  if (arc_delay_calc_)
    arc_delay_calc_->copyState(this);
  sim_->copyState(this);
  search_->copyState(this);
  latches_->copyState(this);
  graph_delay_calc_->copyState(this);
  report_path_->copyState(this);
  if (check_timing_)
    check_timing_->copyState(this);
  clk_network_->copyState(this);
  if (power_)
    power_->copyState(this);
}

void
Sta::makeReport()
{
  report_ = new ReportTcl();
}

void
Sta::makeDebug()
{
  debug_ = new Debug(report_);
}

void
Sta::makeUnits()
{
  units_ = new Units();
}

void
Sta::makeNetwork()
{
  network_ = makeConcreteNetwork();
}

void
Sta::makeSdc()
{
  sdc_ = new Sdc(this);
}

void
Sta::makeLevelize()
{
  levelize_ = new Levelize(this);
}

void
Sta::makeParasitics()
{
  parasitics_ = makeConcreteParasitics(this);
}

void
Sta::makeArcDelayCalc()
{
  arc_delay_calc_ = makeDelayCalc("dmp_ceff_elmore", this);
}

void
Sta::makeGraphDelayCalc()
{
  graph_delay_calc_ = new GraphDelayCalc1(this);
}

void
Sta::makeSim()
{
  sim_ = new Sim(this);
}

void
Sta::makeSearch()
{
  search_ = new Search(this);
}

void
Sta::makeLatches()
{
  latches_ = new Latches(this);
}

void
Sta::makeSdcNetwork()
{
  sdc_network_ = sta::makeSdcNetwork(network_);
}

void
Sta::makeCheckTiming()
{
  check_timing_ = new CheckTiming(this);
}

void
Sta::makeCheckSlewLimits()
{
  check_slew_limits_ = new CheckSlewLimits(this);
}

void
Sta::makeCheckFanoutLimits()
{
  check_fanout_limits_ = new CheckFanoutLimits(this);
}

void
Sta::makeCheckCapacitanceLimits()
{
  check_capacitance_limits_ = new CheckCapacitanceLimits(this);
}

void
Sta::makeCheckMinPulseWidths()
{
  check_min_pulse_widths_ = new CheckMinPulseWidths(this);
}

void
Sta::makeCheckMinPeriods()
{
  check_min_periods_ = new CheckMinPeriods(this);
}

void
Sta::makeCheckMaxSkews()
{
  check_max_skews_ = new CheckMaxSkews(this);
}

void
Sta::makeReportPath()
{
  report_path_ = new ReportPath(this);
}

void
Sta::makeClkNetwork()
{
  clk_network_ = new ClkNetwork(this);
}

void
Sta::makePower()
{
  power_ = new Power(this);
}

void
Sta::setSta(Sta *sta)
{
  sta_ = sta;
}

Sta *
Sta::sta()
{
  return sta_;
}

Sta::~Sta()
{
  // Delete "top down" to minimize chance of referencing deleted memory.
  delete check_slew_limits_;
  delete check_fanout_limits_;
  delete check_capacitance_limits_;
  delete check_min_pulse_widths_;
  delete check_min_periods_;
  delete check_max_skews_;
  delete clk_skews_;
  delete check_timing_;
  delete report_path_;
  // Constraints reference search filter, so delete search first.
  delete search_;
  delete latches_;
  delete parasitics_;
  if (arc_delay_calc_)
    delete arc_delay_calc_;
  delete graph_delay_calc_;
  delete sim_;
  delete levelize_;
  delete sdc_;
  delete corners_;
  delete graph_;
  delete sdc_network_;
  delete network_;
  delete debug_;
  delete units_;
  delete report_;
  delete clk_network_;
  delete power_;
  delete equiv_cells_;
  delete dispatch_queue_;
}

void
Sta::clear()
{
  clkPinsInvalid();
  // Constraints reference search filter, so clear search first.
  search_->clear();
  sdc_->clear();
  graph_sdc_annotated_ = false;
  // corners are NOT cleared because they are used to index liberty files.
  levelize_->clear();
  if (parasitics_)
    parasitics_->clear();
  graph_delay_calc_->clear();
  sim_->clear();
  if (check_min_pulse_widths_)
    check_min_pulse_widths_->clear();
  if (check_min_periods_)
    check_min_periods_->clear();
  delete graph_;
  graph_ = nullptr;
  current_instance_ = nullptr;
  // Notify components that graph is toast.
  updateComponentsState();
}

void
Sta::networkChanged()
{
  // Everything else from clear().
  search_->clear();
  levelize_->clear();
  if (parasitics_)
    parasitics_->clear();
  graph_delay_calc_->clear();
  sim_->clear();
  if (check_min_pulse_widths_)
    check_min_pulse_widths_->clear();
  if (check_min_periods_)
    check_min_periods_->clear();
  delete graph_;
  graph_ = nullptr;
  graph_sdc_annotated_ = false;
  current_instance_ = nullptr;
  updateComponentsState();
}

void
Sta::setTclInterp(Tcl_Interp *interp)
{
  tcl_interp_ = interp;
  report_->setTclInterp(interp);
}

Tcl_Interp *
Sta::tclInterp()
{
  return tcl_interp_;
}

CmdNamespace
Sta::cmdNamespace()
{
  return cmd_namespace_;
}

void
Sta::setCmdNamespace(CmdNamespace namespc)
{
  setCmdNamespace1(namespc);
  updateComponentsState();
}

void
Sta::setCmdNamespace1(CmdNamespace namespc)
{
  cmd_namespace_ = namespc;
  switch (cmd_namespace_) {
  case CmdNamespace::sta:
    cmd_network_ = network_;
    break;
  case CmdNamespace::sdc:
    cmd_network_ = sdc_network_;
    break;
  }
}

Instance *
Sta::currentInstance() const
{
  if (current_instance_ == nullptr)
    return network_->topInstance();
  else
    return current_instance_;
}

void
Sta::setCurrentInstance(Instance *inst)
{
  current_instance_ = inst;
}

////////////////////////////////////////////////////////////////

LibertyLibrary *
Sta::readLiberty(const char *filename,
		 Corner *corner,
		 const MinMaxAll *min_max,
		 bool infer_latches)
{
  Stats stats(debug_, report_);
  LibertyLibrary *library = readLibertyFile(filename, corner, min_max,
                                            infer_latches);
  if (library
      // The default library is the first library read.
      // This corresponds to a link_path of '*'.
      && network_->defaultLibertyLibrary() == nullptr) {
    network_->setDefaultLibertyLibrary(library);
    // Set units from default (first) library.
    *units_ = *library->units();
  }
  stats.report("Read liberty");
  return library;
}

LibertyLibrary *
Sta::readLibertyFile(const char *filename,
		     Corner *corner,
		     const MinMaxAll *min_max,
		     bool infer_latches)
{
  LibertyLibrary *liberty = sta::readLibertyFile(filename, infer_latches,
						 network_);
  if (liberty) {
    // Don't map liberty cells if they are redefined by reading another
    // library with the same cell names.
    if (min_max == MinMaxAll::all()) {
      readLibertyAfter(liberty, corner, MinMax::min());
      readLibertyAfter(liberty, corner, MinMax::max());
    }
    else
      readLibertyAfter(liberty, corner, min_max->asMinMax());
    network_->readLibertyAfter(liberty);
  }
  return liberty;
}

LibertyLibrary *
Sta::readLibertyFile(const char *filename,
		     bool infer_latches)
{
  return sta::readLibertyFile(filename, infer_latches, network_);
}

void
Sta::readLibertyAfter(LibertyLibrary *liberty,
		      Corner *corner,
		      const MinMax *min_max)
{
  corner->addLiberty(liberty, min_max);
  LibertyLibrary::makeCornerMap(liberty, corner->libertyIndex(min_max),
				network_, report_);
}

bool
Sta::setMinLibrary(const char *min_filename,
		   const char *max_filename)
{
  LibertyLibrary *max_lib = network_->findLibertyFilename(max_filename);
  if (max_lib) {
    LibertyLibrary *min_lib = readLibertyFile(min_filename, cmd_corner_,
					      MinMaxAll::min(), false);
    return min_lib != nullptr;
  }
  else
    return false;
}

void
Sta::readNetlistBefore()
{
  clear();
  NetworkReader *network_reader = networkReader();
  if (network_reader)
    network_reader->readNetlistBefore();
}

bool
Sta::linkDesign(const char *top_cell_name)
{
  clear();
  Stats stats(debug_, report_);
  bool status = network_->linkNetwork(top_cell_name,
				      link_make_black_boxes_,
				      report_);
  stats.report("Link");
  return status;
}

bool
Sta::linkMakeBlackBoxes() const
{
  return link_make_black_boxes_;
}

void
Sta::setLinkMakeBlackBoxes(bool make)
{
  link_make_black_boxes_ = make;
}

////////////////////////////////////////////////////////////////

void
Sta::setDebugLevel(const char *what,
		   int level)
{
  debug_->setLevel(what, level);
}

////////////////////////////////////////////////////////////////

void
Sta::setAnalysisType(AnalysisType analysis_type)
{
  if (analysis_type != sdc_->analysisType()) {
    sdc_->setAnalysisType(analysis_type);
    graph_delay_calc_->delaysInvalid();
    search_->arrivalsInvalid();
    search_->deletePathGroups();
    corners_->analysisTypeChanged();
    if (graph_)
      graph_->setDelayCount(corners_->dcalcAnalysisPtCount());
  }
}

OperatingConditions *
Sta::operatingConditions(const MinMax *min_max) const
{
  return sdc_->operatingConditions(min_max);
}

void
Sta::setOperatingConditions(OperatingConditions *op_cond,
			    const MinMaxAll *min_max)
{
  sdc_->setOperatingConditions(op_cond, min_max);
  corners_->operatingConditionsChanged();
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

const Pvt *
Sta::pvt(Instance *inst,
	 const MinMax *min_max)
{
  return sdc_->pvt(inst, min_max);
}

void
Sta::setPvt(Instance *inst,
	    const MinMaxAll *min_max,
	    float process,
	    float voltage,
	    float temperature)
{
  Pvt pvt(process, voltage, temperature);
  setPvt(inst, min_max, pvt);
}

void
Sta::setPvt(const Instance *inst,
	    const MinMaxAll *min_max,
	    const Pvt &pvt)
{
  sdc_->setPvt(inst, min_max, pvt);
  delaysInvalidFrom(inst);
}

void
Sta::setTimingDerate(TimingDerateType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  sdc_->setTimingDerate(type, clk_data, rf, early_late, derate);
  // Delay calculation results are still valid.
  // The search derates delays while finding arrival times.
  search_->arrivalsInvalid();
}

void
Sta::setTimingDerate(const Net *net,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  sdc_->setTimingDerate(net, clk_data, rf, early_late, derate);
  // Delay calculation results are still valid.
  // The search derates delays while finding arrival times.
  search_->arrivalsInvalid();
}

void
Sta::setTimingDerate(const Instance *inst,
		     TimingDerateCellType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  sdc_->setTimingDerate(inst, type, clk_data, rf, early_late, derate);
  // Delay calculation results are still valid.
  // The search derates delays while finding arrival times.
  search_->arrivalsInvalid();
}

void
Sta::setTimingDerate(const LibertyCell *cell,
		     TimingDerateCellType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  sdc_->setTimingDerate(cell, type, clk_data, rf, early_late, derate);
  // Delay calculation results are still valid.
  // The search derates delays while finding arrival times.
  search_->arrivalsInvalid();
}

void
Sta::unsetTimingDerate()
{
  sdc_->unsetTimingDerate();
  // Delay calculation results are still valid.
  // The search derates delays while finding arrival times.
  search_->arrivalsInvalid();
}

void
Sta::setInputSlew(const Port *port,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max,
		  float slew)
{
  sdc_->setInputSlew(port, rf, min_max, slew);
  delaysInvalidFrom(port);
}

void
Sta::setDriveCell(const LibertyLibrary *library,
		  const LibertyCell *cell,
		  const Port *port,
		  const LibertyPort *from_port,
		  float *from_slews,
		  const LibertyPort *to_port,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max)
{
  sdc_->setDriveCell(library, cell, port, from_port, from_slews, to_port,
		     rf, min_max);
  delaysInvalidFrom(port);
}

void
Sta::setDriveResistance(const Port *port,
			const RiseFallBoth *rf,
			const MinMaxAll *min_max,
			float res)
{
  sdc_->setDriveResistance(port, rf, min_max, res);
  delaysInvalidFrom(port);
}

void
Sta::setLatchBorrowLimit(const Pin *pin,
			 float limit)
{
  sdc_->setLatchBorrowLimit(pin, limit);
  search_->requiredInvalid(pin);
}

void
Sta::setLatchBorrowLimit(const Instance *inst,
			 float limit)
{
  sdc_->setLatchBorrowLimit(inst, limit);
  search_->requiredInvalid(inst);
}

void
Sta::setLatchBorrowLimit(const Clock *clk,
			 float limit)
{
  sdc_->setLatchBorrowLimit(clk, limit);
  search_->arrivalsInvalid();
}

void
Sta::setMinPulseWidth(const RiseFallBoth *rf,
		      float min_width)
{
  sdc_->setMinPulseWidth(rf, min_width);
}

void
Sta::setMinPulseWidth(const Pin *pin,
		      const RiseFallBoth *rf,
		      float min_width)
{
  sdc_->setMinPulseWidth(pin, rf, min_width);
}

void
Sta::setMinPulseWidth(const Instance *inst,
		      const RiseFallBoth *rf,
		      float min_width)
{
  sdc_->setMinPulseWidth(inst, rf, min_width);
}

void
Sta::setMinPulseWidth(const Clock *clk,
		      const RiseFallBoth *rf,
		      float min_width)
{
  sdc_->setMinPulseWidth(clk, rf, min_width);
}

void
Sta::setWireloadMode(WireloadMode mode)
{
  sdc_->setWireloadMode(mode);
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Sta::setWireload(Wireload *wireload,
		 const MinMaxAll *min_max)
{
  sdc_->setWireload(wireload, min_max);
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Sta::setWireloadSelection(WireloadSelection *selection,
			  const MinMaxAll *min_max)
{
  sdc_->setWireloadSelection(selection, min_max);
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Sta::setSlewLimit(Clock *clk,
		  const RiseFallBoth *rf,
		  const PathClkOrData clk_data,
		  const MinMax *min_max,
		  float slew)
{
  sdc_->setSlewLimit(clk, rf, clk_data, min_max, slew);
}

void
Sta::setSlewLimit(Port *port,
		  const MinMax *min_max,
		  float slew)
{
  sdc_->setSlewLimit(port, min_max, slew);
}

void
Sta::setSlewLimit(Cell *cell,
		  const MinMax *min_max,
		  float slew)
{
  sdc_->setSlewLimit(cell, min_max, slew);
}

void
Sta::setCapacitanceLimit(Cell *cell,
			 const MinMax *min_max,
			 float cap)
{
  sdc_->setCapacitanceLimit(cell, min_max, cap);
}

void
Sta::setCapacitanceLimit(Port *port,
			 const MinMax *min_max,
			 float cap)
{
  sdc_->setCapacitanceLimit(port, min_max, cap);
}

void
Sta::setCapacitanceLimit(Pin *pin,
			 const MinMax *min_max,
			 float cap)
{
  sdc_->setCapacitanceLimit(pin, min_max, cap);
}

void
Sta::setFanoutLimit(Cell *cell,
		    const MinMax *min_max,
		    float fanout)
{
  sdc_->setFanoutLimit(cell, min_max, fanout);
}

void
Sta::setFanoutLimit(Port *port,
		    const MinMax *min_max,
		    float fanout)
{
  sdc_->setFanoutLimit(port, min_max, fanout);
}

void
Sta::setMaxArea(float area)
{
  sdc_->setMaxArea(area);
}

void
Sta::makeClock(const char *name,
	       PinSet *pins,
	       bool add_to_pins,
	       float period,
	       FloatSeq *waveform,
	       char *comment)
{
  sdc_->makeClock(name, pins, add_to_pins, period, waveform, comment);
  update_genclks_ = true;
  search_->arrivalsInvalid();
}

void
Sta::makeGeneratedClock(const char *name,
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
  sdc_->makeGeneratedClock(name, pins, add_to_pins,
			   src_pin, master_clk,
			   divide_by, multiply_by, duty_cycle,
			   invert, combinational,
			   edges, edge_shifts, comment);
  update_genclks_ = true;
  search_->arrivalsInvalid();
}

void
Sta::removeClock(Clock *clk)
{
  sdc_->removeClock(clk);
  search_->arrivalsInvalid();
}

bool
Sta::isClockSrc(const Pin *pin) const
{
  return sdc_->isClock(pin);
}

void
Sta::setPropagatedClock(Clock *clk)
{
  sdc_->setPropagatedClock(clk);
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
  clkPinsInvalid();
}

void
Sta::removePropagatedClock(Clock *clk)
{
  sdc_->removePropagatedClock(clk);
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
  clkPinsInvalid();
}

void
Sta::setPropagatedClock(Pin *pin)
{
  sdc_->setPropagatedClock(pin);
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
  clkPinsInvalid();
}

void
Sta::removePropagatedClock(Pin *pin)
{
  sdc_->removePropagatedClock(pin);
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
  clkPinsInvalid();
}

void
Sta::setClockSlew(Clock *clk,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max,
		  float slew)
{
  sdc_->setClockSlew(clk, rf, min_max, slew);
  clockSlewChanged(clk);
}

void
Sta::removeClockSlew(Clock *clk)
{
  sdc_->removeClockSlew(clk);
  clockSlewChanged(clk);
}

void
Sta::clockSlewChanged(Clock *clk)
{
  for (const Pin *pin : clk->pins())
    graph_delay_calc_->delayInvalid(pin);
  search_->arrivalsInvalid();
}

void
Sta::setClockLatency(Clock *clk,
		     Pin *pin,
		     const RiseFallBoth *rf,
		     const MinMaxAll *min_max,
		     float delay)
{
  sdcChangedGraph();
  sdc_->setClockLatency(clk, pin, rf, min_max, delay);
  search_->arrivalsInvalid();
}

void
Sta::sdcChangedGraph()
{
  if (graph_sdc_annotated_)
    sdc_->removeGraphAnnotations();
  graph_sdc_annotated_ = false;
}

void
Sta::ensureGraphSdcAnnotated()
{
  if (!graph_sdc_annotated_) {
    sdc_->annotateGraph();
    graph_sdc_annotated_ = true;
  }
}

void
Sta::removeClockLatency(const Clock *clk,
			const Pin *pin)
{
  sdcChangedGraph();
  sdc_->removeClockLatency(clk, pin);
  search_->arrivalsInvalid();
}

void
Sta::setClockInsertion(const Clock *clk,
		       const Pin *pin,
		       const RiseFallBoth *rf,
		       const MinMaxAll *min_max,
		       const EarlyLateAll *early_late,
		       float delay)
{
  sdc_->setClockInsertion(clk, pin, rf, min_max, early_late, delay);
  search_->arrivalsInvalid();
}

void
Sta::removeClockInsertion(const Clock *clk,
			  const Pin *pin)
{
  sdc_->removeClockInsertion(clk, pin);
  search_->arrivalsInvalid();
}

void
Sta::setClockUncertainty(Clock *clk,
			 const SetupHoldAll *setup_hold,
			 float uncertainty)
{
  clk->setUncertainty(setup_hold, uncertainty);
  search_->arrivalsInvalid();
}

void
Sta::removeClockUncertainty(Clock *clk,
			    const SetupHoldAll *setup_hold)
{
  clk->removeUncertainty(setup_hold);
  search_->arrivalsInvalid();
}

void
Sta::setClockUncertainty(Pin *pin,
			 const SetupHoldAll *setup_hold,
			 float uncertainty)
{
  sdc_->setClockUncertainty(pin, setup_hold, uncertainty);
  search_->arrivalsInvalid();
}

void
Sta::removeClockUncertainty(Pin *pin,
			    const SetupHoldAll *setup_hold)
{
  sdc_->removeClockUncertainty(pin, setup_hold);
  search_->arrivalsInvalid();
}

void
Sta::setClockUncertainty(Clock *from_clk,
			 const RiseFallBoth *from_rf,
			 Clock *to_clk,
			 const RiseFallBoth *to_rf,
			 const SetupHoldAll *setup_hold,
			 float uncertainty)
{
  sdc_->setClockUncertainty(from_clk, from_rf, to_clk, to_rf,
			    setup_hold, uncertainty);
  search_->arrivalsInvalid();
}

void
Sta::removeClockUncertainty(Clock *from_clk,
			    const RiseFallBoth *from_rf,
			    Clock *to_clk,
			    const RiseFallBoth *to_rf,
			    const SetupHoldAll *setup_hold)
{
  sdc_->removeClockUncertainty(from_clk, from_rf, to_clk, to_rf, setup_hold);
  search_->arrivalsInvalid();
}

ClockGroups *
Sta::makeClockGroups(const char *name,
		     bool logically_exclusive,
		     bool physically_exclusive,
		     bool asynchronous,
		     bool allow_paths,
		     const char *comment)
{
  ClockGroups *groups = sdc_->makeClockGroups(name,
					      logically_exclusive,
					      physically_exclusive,
					      asynchronous,
					      allow_paths,
					      comment);
  search_->requiredsInvalid();
  return groups;
}

void
Sta::removeClockGroupsLogicallyExclusive(const char *name)
{
  sdc_->removeClockGroupsLogicallyExclusive(name);
  search_->requiredsInvalid();
}

void
Sta::removeClockGroupsPhysicallyExclusive(const char *name)
{
  sdc_->removeClockGroupsPhysicallyExclusive(name);
  search_->requiredsInvalid();
}

void
Sta::removeClockGroupsAsynchronous(const char *name)
{
  sdc_->removeClockGroupsAsynchronous(name);
  search_->requiredsInvalid();
}

void
Sta::makeClockGroup(ClockGroups *clk_groups,
		    ClockSet *clks)
{
  sdc_->makeClockGroup(clk_groups, clks);
}

void
Sta::setClockSense(PinSet *pins,
		   ClockSet *clks,
		   ClockSense sense)
{
  sdc_->setClockSense(pins, clks, sense);
  search_->arrivalsInvalid();
}

////////////////////////////////////////////////////////////////

void
Sta::setClockGatingCheck(const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin)
{
  sdc_->setClockGatingCheck(rf, setup_hold, margin);
  search_->arrivalsInvalid();
}

void
Sta::setClockGatingCheck(Clock *clk,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin)
{
  sdc_->setClockGatingCheck(clk, rf, setup_hold, margin);
  search_->arrivalsInvalid();
}

void
Sta::setClockGatingCheck(Instance *inst,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin,
			 LogicValue active_value)
{
  sdc_->setClockGatingCheck(inst, rf, setup_hold, margin,active_value);
  search_->arrivalsInvalid();
}

void
Sta::setClockGatingCheck(Pin *pin,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin,
			 LogicValue active_value)
{
  sdc_->setClockGatingCheck(pin, rf, setup_hold, margin,active_value);
  search_->arrivalsInvalid();
}

void
Sta::setDataCheck(Pin *from,
		  const RiseFallBoth *from_rf,
		  Pin *to,
		  const RiseFallBoth *to_rf,
		  Clock *clk,
		  const SetupHoldAll *setup_hold,
		  float margin)
{
  sdcChangedGraph();
  sdc_->setDataCheck(from, from_rf, to, to_rf, clk, setup_hold,margin);
  search_->requiredInvalid(to);
}

void
Sta::removeDataCheck(Pin *from,
		     const RiseFallBoth *from_rf,
		     Pin *to,
		     const RiseFallBoth *to_rf,
		     Clock *clk,
		     const SetupHoldAll *setup_hold)
{
  sdc_->removeDataCheck(from, from_rf, to, to_rf, clk, setup_hold);
  search_->requiredInvalid(to);
}

////////////////////////////////////////////////////////////////

void
Sta::disable(Pin *pin)
{
  sdcChangedGraph();
  sdc_->disable(pin);
  // Levelization respects disabled edges.
  levelize_->invalid();
  graph_delay_calc_->delayInvalid(pin);
  search_->arrivalsInvalid();
}

void
Sta::removeDisable(Pin *pin)
{
  sdcChangedGraph();
  sdc_->removeDisable(pin);
  disableAfter();
  // Levelization respects disabled edges.
  levelize_->invalid();
  graph_delay_calc_->delayInvalid(pin);
  search_->arrivalsInvalid();
}

void
Sta::disable(Instance *inst,
	     LibertyPort *from,
	     LibertyPort *to)
{
  sdcChangedGraph();
  sdc_->disable(inst, from, to);

  if (from) {
    Pin *from_pin = network_->findPin(inst, from);
    graph_delay_calc_->delayInvalid(from_pin);
  }
  if (to) {
    Pin *to_pin = network_->findPin(inst, to);
    graph_delay_calc_->delayInvalid(to_pin);
  }
  if (from == nullptr && to == nullptr) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      graph_delay_calc_->delayInvalid(pin);
    }
    delete pin_iter;
  }
  // Levelization respects disabled edges.
  levelize_->invalid();
  search_->arrivalsInvalid();
}

void
Sta::removeDisable(Instance *inst,
		   LibertyPort *from,
		   LibertyPort *to)
{
  sdcChangedGraph();
  sdc_->removeDisable(inst, from, to);

  if (from) {
    Pin *from_pin = network_->findPin(inst, from);
    graph_delay_calc_->delayInvalid(from_pin);
  }
  if (to) {
    Pin *to_pin = network_->findPin(inst, to);
    graph_delay_calc_->delayInvalid(to_pin);
  }
  if (from == nullptr && to == nullptr) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      graph_delay_calc_->delayInvalid(pin);
    }
    delete pin_iter;
  }
  // Levelization respects disabled edges.
  levelize_->invalid();
  search_->arrivalsInvalid();
}

void
Sta::disable(LibertyCell *cell,
	     LibertyPort *from,
	     LibertyPort *to)
{
  sdc_->disable(cell, from, to);
  disableAfter();
}

void
Sta::removeDisable(LibertyCell *cell,
		   LibertyPort *from,
		   LibertyPort *to)
{
  sdc_->removeDisable(cell, from, to);
  disableAfter();
}

void
Sta::disable(LibertyPort *port)
{
  sdcChangedGraph();
  sdc_->disable(port);
  disableAfter();
}

void
Sta::removeDisable(LibertyPort *port)
{
  sdcChangedGraph();
  sdc_->removeDisable(port);
  disableAfter();
}

void
Sta::disable(Port *port)
{
  sdc_->disable(port);
  disableAfter();
}

void
Sta::removeDisable(Port *port)
{
  sdc_->removeDisable(port);
  disableAfter();
}

void
Sta::disable(Edge *edge)
{
  sdc_->disable(edge);
  disableAfter();
}

void
Sta::removeDisable(Edge *edge)
{
  sdc_->removeDisable(edge);
  disableAfter();
}

void
Sta::disable(TimingArcSet *arc_set)
{
  sdc_->disable(arc_set);
  disableAfter();
}

void
Sta::removeDisable(TimingArcSet *arc_set)
{
  sdc_->removeDisable(arc_set);
  disableAfter();
}

void
Sta::disableAfter()
{
  // Levelization respects disabled edges.
  levelize_->invalid();
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

////////////////////////////////////////////////////////////////

EdgeSeq
Sta::disabledEdges()
{
  ensureLevelized();
  EdgeSeq disabled_edges;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (isDisabledConstant(edge)
	  || isDisabledCondDefault(edge)
	  || isDisabledConstraint(edge)
	  || edge->isDisabledLoop()
	  || isDisabledPresetClr(edge))
	disabled_edges.push_back(edge);
    }
  }
  return disabled_edges;
}


EdgeSeq
Sta::disabledEdgesSorted()
{
  EdgeSeq disabled_edges = disabledEdges();
  sortEdges(&disabled_edges, network_, graph_);
  return disabled_edges;
}

bool
Sta::isDisabledConstraint(Edge *edge)
{
  Pin *from_pin = edge->from(graph_)->pin();
  Pin *to_pin = edge->to(graph_)->pin();
  const Instance *inst = network_->instance(from_pin);
  TimingArcSet *arc_set = edge->timingArcSet();
  return sdc_->isDisabled(from_pin)
    || sdc_->isDisabled(to_pin)
    || sdc_->isDisabled(inst, from_pin, to_pin, edge->role())
    || sdc_->isDisabled(edge)
    || sdc_->isDisabled(arc_set);
}

bool
Sta::isDisabledConstant(Edge *edge)
{
  sim_->ensureConstantsPropagated();
  const TimingRole *role = edge->role();
  Vertex *from_vertex = edge->from(graph_);
  Pin *from_pin = from_vertex->pin();
  Vertex *to_vertex = edge->to(graph_);
  Pin *to_pin = to_vertex->pin();
  const Instance *inst = network_->instance(from_pin);
  return sim_->logicZeroOne(from_vertex)
    || sim_->logicZeroOne(to_vertex)
    || (!role->isWire()
	&& (isCondDisabled(edge, inst, from_pin, to_pin, network_, sim_)
	    || isModeDisabled(edge, inst, network_, sim_)
	    || isTestDisabled(inst, from_pin, to_pin, network_, sim_)
	    || hasDisabledArcs(edge, graph_)));
}

static bool
hasDisabledArcs(Edge *edge,
		Graph *graph)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  for (TimingArc *arc : arc_set->arcs()) {
    if (!searchThru(edge, arc, graph))
      return true;
  }
  return false;
}

bool
Sta::isDisabledLoop(Edge *edge) const
{
  return levelize_->isDisabledLoop(edge);
}

bool
Sta::isDisabledCondDefault(Edge *edge)
{
  return sdc_->isDisabledCondDefault(edge);
}

PinSet
Sta::disabledConstantPins(Edge *edge)
{
  sim_->ensureConstantsPropagated();
  PinSet pins(network_);
  Vertex *from_vertex = edge->from(graph_);
  Pin *from_pin = from_vertex->pin();
  Vertex *to_vertex = edge->to(graph_);
  const Pin *to_pin = to_vertex->pin();
  if (sim_->logicZeroOne(from_vertex))
    pins.insert(from_pin);
  if (edge->role()->isWire()) {
    if (sim_->logicZeroOne(to_vertex))
      pins.insert(to_pin);
  }
  else {
    const Instance *inst = network_->instance(to_pin);
    bool is_disabled;
    FuncExpr *disable_cond;
    isCondDisabled(edge, inst, from_pin, to_pin, network_, sim_,
		   is_disabled, disable_cond);
    if (is_disabled)
      exprConstantPins(disable_cond, inst, pins);
    isModeDisabled(edge, inst, network_, sim_,
		   is_disabled, disable_cond);
    if (is_disabled)
      exprConstantPins(disable_cond, inst, pins);
    Pin *scan_enable;
    isTestDisabled(inst, from_pin, to_pin, network_, sim_,
		   is_disabled, scan_enable);
    if (is_disabled)
      pins.insert(scan_enable);
    if (hasDisabledArcs(edge, graph_)) {
      LibertyPort *to_port = network_->libertyPort(to_pin);
      if (to_port) {
	FuncExpr *func = to_port->function();
	if (func
	    && sim_->functionSense(inst, from_pin, to_pin) != edge->sense())
	  exprConstantPins(func, inst, pins);
      }
    }
  }
  return pins;
}

TimingSense
Sta::simTimingSense(Edge *edge)
{
  Pin *from_pin = edge->from(graph_)->pin();
  Pin *to_pin = edge->to(graph_)->pin();
  Instance *inst = network_->instance(from_pin);
  return sim_->functionSense(inst, from_pin, to_pin);
}

void
Sta::exprConstantPins(FuncExpr *expr,
                      const Instance *inst,
                      PinSet &pins)
{
  FuncExprPortIterator port_iter(expr);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    Pin *pin = network_->findPin(inst, port);
    if (pin) {
      LogicValue value = sim_->logicValue(pin);
      if (value != LogicValue::unknown)
	pins.insert(pin);
    }
  }
}

bool
Sta::isDisabledBidirectInstPath(Edge *edge) const
{
  return !sdc_->bidirectInstPathsEnabled()
    && edge->isBidirectInstPath();
}

bool
Sta::isDisabledBidirectNetPath(Edge *edge) const
{
  return !sdc_->bidirectNetPathsEnabled()
    && edge->isBidirectNetPath();
}

bool
Sta::isDisabledPresetClr(Edge *edge) const
{
  return !sdc_->presetClrArcsEnabled()
    && edge->role() == TimingRole::regSetClr();
}

void
Sta::disableClockGatingCheck(Instance *inst)
{
  sdc_->disableClockGatingCheck(inst);
  search_->endpointsInvalid();
}

void
Sta::disableClockGatingCheck(Pin *pin)
{
  sdc_->disableClockGatingCheck(pin);
  search_->endpointsInvalid();
}

void
Sta::removeDisableClockGatingCheck(Instance *inst)
{
  sdc_->removeDisableClockGatingCheck(inst);
  search_->endpointsInvalid();
}

void
Sta::removeDisableClockGatingCheck(Pin *pin)
{
  sdc_->removeDisableClockGatingCheck(pin);
  search_->endpointsInvalid();
}

void
Sta::setLogicValue(Pin *pin,
		   LogicValue value)
{
  sdc_->setLogicValue(pin, value);
  // Levelization respects constant disabled edges.
  levelize_->invalid();
  sim_->constantsInvalid();
  // Constants disable edges which isolate downstream vertices of the
  // graph from the delay calculator's BFS search.  This means that
  // simply invaldating the delays downstream from the constant pin
  // fails.  This could be more incremental if the graph delay
  // calculator searched thru disabled edges but ignored their
  // results.
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Sta::setCaseAnalysis(Pin *pin,
		     LogicValue value)
{
  sdc_->setCaseAnalysis(pin, value);
  // Levelization respects constant disabled edges.
  levelize_->invalid();
  sim_->constantsInvalid();
  // Constants disable edges which isolate downstream vertices of the
  // graph from the delay calculator's BFS search.  This means that
  // simply invaldating the delays downstream from the constant pin
  // fails.  This could be handled incrementally by invalidating delays
  // on the output of gates one level downstream.
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Sta::removeCaseAnalysis(Pin *pin)
{
  sdc_->removeCaseAnalysis(pin);
  // Levelization respects constant disabled edges.
  levelize_->invalid();
  sim_->constantsInvalid();
  // Constants disable edges which isolate downstream vertices of the
  // graph from the delay calculator's BFS search.  This means that
  // simply invaldating the delays downstream from the constant pin
  // fails.  This could be handled incrementally by invalidating delays
  // on the output of gates one level downstream.
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Sta::setInputDelay(const Pin *pin,
		   const RiseFallBoth *rf,
		   const Clock *clk,
		   const RiseFall *clk_rf,
		   const Pin *ref_pin,
		   bool source_latency_included,
		   bool network_latency_included,
		   const MinMaxAll *min_max,
		   bool add,
		   float delay)
{
  sdc_->setInputDelay(pin, rf, clk, clk_rf, ref_pin,
		      source_latency_included, network_latency_included,
		      min_max, add, delay);

  search_->arrivalInvalid(pin);
}

void 
Sta::removeInputDelay(const Pin *pin,
		      const RiseFallBoth *rf,
		      const Clock *clk,
		      const RiseFall *clk_rf,
		      const MinMaxAll *min_max)
{
  sdc_->removeInputDelay(pin, rf, clk, clk_rf, min_max);
  search_->arrivalInvalid(pin);
}

void
Sta::setOutputDelay(const Pin *pin,
		    const RiseFallBoth *rf,
		    const Clock *clk,
		    const RiseFall *clk_rf,
		    const Pin *ref_pin,
		    bool source_latency_included,
		    bool network_latency_included,
		    const MinMaxAll *min_max,
		    bool add,
		    float delay)
{
  sdc_->setOutputDelay(pin, rf, clk, clk_rf, ref_pin,
		       source_latency_included,network_latency_included,
		       min_max, add, delay);
  sdcChangedGraph();
  search_->requiredInvalid(pin);
}

void 
Sta::removeOutputDelay(const Pin *pin,
		       const RiseFallBoth *rf,
		       const Clock *clk,
		       const RiseFall *clk_rf,
		       const MinMaxAll *min_max)
{
  sdc_->removeOutputDelay(pin, rf, clk, clk_rf, min_max);
  sdcChangedGraph();
  search_->arrivalInvalid(pin);
}

void
Sta::makeFalsePath(ExceptionFrom *from,
		   ExceptionThruSeq *thrus,
		   ExceptionTo *to,
		   const MinMaxAll *min_max,
		   const char *comment)
{
  sdc_->makeFalsePath(from, thrus, to, min_max, comment);
  search_->arrivalsInvalid();
}

void
Sta::makeMulticyclePath(ExceptionFrom *from,
			ExceptionThruSeq *thrus,
			ExceptionTo *to,
			const MinMaxAll *min_max,
			bool use_end_clk,
			int path_multiplier,
			const char *comment)
{
  sdc_->makeMulticyclePath(from, thrus, to, min_max,
			   use_end_clk, path_multiplier,
			   comment);
  search_->arrivalsInvalid();
}

void
Sta::makePathDelay(ExceptionFrom *from,
		   ExceptionThruSeq *thrus,
		   ExceptionTo *to,
		   const MinMax *min_max,
		   bool ignore_clk_latency,
		   float delay,
		   const char *comment)
{
  sdc_->makePathDelay(from, thrus, to, min_max, 
		      ignore_clk_latency, delay,
		      comment);
  search_->endpointsInvalid();
  search_->arrivalsInvalid();
}

void
Sta::resetPath(ExceptionFrom *from,
	       ExceptionThruSeq *thrus,
	       ExceptionTo *to,
	       const MinMaxAll *min_max)
{
  sdc_->resetPath(from, thrus, to, min_max);
  search_->arrivalsInvalid();
}

void
Sta::makeGroupPath(const char *name,
		   bool is_default,
		   ExceptionFrom *from,
		   ExceptionThruSeq *thrus,
		   ExceptionTo *to,
		   const char *comment)
{
  sdc_->makeGroupPath(name, is_default, from, thrus, to, comment);
  search_->arrivalsInvalid();
}

bool
Sta::isGroupPathName(const char *group_name)
{
  return PathGroups::isGroupPathName(group_name)
    || sdc_->findClock(group_name)
    || sdc_->isGroupPathName(group_name);
}

ExceptionFrom *
Sta::makeExceptionFrom(PinSet *from_pins,
		       ClockSet *from_clks,
		       InstanceSet *from_insts,
		       const RiseFallBoth *from_rf)
{
  return sdc_->makeExceptionFrom(from_pins, from_clks, from_insts,
				 from_rf);
}

void
Sta::checkExceptionFromPins(ExceptionFrom *from,
			    const char *file,
			    int line) const
{
  if (from) {
    PinSet::ConstIterator pin_iter(from->pins());
    while (pin_iter.hasNext()) {
      const Pin *pin = pin_iter.next();
      if (exceptionFromInvalid(pin)) {
	if (line)
	  report_->fileWarn(267, file, line, "'%s' is not a valid start point.",
			    cmd_network_->pathName(pin));
	else
	  report_->warn(18, "'%s' is not a valid start point.",
			cmd_network_->pathName(pin));
      }
    }
  }
}

bool
Sta::exceptionFromInvalid(const Pin *pin) const
{
  Net *net = network_->net(pin);
  // Floating pins are invalid.
  return (net == nullptr
	  && !network_->isTopLevelPort(pin))
    || (net
	// Pins connected to power/ground are invalid.
	&& (network_->isPower(net)
            || network_->isGround(net)))
    || !((network_->isTopLevelPort(pin)
	  && network_->direction(pin)->isAnyInput())
	 || network_->isRegClkPin(pin)
	 || network_->isLatchData(pin)
         || network_->direction(pin)->isInternal());
}

void
Sta::deleteExceptionFrom(ExceptionFrom *from)
{
  delete from;
}

ExceptionThru *
Sta::makeExceptionThru(PinSet *pins,
		       NetSet *nets,
		       InstanceSet *insts,
		       const RiseFallBoth *rf)
{
  return sdc_->makeExceptionThru(pins, nets, insts, rf);
}

void
Sta::deleteExceptionThru(ExceptionThru *thru)
{
  delete thru;
}

ExceptionTo *
Sta::makeExceptionTo(PinSet *to_pins,
		     ClockSet *to_clks,
		     InstanceSet *to_insts,
		     const RiseFallBoth *rf,
 		     RiseFallBoth *end_rf)
{
  return sdc_->makeExceptionTo(to_pins, to_clks, to_insts, rf, end_rf);
}

void
Sta::deleteExceptionTo(ExceptionTo *to)
{
  delete to;
}

void
Sta::checkExceptionToPins(ExceptionTo *to,
			  const char *file,
			  int line) const
{
  if (to) {
    PinSet::Iterator pin_iter(to->pins());
    while (pin_iter.hasNext()) {
      const Pin *pin = pin_iter.next();
      if (sdc_->exceptionToInvalid(pin)) {
	if (line)
	  report_->fileWarn(266, file, line, "'%s' is not a valid endpoint.",
			    cmd_network_->pathName(pin));
	else
	  report_->warn(17, "'%s' is not a valid endpoint.",
			cmd_network_->pathName(pin));
      }
    }
  }
}

void
Sta::removeConstraints()
{
  levelize_->invalid();
  graph_delay_calc_->clear();
  search_->clear();
  sim_->constantsInvalid();
  if (graph_)
    sdc_->removeGraphAnnotations();
  sdc_->clear();
  clk_network_->clear();
}

void
Sta::constraintsChanged()
{
  levelize_->invalid();
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
  sim_->constantsInvalid();
}

void
Sta::writeSdc(const char *filename,
	      bool leaf,
	      bool native,
	      int digits,
              bool gzip,
	      bool no_timestamp)
{
  sta::writeSdc(network_->topInstance(), filename, "write_sdc",
		leaf, native, digits, gzip, no_timestamp, sdc_);
}

////////////////////////////////////////////////////////////////

CheckErrorSeq &
Sta::checkTiming(bool no_input_delay,
		 bool no_output_delay,
		 bool reg_multiple_clks,
		 bool reg_no_clks,
		 bool unconstrained_endpoints,
		 bool loops,
		 bool generated_clks)
{
  searchPreamble();
  if (unconstrained_endpoints)
    // Only need non-clock arrivals for unconstrained_endpoints.
    search_->findAllArrivals();
  else
    search_->findClkArrivals();
  if (check_timing_ == nullptr)
    makeCheckTiming();
  return check_timing_->check(no_input_delay, no_output_delay,
			      reg_multiple_clks, reg_no_clks,
			      unconstrained_endpoints,
			      loops, generated_clks);
}

bool
Sta::crprEnabled() const
{
  return sdc_->crprEnabled();
}

void
Sta::setCrprEnabled(bool enabled)
{
  // Pessimism is only relevant for on_chip_variation analysis.
  if (sdc_->analysisType() == AnalysisType::ocv
      && enabled != sdc_->crprEnabled())
    search_->arrivalsInvalid();
  sdc_->setCrprEnabled(enabled);
}

CrprMode
Sta::crprMode() const
{
  return sdc_->crprMode();
}

void
Sta::setCrprMode(CrprMode mode)
{
  // Pessimism is only relevant for on_chip_variation analysis.
  if (sdc_->analysisType() == AnalysisType::ocv
      && sdc_->crprEnabled()
      && sdc_->crprMode() != mode)
    search_->arrivalsInvalid();
  sdc_->setCrprMode(mode);
}

bool
Sta::pocvEnabled() const
{
  return pocv_enabled_;
}

void
Sta::setPocvEnabled(bool enabled)
{
  if (enabled != pocv_enabled_) {
    graph_delay_calc_->delaysInvalid();
    search_->arrivalsInvalid();
  }
  pocv_enabled_ = enabled;
  updateComponentsState();
}

void
Sta::setSigmaFactor(float factor)
{
  if (!fuzzyEqual(factor, sigma_factor_)) {
    sigma_factor_ = factor;
    search_->arrivalsInvalid();
    updateComponentsState();
  }
}

bool
Sta::propagateGatedClockEnable() const
{
  return sdc_->propagateGatedClockEnable();
}

void
Sta::setPropagateGatedClockEnable(bool enable)
{
  if (sdc_->propagateGatedClockEnable() != enable)
    search_->arrivalsInvalid();
  sdc_->setPropagateGatedClockEnable(enable);
}

bool
Sta::presetClrArcsEnabled() const
{
  return sdc_->presetClrArcsEnabled();
}

void
Sta::setPresetClrArcsEnabled(bool enable)
{
  if (sdc_->presetClrArcsEnabled() != enable) {
    levelize_->invalid();
    graph_delay_calc_->delaysInvalid();
    search_->arrivalsInvalid();
  }
  sdc_->setPresetClrArcsEnabled(enable);
}

bool
Sta::condDefaultArcsEnabled() const
{
  return sdc_->condDefaultArcsEnabled();
}

void
Sta::setCondDefaultArcsEnabled(bool enabled)
{
  if (sdc_->condDefaultArcsEnabled() != enabled) {
    graph_delay_calc_->delaysInvalid();
    search_->arrivalsInvalid();
    sdc_->setCondDefaultArcsEnabled(enabled);
  }
}

bool
Sta::bidirectInstPathsEnabled() const
{
  return sdc_->bidirectInstPathsEnabled();
}

void
Sta::setBidirectInstPathsEnabled(bool enabled)
{
  if (sdc_->bidirectInstPathsEnabled() != enabled) {
    levelize_->invalid();
    graph_delay_calc_->delaysInvalid();
    search_->arrivalsInvalid();
    sdc_->setBidirectInstPathsEnabled(enabled);
  }
}

bool
Sta::bidirectNetPathsEnabled() const
{
  return sdc_->bidirectNetPathsEnabled();
}

void
Sta::setBidirectNetPathsEnabled(bool enabled)
{
  if (sdc_->bidirectNetPathsEnabled() != enabled) {
    graph_delay_calc_->delaysInvalid();
    search_->arrivalsInvalid();
    sdc_->setBidirectNetPathsEnabled(enabled);
  }
}

bool
Sta::recoveryRemovalChecksEnabled() const
{
  return sdc_->recoveryRemovalChecksEnabled();
} 

void
Sta::setRecoveryRemovalChecksEnabled(bool enabled)
{
  if (sdc_->recoveryRemovalChecksEnabled() != enabled) {
    search_->arrivalsInvalid();
    sdc_->setRecoveryRemovalChecksEnabled(enabled);
  }
}

bool
Sta::gatedClkChecksEnabled() const
{
  return sdc_->gatedClkChecksEnabled();
}

void
Sta::setGatedClkChecksEnabled(bool enabled)
{
  if (sdc_->gatedClkChecksEnabled() != enabled) {
    search_->arrivalsInvalid();
    sdc_->setGatedClkChecksEnabled(enabled);
  }
}

bool
Sta::dynamicLoopBreaking() const
{
  return sdc_->dynamicLoopBreaking();
}

void
Sta::setDynamicLoopBreaking(bool enable)
{
  if (sdc_->dynamicLoopBreaking() != enable) {
    sdc_->setDynamicLoopBreaking(enable);
    search_->arrivalsInvalid();
  }
}

bool
Sta::useDefaultArrivalClock() const
{
  return sdc_->useDefaultArrivalClock();
}

void
Sta::setUseDefaultArrivalClock(bool enable)
{
  if (sdc_->useDefaultArrivalClock() != enable) {
    sdc_->setUseDefaultArrivalClock(enable);
    search_->arrivalsInvalid();
  }
}

bool
Sta::propagateAllClocks() const
{
  return sdc_->propagateAllClocks();
}

void
Sta::setPropagateAllClocks(bool prop)
{
  sdc_->setPropagateAllClocks(prop);
}

bool
Sta::clkThruTristateEnabled() const
{
  return sdc_->clkThruTristateEnabled();
}

void
Sta::setClkThruTristateEnabled(bool enable)
{
  if (enable != sdc_->clkThruTristateEnabled()) {
    search_->arrivalsInvalid();
    sdc_->setClkThruTristateEnabled(enable);
  }
}

////////////////////////////////////////////////////////////////

Corner *
Sta::findCorner(const char *corner_name)
{
  return corners_->findCorner(corner_name);
}

bool
Sta::multiCorner()
{
  return corners_->multiCorner();
}

// Init one corner named "default".
void
Sta::makeCorners()
{
  corners_ = new Corners(this);
  StringSet corner_names;
  corner_names.insert("default");
  corners_->makeCorners(&corner_names);
  cmd_corner_ = corners_->findCorner(0);
  sdc_->makeCornersAfter(corners_);
}

void
Sta::makeCorners(StringSet *corner_names)
{
  if (corner_names->size() > corner_count_max)
    report_->error(374, "maximum corner count exceeded");
  sdc_->makeCornersBefore();
  parasitics_->deleteParasitics();
  corners_->makeCorners(corner_names);
  makeParasiticAnalysisPts();
  cmd_corner_ = corners_->findCorner(0);
  updateComponentsState();
  sdc_->makeCornersAfter(corners_);
}

Corner *
Sta::cmdCorner() const
{
  return cmd_corner_;
}

void
Sta::setCmdCorner(Corner *corner)
{
  cmd_corner_ = corner;
}

////////////////////////////////////////////////////////////////

// from/thrus/to are owned and deleted by Search.
// Returned sequence is owned by the caller.
// PathEnds are owned by Search PathGroups and deleted on next call.
PathEndSeq
Sta::findPathEnds(ExceptionFrom *from,
		  ExceptionThruSeq *thrus,
		  ExceptionTo *to,
		  bool unconstrained,
		  const Corner *corner,
		  const MinMaxAll *min_max,
		  int group_count,
		  int endpoint_count,
		  bool unique_pins,
		  float slack_min,
		  float slack_max,
		  bool sort_by_slack,
		  PathGroupNameSet *group_names,
		  bool setup,
		  bool hold,
		  bool recovery,
		  bool removal,
		  bool clk_gating_setup,
		  bool clk_gating_hold)
{
  searchPreamble();
  return search_->findPathEnds(from, thrus, to, unconstrained,
			       corner, min_max, group_count, endpoint_count,
			       unique_pins, slack_min, slack_max,
			       sort_by_slack, group_names,
			       setup, hold,
			       recovery, removal,
			       clk_gating_setup, clk_gating_hold);
}

////////////////////////////////////////////////////////////////

// Overall flow:
//  make graph
//  propagate constants
//  levelize
//  delay calculation
//  update generated clocks
//  find arrivals

void
Sta::searchPreamble()
{
  findDelays();
  updateGeneratedClks();
  sdc_->searchPreamble();
  search_->deleteFilteredArrivals();
}

void
Sta::setReportPathFormat(ReportPathFormat format)
{
  report_path_->setPathFormat(format);
}

void
Sta::setReportPathFieldOrder(StringSeq *field_names)
{
  report_path_->setReportFieldOrder(field_names);
}

void
Sta::setReportPathFields(bool report_input_pin,
			 bool report_net,
			 bool report_cap,
			 bool report_slew)
{
  report_path_->setReportFields(report_input_pin, report_net, report_cap,
				report_slew);
}

ReportField *
Sta::findReportPathField(const char *name)
{
  return report_path_->findField(name);
}

void
Sta::setReportPathDigits(int digits)
{
  report_path_->setDigits(digits);
}

void
Sta::setReportPathNoSplit(bool no_split)
{
  report_path_->setNoSplit(no_split);
}

void
Sta::setReportPathSigmas(bool report_sigmas)
{
  report_path_->setReportSigmas(report_sigmas);
}

void
Sta::reportPathEnds(PathEndSeq *ends)
{
  report_path_->reportPathEnds(ends);
}

void
Sta::reportPathEndHeader()
{
  report_path_->reportPathEndHeader();
}

void
Sta::reportPathEndFooter()
{
  report_path_->reportPathEndFooter();
}

void
Sta::reportPathEnd(PathEnd *end)
{
  report_path_->reportPathEnd(end);
}

void
Sta::reportPathEnd(PathEnd *end,
		   PathEnd *prev_end)
{
  report_path_->reportPathEnd(end, prev_end);
}

void
Sta::reportPath(Path *path)
{
  report_path_->reportPath(path);
}

void
Sta::updateTiming(bool full)
{
  searchPreamble();
  if (full)
    search_->arrivalsInvalid();
  search_->findAllArrivals();
}

////////////////////////////////////////////////////////////////

void
Sta::reportClkSkew(ClockSet *clks,
		   const Corner *corner,
		   const SetupHold *setup_hold,
		   int digits)
{
  clkSkewPreamble();
  clk_skews_->reportClkSkew(clks, corner, setup_hold, digits);
}

float
Sta::findWorstClkSkew(const SetupHold *setup_hold)
{
  clkSkewPreamble();
  return clk_skews_->findWorstClkSkew(cmd_corner_, setup_hold);
}

void
Sta::clkSkewPreamble()
{
  ensureClkArrivals();
  if (clk_skews_ == nullptr)
    clk_skews_ = new ClkSkews(this);
}

////////////////////////////////////////////////////////////////

void
Sta::delaysInvalid()
{
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Sta::arrivalsInvalid()
{
  search_->arrivalsInvalid();
}

void
Sta::ensureClkArrivals()
{
  searchPreamble();
  search_->findClkArrivals();
}

////////////////////////////////////////////////////////////////

void
Sta::visitStartpoints(VertexVisitor *visitor)
{
  ensureGraph();
  search_->visitStartpoints(visitor);
}

void
Sta::visitEndpoints(VertexVisitor *visitor)
{
  ensureGraph();
  search_->visitEndpoints(visitor);
}

VertexSet *
Sta::endpoints()
{
  ensureGraph();
  return search_->endpoints();
}

int
Sta::endpointViolationCount(const MinMax *min_max)
{
  int violations = 0;
  for (Vertex *end : *search_->endpoints()) {
    if (delayLess(vertexSlack(end, min_max), 0.0, this))
      violations++;
  }
  return violations;
}

PinSet
Sta::findGroupPathPins(const char *group_path_name)
{
  if (!(search_->havePathGroups()
        && search_->arrivalsValid())) {
    PathEndSeq path_ends = findPathEnds(// from, thrus, to, unconstrained
                                        nullptr, nullptr, nullptr, false,
                                        // corner, min_max, 
                                        nullptr, MinMaxAll::max(),
                                        // group_count, endpoint_count, unique_pins
                                        1, 1, false,
                                        -INF, INF, // slack_min, slack_max,
                                        false, // sort_by_slack
                                        nullptr, // group_names
                                        // setup, hold, recovery, removal, 
                                        true, true, true, true,
                                        // clk_gating_setup, clk_gating_hold
                                        true, true);
  }

  PathGroup *path_group = search_->findPathGroup(group_path_name,
						 MinMax::max());
  PinSet pins(network_);
  VertexPinCollector visitor(pins);
  visitPathGroupVertices(path_group, &visitor, this);
  return pins;
}

////////////////////////////////////////////////////////////////

void
Sta::findRequireds()
{
  searchPreamble();
  search_->findAllArrivals();
  search_->findRequireds();
}

////////////////////////////////////////////////////////////////

VertexPathIterator *
Sta::vertexPathIterator(Vertex *vertex,
			const RiseFall *rf,
			const PathAnalysisPt *path_ap)
{
  return new VertexPathIterator(vertex, rf, path_ap, this);
}

VertexPathIterator *
Sta::vertexPathIterator(Vertex *vertex,
			const RiseFall *rf,
			const MinMax *min_max)
{
  return new VertexPathIterator(vertex, rf, min_max, this);
}

PathRef
Sta::vertexWorstArrivalPath(Vertex *vertex,
			    const MinMax *min_max)
{
  return vertexWorstArrivalPath(vertex, nullptr, min_max);
}

PathRef
Sta::vertexWorstArrivalPath(Vertex *vertex,
			    const RiseFall *rf,
			    const MinMax *min_max)
{
  PathRef worst_path;
  Arrival worst_arrival = min_max->initValue();
  VertexPathIterator path_iter(vertex, rf, min_max, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    Arrival arrival = path->arrival(this);
    if (!path->tag(this)->isGenClkSrcPath()
	&& delayGreater(arrival, worst_arrival, min_max, this)) {
      worst_arrival = arrival;
      worst_path.init(path);
    }
  }
  return worst_path;
}

PathRef
Sta::vertexWorstRequiredPath(Vertex *vertex,
                             const MinMax *min_max)
{
  return vertexWorstRequiredPath(vertex, nullptr, min_max);
}

PathRef
Sta::vertexWorstRequiredPath(Vertex *vertex,
                             const RiseFall *rf,
                             const MinMax *min_max)
{
  PathRef worst_path;
  const MinMax *req_min_max = min_max->opposite();
  Arrival worst_req = req_min_max->initValue();
  VertexPathIterator path_iter(vertex, rf, min_max, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    const Required path_req = path->required(this);
    if (!path->tag(this)->isGenClkSrcPath()
	&& delayGreater(path_req, worst_req, req_min_max, this)) {
      worst_req = path_req;
      worst_path.init(path);
    }
  }
  return worst_path;
}

PathRef
Sta::vertexWorstSlackPath(Vertex *vertex,
			  const RiseFall *rf,
			  const MinMax *min_max)
{
  PathRef worst_path;
  Slack min_slack = MinMax::min()->initValue();
  VertexPathIterator path_iter(vertex, rf, min_max, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    Slack slack = path->slack(this);
    if (!path->tag(this)->isGenClkSrcPath()
	&& delayLess(slack, min_slack, this)) {
      min_slack = slack;
      worst_path.init(path);
    }
  }
  return worst_path;
}

PathRef
Sta::vertexWorstSlackPath(Vertex *vertex,
			  const MinMax *min_max)

{
  return vertexWorstSlackPath(vertex, nullptr, min_max);
}

Arrival
Sta::pinArrival(const Pin *pin,
                const RiseFall *rf,
                const MinMax *min_max)
{
  Vertex *vertex, *bidirect_vertex;
  graph_->pinVertices(pin, vertex, bidirect_vertex);
  Arrival arrival;
  if (vertex)
    arrival = vertexArrival(vertex, rf, clk_edge_wildcard, nullptr, min_max);
  if (bidirect_vertex) {
    Arrival arrival1 = vertexArrival(bidirect_vertex, rf, clk_edge_wildcard,
                                     nullptr, min_max);
    if (delayLess(arrival1, arrival, this))
      arrival = arrival1;
  }
  return arrival;
}

Arrival
Sta::vertexArrival(Vertex *vertex,
                   const MinMax *min_max)
{
  return vertexArrival(vertex, nullptr, nullptr, nullptr, min_max);
}

Arrival
Sta::vertexArrival(Vertex *vertex,
		   const RiseFall *rf,
		   const PathAnalysisPt *path_ap)
{
  return vertexArrival(vertex, rf, clk_edge_wildcard, path_ap, nullptr);
}

Arrival
Sta::vertexArrival(Vertex *vertex,
		   const RiseFall *rf,
		   const ClockEdge *clk_edge,
		   const PathAnalysisPt *path_ap,
                   const MinMax *min_max)
{
  searchPreamble();
  search_->findArrivals(vertex->level());
  if (min_max == nullptr)
    min_max = path_ap->pathMinMax();
  Arrival arrival = min_max->initValue();
  VertexPathIterator path_iter(vertex, rf, path_ap, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    const Arrival &path_arrival = path->arrival(this);
    ClkInfo *clk_info = path->clkInfo(search_);
    if ((clk_edge == clk_edge_wildcard
	 || clk_info->clkEdge() == clk_edge)
	&& !clk_info->isGenClkSrcPath()
	&& delayGreater(path->arrival(this), arrival, min_max, this))
      arrival = path_arrival;
  }
  return arrival;
}

Required
Sta::vertexRequired(Vertex *vertex,
		    const MinMax *min_max)
{
  return vertexRequired(vertex, nullptr, clk_edge_wildcard, nullptr, min_max);
}

Required
Sta::vertexRequired(Vertex *vertex,
		    const RiseFall *rf,
		    const MinMax *min_max)
{
  return vertexRequired(vertex, rf, clk_edge_wildcard, nullptr, min_max);
}

Required
Sta::vertexRequired(Vertex *vertex,
		    const RiseFall *rf,
		    const PathAnalysisPt *path_ap)
{
  return vertexRequired(vertex, rf, clk_edge_wildcard, path_ap, nullptr);
}

Required
Sta::vertexRequired(Vertex *vertex,
		    const RiseFall *rf,
		    const ClockEdge *clk_edge,
		    const PathAnalysisPt *path_ap)
{
  return vertexRequired(vertex, rf, clk_edge, path_ap, nullptr);
}

Required
Sta::vertexRequired(Vertex *vertex,
		    const RiseFall *rf,
		    const ClockEdge *clk_edge,
		    const PathAnalysisPt *path_ap,
		    const MinMax *min_max)
{
  findRequired(vertex);
  const MinMax *req_min_max = min_max
    ? min_max->opposite()
    : path_ap->pathMinMax()->opposite();
  Required required = req_min_max->initValue();
  VertexPathIterator path_iter(vertex, rf, path_ap, min_max, this);
  while (path_iter.hasNext()) {
    const Path *path = path_iter.next();
    const Required path_required = path->required(this);
    if ((clk_edge == clk_edge_wildcard
	 || path->clkEdge(search_) == clk_edge)
	&& delayGreater(path_required, required, req_min_max, this))
      required = path_required;
  }
  return required;
}

Slack
Sta::netSlack(const Net *net,
	      const MinMax *min_max)
{
  ensureGraph();
  Slack slack = MinMax::min()->initValue();
  NetPinIterator *pin_iter = network_->pinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isLoad(pin)) {
      Vertex *vertex = graph_->pinLoadVertex(pin);
      Slack pin_slack = vertexSlack(vertex, min_max);
      if (delayLess(pin_slack, slack, this))
	slack = pin_slack;
    }
  }
  return slack;
}

Slack
Sta::pinSlack(const Pin *pin,
	      const MinMax *min_max)
{
  ensureGraph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slack slack = MinMax::min()->initValue();
  if (vertex)
    slack = vertexSlack(vertex, min_max);
  if (bidirect_drvr_vertex) {
    Slack slack1 = vertexSlack(bidirect_drvr_vertex, min_max);
    if (delayLess(slack1, slack, this))
      slack = slack1;
  }
  return slack;
}

Slack
Sta::pinSlack(const Pin *pin,
	      const RiseFall *rf,
	      const MinMax *min_max)
{
  ensureGraph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slack slack = MinMax::min()->initValue();
  if (vertex)
    slack = vertexSlack(vertex, rf, min_max);
  if (bidirect_drvr_vertex) {
    Slack slack1 = vertexSlack(bidirect_drvr_vertex, rf, min_max);
    if (delayLess(slack1, slack, this))
      slack = slack1;
  }
  return slack;
}

Slack
Sta::vertexSlack(Vertex *vertex,
		 const MinMax *min_max)
{
  findRequired(vertex);
  MinMax *min = MinMax::min();
  Slack slack = min->initValue();
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    if (path->minMax(this) == min_max) {
      Slack path_slack = path->slack(this);
      if (delayLess(path_slack, slack, this))
	slack = path_slack;
    }
  }
  return slack;
}

Slack
Sta::vertexSlack(Vertex *vertex,
		 const RiseFall *rf,
		 const MinMax *min_max)
{
  findRequired(vertex);
  Slack slack = MinMax::min()->initValue();
  VertexPathIterator path_iter(vertex, rf, min_max, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Slack path_slack = path->slack(this);
    if (delayLess(path_slack, slack, this))
      slack = path_slack;
  }
  return slack;
}

Slack
Sta::vertexSlack(Vertex *vertex,
		 const RiseFall *rf,
		 const PathAnalysisPt *path_ap)
{
  findRequired(vertex);
  return vertexSlack1(vertex, rf, clk_edge_wildcard, path_ap);
}

Slack
Sta::vertexSlack(Vertex *vertex,
		 const RiseFall *rf,
		 const ClockEdge *clk_edge,
		 const PathAnalysisPt *path_ap)
{
  findRequired(vertex);
  return vertexSlack1(vertex, rf, clk_edge, path_ap);
}

Slack
Sta::vertexSlack1(Vertex *vertex,
		  const RiseFall *rf,
		  const ClockEdge *clk_edge,
		  const PathAnalysisPt *path_ap)
{
  MinMax *min = MinMax::min();
  Slack slack = min->initValue();
  VertexPathIterator path_iter(vertex, rf, path_ap, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Slack path_slack = path->slack(this);
    if ((clk_edge == clk_edge_wildcard
	 || path->clkEdge(search_) == clk_edge)
	&& delayLess(path_slack, slack, this))
      slack = path_slack;
  }
  return slack;
}

void
Sta::vertexSlacks(Vertex *vertex,
		  Slack (&slacks)[RiseFall::index_count][MinMax::index_count])
{
  findRequired(vertex);
  for(int rf_index : RiseFall::rangeIndex()) {
    for(MinMax *min_max : MinMax::range()) {
      slacks[rf_index][min_max->index()] = MinMax::min()->initValue();
    }
  }
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Slack path_slack = path->slack(this);
    int rf_index = path->rfIndex(this);
    int mm_index = path->minMax(this)->index();
    if (delayLess(path_slack, slacks[rf_index][mm_index], this))
      slacks[rf_index][mm_index] = path_slack;
  }
}

////////////////////////////////////////////////////////////////

class MinPeriodEndVisitor : public PathEndVisitor
{
public:
  MinPeriodEndVisitor(const Clock *clk,
                      bool include_port_paths,
                      StaState *sta);
  MinPeriodEndVisitor(const MinPeriodEndVisitor &) = default;
  virtual PathEndVisitor *copy() const;
  virtual void visit(PathEnd *path_end);
  float minPeriod() const { return min_period_; }

private:
  bool pathIsFromInputPort(PathEnd *path_end);

  const Clock *clk_;
  bool include_port_paths_;
  StaState *sta_;
  float min_period_;
};

MinPeriodEndVisitor::MinPeriodEndVisitor(const Clock *clk,
                                         bool include_port_paths,
                                         StaState *sta) :
  clk_(clk),
  include_port_paths_(include_port_paths),
  sta_(sta),
  min_period_(0)
{
}

PathEndVisitor *
MinPeriodEndVisitor::copy() const
{
  return new MinPeriodEndVisitor(*this);
}

void
MinPeriodEndVisitor::visit(PathEnd *path_end)
{
  Network *network = sta_->network();
  Path *path = path_end->path();
  const ClockEdge *src_edge = path_end->sourceClkEdge(sta_);
  const ClockEdge *tgt_edge = path_end->targetClkEdge(sta_);
  PathEnd::Type end_type = path_end->type();
  if ((end_type == PathEnd::Type::check
       || end_type == PathEnd::Type::output_delay)
      && path->minMax(sta_) == MinMax::max()
      && src_edge->clock() == clk_
      && tgt_edge->clock() == clk_
      // Only consider rise/rise and fall/fall paths.
      && src_edge->transition() == tgt_edge->transition()
      && path_end->multiCyclePath() == nullptr
      && (include_port_paths_
          || !(network->isTopLevelPort(path->pin(sta_))
               || pathIsFromInputPort(path_end)))) {
    Slack slack = path_end->slack(sta_);
    float period = clk_->period() - delayAsFloat(slack);
    min_period_ = max(min_period_, period);
  }
}

bool
MinPeriodEndVisitor::pathIsFromInputPort(PathEnd *path_end)
{
  PathExpanded expanded(path_end->path(), sta_);
  PathRef *start = expanded.startPath();
  Graph *graph = sta_->graph();
  const Pin *first_pin = start->pin(graph);
  Network *network = sta_->network();
  return network->isTopLevelPort(first_pin);
}

float
Sta::findClkMinPeriod(const Clock *clk,
                      bool include_port_paths)
{
  searchPreamble();
  search_->findArrivals();
  VisitPathEnds visit_ends(this);
  MinPeriodEndVisitor min_period_visitor(clk, include_port_paths, this);
  for (Vertex *vertex : *search_->endpoints()) {
    findRequired(vertex);
    visit_ends.visitPathEnds(vertex, &min_period_visitor);
  }
  return min_period_visitor.minPeriod();
}

////////////////////////////////////////////////////////////////

void
Sta::findRequired(Vertex *vertex)
{
  searchPreamble();
  search_->findAllArrivals();
  search_->findRequireds(vertex->level());
  if (sdc_->crprEnabled()
      && search_->crprPathPruningEnabled()
      && !search_->crprApproxMissingRequireds()
      // Clocks invariably have requireds that are pruned but isn't
      // worth finding arrivals and requireds all over again for
      // the entire fanout of the clock.
      && !search_->isClock(vertex)
      && vertex->requiredsPruned()) {
    // Invalidate arrivals and requireds and disable
    // path pruning on fanout vertices with DFS.
    int fanout = 0;
    disableFanoutCrprPruning(vertex, fanout);
    debugPrint(debug_, "search", 1, "resurrect pruned required %s fanout %d",
               vertex->name(sdc_network_),
               fanout);
    // Find fanout arrivals and requireds with pruning disabled.
    search_->findArrivals();
    search_->findRequireds(vertex->level());
  }
}

void
Sta::disableFanoutCrprPruning(Vertex *vertex,
			      int &fanout)
{
  if (!vertex->crprPathPruningDisabled()) {
    search_->arrivalInvalid(vertex);
    search_->requiredInvalid(vertex);
    vertex->setCrprPathPruningDisabled(true);
    fanout++;
    SearchPred *pred = search_->searchAdj();
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      if (pred->searchThru(edge)
	  && pred->searchTo(to_vertex))
	disableFanoutCrprPruning(to_vertex, fanout);
    }
  }
}

Slack
Sta::totalNegativeSlack(const MinMax *min_max)
{
  searchPreamble();
  return search_->totalNegativeSlack(min_max);
}

Slack
Sta::totalNegativeSlack(const Corner *corner,
			const MinMax *min_max)
{
  searchPreamble();
  return search_->totalNegativeSlack(corner, min_max);
}

Slack
Sta::worstSlack(const MinMax *min_max)
{
  searchPreamble();
  Slack worst_slack;
  Vertex *worst_vertex;
  search_->worstSlack(min_max, worst_slack, worst_vertex);
  return worst_slack;
}

void
Sta::worstSlack(const MinMax *min_max,
		// Return values.
		Slack &worst_slack,
		Vertex *&worst_vertex)
{
  searchPreamble();
  search_->worstSlack(min_max, worst_slack, worst_vertex);
}

void
Sta::worstSlack(const Corner *corner,
		const MinMax *min_max,
		// Return values.
		Slack &worst_slack,
		Vertex *&worst_vertex)
{
  searchPreamble();
  return search_->worstSlack(corner, min_max, worst_slack, worst_vertex);
}

////////////////////////////////////////////////////////////////

string
Sta::reportDelayCalc(Edge *edge,
		     TimingArc *arc,
		     const Corner *corner,
		     const MinMax *min_max,
		     int digits)
{
  findDelays();
  return graph_delay_calc_->reportDelayCalc(edge, arc, corner, min_max, digits);
}

void
Sta::setArcDelayCalc(const char *delay_calc_name)
{
  delete arc_delay_calc_;
  arc_delay_calc_ = makeDelayCalc(delay_calc_name, sta_);
  // Update pointers to arc_delay_calc.
  updateComponentsState();
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

void
Sta::findDelays(Vertex *to_vertex)
{
  delayCalcPreamble();
  graph_delay_calc_->findDelays(to_vertex->level());
}

void
Sta::findDelays()
{
  delayCalcPreamble();
  graph_delay_calc_->findDelays(levelize_->maxLevel());
}

void
Sta::findDelays(Level level)
{
  delayCalcPreamble();
  graph_delay_calc_->findDelays(level);
}

void
Sta::delayCalcPreamble()
{
  ensureClkNetwork();
}

void
Sta::setIncrementalDelayTolerance(float tol)
{
  graph_delay_calc_->setIncrementalDelayTolerance(tol);
}

ArcDelay
Sta::arcDelay(Edge *edge,
	      TimingArc *arc,
	      const DcalcAnalysisPt *dcalc_ap)
{
  findDelays(edge->to(graph_));
  return graph_->arcDelay(edge, arc, dcalc_ap->index());
}

bool
Sta::arcDelayAnnotated(Edge *edge,
		       TimingArc *arc,
		       DcalcAnalysisPt *dcalc_ap)
{
  return graph_->arcDelayAnnotated(edge, arc, dcalc_ap->index());
}

void
Sta::setArcDelayAnnotated(Edge *edge,
			  TimingArc *arc,
			  DcalcAnalysisPt *dcalc_ap,
			  bool annotated)
{
  graph_->setArcDelayAnnotated(edge, arc, dcalc_ap->index(), annotated);
  Vertex *to = edge->to(graph_);
  search_->arrivalInvalid(to);
  search_->requiredInvalid(edge->from(graph_));
  if (!annotated)
    graph_delay_calc_->delayInvalid(to);
}

Slew
Sta::vertexSlew(Vertex *vertex,
		const RiseFall *rf,
                const Corner *corner,
                const MinMax *min_max)
{
  findDelays(vertex);
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  return graph_->slew(vertex, rf, dcalc_ap->index());
}

Slew
Sta::vertexSlew(Vertex *vertex,
		const RiseFall *rf,
		const DcalcAnalysisPt *dcalc_ap)
{
  findDelays(vertex);
  return graph_->slew(vertex, rf, dcalc_ap->index());
}

Slew
Sta::vertexSlew(Vertex *vertex,
		const RiseFall *rf,
		const MinMax *min_max)
{
  findDelays(vertex);
  Slew mm_slew = min_max->initValue();
  for (const DcalcAnalysisPt *dcalc_ap : corners_->dcalcAnalysisPts()) {
    Slew slew = graph_->slew(vertex, rf, dcalc_ap->index());
    if (delayGreater(slew, mm_slew, min_max, this))
      mm_slew = slew;
  }
  return mm_slew;
}

Slew
Sta::vertexSlew(Vertex *vertex,
                const MinMax *min_max)
{
  findDelays(vertex);
  Slew mm_slew = min_max->initValue();
  for (const DcalcAnalysisPt *dcalc_ap : corners_->dcalcAnalysisPts()) {
    for (const RiseFall *rf : RiseFall::range()) {
      Slew slew = graph_->slew(vertex, rf, dcalc_ap->index());
      if (delayGreater(slew, mm_slew, min_max, this))
        mm_slew = slew;
    }
  }
  return mm_slew;
}

////////////////////////////////////////////////////////////////

Graph *
Sta::ensureGraph()
{
  if (graph_ == nullptr && network_) {
    makeGraph();
    // Update pointers to graph.
    updateComponentsState();
  }
  return graph_;
}

void
Sta::makeGraph()
{
  graph_ = new Graph(this, 2, true, corners_->dcalcAnalysisPtCount());
  graph_->makeGraph();
}

void
Sta::ensureLevelized()
{
  ensureGraph();
  ensureGraphSdcAnnotated();
  // Need constant propagation before levelization to know edges that
  // are disabled by constants.
  sim_->ensureConstantsPropagated();
  levelize_->ensureLevelized();
}

void
Sta::updateGeneratedClks()
{
  if (update_genclks_) {
    ensureLevelized();
    bool gen_clk_changed = true;
    while (gen_clk_changed) {
      gen_clk_changed = false;
      for (Clock *clk : sdc_->clks()) {
	if (clk->isGenerated() && !clk->waveformValid()) {
	  search_->genclks()->ensureMaster(clk);
	  Clock *master_clk = clk->masterClk();
	  if (master_clk && master_clk->waveformValid()) {
	    clk->generate(master_clk);
	    gen_clk_changed = true;
	  }
	}
      }
    }
  }
  update_genclks_ = false;
}

Level
Sta::vertexLevel(Vertex *vertex)
{
  ensureLevelized();
  return vertex->level();
}

GraphLoopSeq *
Sta::graphLoops()
{
  ensureLevelized();
  return levelize_->loops();
}

PathAnalysisPt *
Sta::pathAnalysisPt(Path *path)
{
  return path->pathAnalysisPt(this);
}

DcalcAnalysisPt *
Sta::pathDcalcAnalysisPt(Path *path)
{
  return pathAnalysisPt(path)->dcalcAnalysisPt();
}

Vertex *
Sta::maxArrivalCountVertex() const
{
  Vertex *max_vertex = nullptr;
  int max_count = 0;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    int count = vertexArrivalCount(vertex);
    if (count > max_count) {
      max_count = count;
      max_vertex = vertex;
    }
  }
  return max_vertex;
}

int
Sta::vertexArrivalCount(Vertex  *vertex) const
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group)
    return tag_group->arrivalCount();
  else
    return 0;
}

int
Sta::arrivalCount() const
{
  int count = 0;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    count += vertexArrivalCount(vertex);
  }
  return count;
}

int
Sta::requiredCount() const
{
  int count = 0;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (vertex->hasRequireds())
      count += vertexArrivalCount(vertex);
  }
  return count;
}

TagIndex
Sta::tagCount() const
{
  return search_->tagCount();
}

TagGroupIndex
Sta::tagGroupCount() const
{
  return search_->tagGroupCount();
}

int
Sta::clkInfoCount() const
{
  return search_->clkInfoCount();
}

void
Sta::setArcDelay(Edge *edge,
		 TimingArc *arc,
		 const Corner *corner,
		 const MinMaxAll *min_max,
		 ArcDelay delay)
{
  for (MinMax *mm : min_max->range()) {
    const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(mm);
    DcalcAPIndex ap_index = dcalc_ap->index();
    graph_->setArcDelay(edge, arc, ap_index, delay);
    // Don't let delay calculation clobber the value.
    graph_->setArcDelayAnnotated(edge, arc, ap_index, true);
  }
  if (edge->role()->isTimingCheck())
    search_->requiredInvalid(edge->to(graph_));
  else {
    search_->arrivalInvalid(edge->to(graph_));
    search_->requiredInvalid(edge->from(graph_));
  }
}

void
Sta::setAnnotatedSlew(Vertex *vertex,
		      const Corner *corner,
		      const MinMaxAll *min_max,
		      const RiseFallBoth *rf,
		      float slew)
{
  for (MinMax *mm : min_max->range()) {
    const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(mm);
    DcalcAPIndex ap_index = dcalc_ap->index();
    for (RiseFall *rf1 : rf->range()) {
      graph_->setSlew(vertex, rf1, ap_index, slew);
      // Don't let delay calculation clobber the value.
      vertex->setSlewAnnotated(true, rf1, ap_index);
    }
  }
  graph_delay_calc_->delayInvalid(vertex);
}

void
Sta::writeSdf(const char *filename,
	      Corner *corner,
	      char divider,
	      bool include_typ,
              int digits,
	      bool gzip,
	      bool no_timestamp,
	      bool no_version)
{
  findDelays();
  sta::writeSdf(filename, corner, divider, include_typ, digits, gzip,
                no_timestamp, no_version, this);
}

void
Sta::removeDelaySlewAnnotations()
{
  graph_->removeDelaySlewAnnotations();
  graph_delay_calc_->delaysInvalid();
}

LogicValue
Sta::simLogicValue(const Pin *pin)
{
  ensureGraph();
  sim_->ensureConstantsPropagated();
  return sim_->logicValue(pin);
}

void
Sta::findLogicConstants()
{
  ensureGraph();
  sim_->findLogicConstants();
}

void
Sta::clearLogicConstants()
{
  sim_->clear();
}

void
Sta::setPortExtPinCap(const Port *port,
		      const RiseFallBoth *rf,
                      const Corner *corner,
		      const MinMaxAll *min_max,
		      float cap)
{
  for (RiseFall *rf1 : rf->range()) {
    for (MinMax *mm : min_max->range()) {
      if (corner == nullptr) {
        for (Corner *corner : *corners_)
          sdc_->setPortExtPinCap(port, rf1, corner, mm, cap);
      }
      else
        sdc_->setPortExtPinCap(port, rf1, corner, mm, cap);
    }
  }
  delaysInvalidFromFanin(port);
}

void
Sta::portExtCaps(const Port *port,
                 const Corner *corner,
                 const MinMax *min_max,
                 float &pin_cap,
                 float &wire_cap,
                 int &fanout)
{
  bool pin_exists = false;
  bool wire_exists = false;
  bool fanout_exists = false;
  pin_cap = min_max->initValue();
  wire_cap = min_max->initValue();
  fanout = min_max->initValue();
  for (RiseFall *rf : RiseFall::range()) {
    float pin_cap1, wire_cap1;
    int fanout1;
    bool pin_exists1, wire_exists1, fanout_exists1;
    sdc_->portExtCap(port, rf, corner, min_max,
                     pin_cap1, pin_exists1,
                     wire_cap1, wire_exists1,
                     fanout1, fanout_exists1);
    if (pin_exists1) {
      pin_cap = min_max->minMax(pin_cap, pin_cap1);
      pin_exists = true;
    }
    if (wire_exists1) {
      wire_cap = min_max->minMax(wire_cap, wire_cap1);
      wire_exists = true;
    }
    if (fanout_exists1) {
      fanout = min_max->minMax(fanout, fanout1);
      fanout_exists = true;
    }
  }
  if (!pin_exists)
    pin_cap = 0.0;
  if (!wire_exists)
    wire_cap = 0.0;
  if (!fanout_exists)
    fanout = 0;
}

void
Sta::setPortExtWireCap(const Port *port,
		       bool subtract_pin_cap,
		       const RiseFallBoth *rf,
                       const Corner *corner,
		       const MinMaxAll *min_max,
		       float cap)
{
  for (RiseFall *rf1 : rf->range()) {
    for (MinMax *mm : min_max->range()) {
      if (corner == nullptr) {
        for (Corner *corner : *corners_)
          sdc_->setPortExtWireCap(port, subtract_pin_cap, rf1, corner, mm, cap);
      }
      else
        sdc_->setPortExtWireCap(port, subtract_pin_cap, rf1, corner, mm, cap);
    }
  }
  delaysInvalidFromFanin(port);
}

void
Sta::removeNetLoadCaps() const
{
  sdc_->removeNetLoadCaps();
  graph_delay_calc_->delaysInvalid();
}

void
Sta::setPortExtFanout(const Port *port,
		      int fanout,
                      const Corner *corner,
		      const MinMaxAll *min_max)
{
  for (MinMax *mm : min_max->range()) {
    if (corner == nullptr) {
      for (Corner *corner : *corners_)
        sdc_->setPortExtFanout(port, corner, mm, fanout);
    }
    else
      sdc_->setPortExtFanout(port, corner, mm, fanout);
  }
  delaysInvalidFromFanin(port);
}

void
Sta::setNetWireCap(const Net *net,
		   bool subtract_pin_cap,
		   const Corner *corner,
		   const MinMaxAll *min_max,
		   float cap)
{
  for (MinMax *mm : min_max->range()) {
    if (corner == nullptr) {
      for (Corner *corner : *corners_)
        sdc_->setNetWireCap(net, subtract_pin_cap, corner, mm, cap);
    }
    else
      sdc_->setNetWireCap(net, subtract_pin_cap, corner, mm, cap);
  }
  delaysInvalidFromFanin(net);
}

void
Sta::connectedCap(const Pin *drvr_pin,
		  const RiseFall *rf,
		  const Corner *corner,
		  const MinMax *min_max,
		  float &pin_cap,
		  float &wire_cap) const
{
  pin_cap = 0.0;
  wire_cap = 0.0;
  bool cap_exists = false;
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  Parasitic *parasitic = arc_delay_calc_->findParasitic(drvr_pin, rf, dcalc_ap);
  float ap_pin_cap = 0.0;
  float ap_wire_cap = 0.0;
  graph_delay_calc_->loadCap(drvr_pin, parasitic, rf, dcalc_ap,
			     ap_pin_cap, ap_wire_cap);
  arc_delay_calc_->finishDrvrPin();
  if (!cap_exists
      || min_max->compare(ap_pin_cap, pin_cap)) {
    pin_cap = ap_pin_cap;
    wire_cap = ap_wire_cap;
    cap_exists = true;
  }
}

void
Sta::connectedCap(const Net *net,
		  Corner *corner,
		  const MinMax *min_max,
		  float &pin_cap,
		  float &wire_cap) const
{
  const Pin *drvr_pin = findNetParasiticDrvrPin(net);
  if (drvr_pin) {
    pin_cap = min_max->initValue();
    wire_cap = min_max->initValue();
    for (const Corner *corner : makeCornerSeq(corner)) {
      for (RiseFall *rf : RiseFall::range()) {
        float pin_cap1, wire_cap1;
        connectedCap(drvr_pin, rf, corner, min_max, pin_cap1, wire_cap1);
        pin_cap = min_max->minMax(pin_cap, pin_cap1);
        wire_cap = min_max->minMax(wire_cap, wire_cap1);
      }
    }
  }
  else {
    pin_cap = 0.0;
    wire_cap = 0.0;
  }
}

CornerSeq
Sta::makeCornerSeq(Corner *corner) const
{
  CornerSeq corners;
  if (corner)
    corners.push_back(corner);
  else
    corners = corners_->corners();
  return corners;
}

float
Sta::capacitance(const LibertyPort *port,
                 Corner *corner,
                 const MinMax *min_max)
{
  OperatingConditions *op_cond = operatingConditions(min_max);
  float cap = min_max->initValue();
  for (const Corner *corner : makeCornerSeq(corner)) {
    const LibertyPort *corner_port = port->cornerPort(corner, min_max);
    for (RiseFall *rf : RiseFall::range())
      cap = min_max->minMax(cap, corner_port->capacitance(rf, min_max, op_cond, op_cond));
  }
  return cap;
}

// Look for a driver to find a parasitic if the net has one.
// Settle for a load pin if there are no drivers.
const Pin *
Sta::findNetParasiticDrvrPin(const Net *net) const
{
  const Pin *load_pin = nullptr;
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isDriver(pin)) {
      delete pin_iter;
      return pin;
    }
    if (network_->isLoad(pin))
      load_pin = pin;
  }
  delete pin_iter;
  return load_pin;
}

void
Sta::setResistance(const Net *net,
		   const MinMaxAll *min_max,
		   float res)
{
  sdc_->setResistance(net, min_max, res);
}

////////////////////////////////////////////////////////////////

bool
Sta::readSpef(const char *filename,
	      Instance *instance,
              const Corner *corner,
	      const MinMaxAll *min_max,
	      bool pin_cap_included,
	      bool keep_coupling_caps,
	      float coupling_cap_factor,
	      ReducedParasiticType reduce_to,
	      bool delete_after_reduce,
	      bool quiet)
{
  setParasiticAnalysisPts(corner != nullptr,
                          min_max != MinMaxAll::all());
  if (corner == nullptr)
    corner = cmd_corner_;
  const MinMax *cnst_min_max = (min_max == MinMaxAll::all())
    ? MinMax::max()
    : min_max->asMinMax();
  ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(cnst_min_max);
  const OperatingConditions *op_cond =
    sdc_->operatingConditions(cnst_min_max);
  bool success = readSpefFile(filename, instance, ap,
			      pin_cap_included, keep_coupling_caps,
                              coupling_cap_factor,
			      reduce_to, delete_after_reduce,
			      op_cond, corner, cnst_min_max, quiet,
			      report_, network_, parasitics_);
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
  return success;
}

void
Sta::setParasiticAnalysisPts(bool per_corner,
                             bool per_min_max)
{
  if (per_corner != parasitics_per_corner_
      || per_min_max != parasitics_per_min_max_) {
    deleteParasitics();
    parasitics_per_corner_ = per_corner;
    parasitics_per_min_max_ = per_min_max;
    makeParasiticAnalysisPts();
  }
}

void
Sta::makeParasiticAnalysisPts()
{
  corners_->makeParasiticAnalysisPts(parasitics_per_corner_,
                                     parasitics_per_min_max_);
}

void
Sta::reportParasiticAnnotation(bool report_unannotated,
                               const Corner *corner)
{
  ensureGraph();
  findDelays();
  sta::reportParasiticAnnotation(report_unannotated, corner, this);
}

void
Sta::findPiElmore(Pin *drvr_pin,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  float &c2,
		  float &rpi,
		  float &c1,
		  bool &exists) const
{
  Corner *corner = cmd_corner_;
  const ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(min_max);
  Parasitic *pi_elmore = parasitics_->findPiElmore(drvr_pin, rf, ap);
  if (pi_elmore) {
    parasitics_->piModel(pi_elmore, c2, rpi, c1);
    exists = true;
  }
  else
    exists = false;
}

void
Sta::makePiElmore(Pin *drvr_pin,
		  const RiseFall *rf,
		  const MinMaxAll *min_max,
		  float c2,
		  float rpi,
		  float c1)
{
  Corner *corner = cmd_corner_;
  for (MinMax *mm : min_max->range()) {
    ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(mm);
    parasitics_->makePiElmore(drvr_pin, rf, ap, c2, rpi, c1);
  }
  delaysInvalidFrom(drvr_pin);
}

void
Sta::findElmore(Pin *drvr_pin,
		Pin *load_pin,
		const RiseFall *rf,
		const MinMax *min_max,
		float &elmore,
		bool &exists) const
{
  Corner *corner = cmd_corner_;
  const ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(min_max);
  Parasitic *pi_elmore = parasitics_->findPiElmore(drvr_pin, rf, ap);
  if (pi_elmore)
    parasitics_->findElmore(pi_elmore, load_pin, elmore, exists);
  else
    exists = false;
}

void
Sta::setElmore(Pin *drvr_pin,
	       Pin *load_pin,
	       const RiseFall *rf,
	       const MinMaxAll *min_max,
	       float elmore)
{
  Corner *corner = cmd_corner_;
  for (MinMax *mm : min_max->range()) {
    const ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(mm);
    Parasitic *pi_elmore = parasitics_->findPiElmore(drvr_pin, rf, ap);
    if (pi_elmore)
      parasitics_->setElmore(pi_elmore, load_pin, elmore);
  }
  delaysInvalidFrom(drvr_pin);
}

void
Sta::deleteParasitics()
{
  parasitics_->deleteParasitics();
  graph_delay_calc_->delaysInvalid();
  search_->arrivalsInvalid();
}

Parasitic *
Sta::makeParasiticNetwork(const Net *net,
                          bool includes_pin_caps,
                          const ParasiticAnalysisPt *ap)
{
  Parasitic *parasitic = parasitics_->makeParasiticNetwork(net, includes_pin_caps, ap);
  delaysInvalidFromFanin(const_cast<Net*>(net));
  return parasitic;
}

////////////////////////////////////////////////////////////////
//
// Network edit commands.
//
// This implementation calls Sta before/after methods to
// update the Sta components.
// A different implementation may let the network edits
// call the before/after methods implicitly so these functions
// should not (Verific).
//
////////////////////////////////////////////////////////////////

NetworkEdit *
Sta::networkCmdEdit()
{
  return dynamic_cast<NetworkEdit*>(cmd_network_);
}

Instance *
Sta::makeInstance(const char *name,
		  LibertyCell *cell,
		  Instance *parent)
{
  NetworkEdit *network = networkCmdEdit();
  Instance *inst = network->makeInstance(cell, name, parent);
  network->makePins(inst);
  makeInstanceAfter(inst);
  return inst;
}

void
Sta::deleteInstance(Instance *inst)
{
  NetworkEdit *network = networkCmdEdit();
  deleteInstanceBefore(inst);
  network->deleteInstance(inst);
}

void
Sta::replaceCell(Instance *inst,
		 LibertyCell *to_lib_cell)
{
  Cell *to_cell = network_->cell(to_lib_cell);
  replaceCell(inst, to_cell, to_lib_cell);
}

void
Sta::replaceCell(Instance *inst,
		 Cell *to_cell)
{
  LibertyCell *to_lib_cell = network_->libertyCell(to_cell);
  replaceCell(inst, to_cell, to_lib_cell);
}

void
Sta::replaceCell(Instance *inst,
		 Cell *to_cell,
		 LibertyCell *to_lib_cell)
{
  NetworkEdit *network = networkCmdEdit();
  LibertyCell *from_lib_cell = network->libertyCell(inst);
  if (sta::equivCells(from_lib_cell, to_lib_cell)) {
    replaceEquivCellBefore(inst, to_lib_cell);
    network->replaceCell(inst, to_cell);
    replaceEquivCellAfter(inst);
  }
  else {
    replaceCellBefore(inst, to_lib_cell);
    network->replaceCell(inst, to_cell);
    replaceCellAfter(inst);
  }
}

Net *
Sta::makeNet(const char *name,
	     Instance *parent)
{
  NetworkEdit *network = networkCmdEdit();
  Net *net = network->makeNet(name, parent);
  // Sta notification unnecessary.
  return net;
}

void
Sta::deleteNet(Net *net)
{
  NetworkEdit *network = networkCmdEdit();
  deleteNetBefore(net);
  network->deleteNet(net);
}

void
Sta::connectPin(Instance *inst,
		Port *port,
		Net *net)
{
  NetworkEdit *network = networkCmdEdit();
  Pin *pin = network->connect(inst, port, net);
  connectPinAfter(pin);
}

void
Sta::connectPin(Instance *inst,
		LibertyPort *port,
		Net *net)
{
  NetworkEdit *network = networkCmdEdit();
  Pin *pin = network->connect(inst, port, net);
  connectPinAfter(pin);
}

void
Sta::disconnectPin(Pin *pin)
{
  NetworkEdit *network = networkCmdEdit();
  disconnectPinBefore(pin);
  network->disconnectPin(pin);
}

////////////////////////////////////////////////////////////////
//
// Network edit before/after methods.
//
////////////////////////////////////////////////////////////////

void
Sta::makeInstanceAfter(const Instance *inst)
{
  if (graph_) {
    LibertyCell *lib_cell = network_->libertyCell(inst);
    if (lib_cell) {
      // Make vertices and internal instance edges.
      LibertyCellPortBitIterator port_iter(lib_cell);
      while (port_iter.hasNext()) {
        LibertyPort *lib_port = port_iter.next();
        Pin *pin = network_->findPin(inst, lib_port);
        if (pin) {
          Vertex *vertex, *bidir_drvr_vertex;
          graph_->makePinVertices(pin, vertex, bidir_drvr_vertex);

        }
      }
      graph_->makeInstanceEdges(inst);
    }
  }
}

void
Sta::replaceEquivCellBefore(const Instance *inst,
			    const LibertyCell *to_cell)
{
  if (graph_) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      LibertyPort *port = network_->libertyPort(pin);
      if (port) {
        if (port->direction()->isAnyInput()) {
          Vertex *vertex = graph_->pinLoadVertex(pin);
          replaceCellPinInvalidate(port, vertex, to_cell);

          // Replace the timing arc sets in the graph edges.
          VertexOutEdgeIterator edge_iter(vertex, graph_);
          while (edge_iter.hasNext()) {
            Edge *edge = edge_iter.next();
            Vertex *to_vertex = edge->to(graph_);
            if (network_->instance(to_vertex->pin()) == inst) {
              TimingArcSet *from_set = edge->timingArcSet();
              // Find corresponding timing arc set.
              TimingArcSet *to_set = to_cell->findTimingArcSet(from_set);
              if (to_set)
                edge->setTimingArcSet(to_set);
              else
                report_->critical(264, "corresponding timing arc set not found in equiv cells");
            }
          }
        }
        else {
          // Force delay calculation on output pins.
          Vertex *vertex = graph_->pinDrvrVertex(pin);
          graph_delay_calc_->delayInvalid(vertex);
        }
      }
    }
    delete pin_iter;
  }
}

void
Sta::replaceEquivCellAfter(const Instance *inst)
{
  if (graph_) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      if (network_->direction(pin)->isAnyInput())
	parasitics_->loadPinCapacitanceChanged(pin);
    }
    delete pin_iter;
  }
}

void
Sta::replaceCellPinInvalidate(const LibertyPort *from_port,
			      Vertex *vertex,
			      const LibertyCell *to_cell)
{
  LibertyPort *to_port = to_cell->findLibertyPort(from_port->name());
  if (to_port == nullptr
      || (!libertyPortCapsEqual(to_port, from_port)
          // If this is an ideal clock pin, do not invalidate
          // arrivals and delay calc on the clock pin driver.
          && !(to_port->isClock()
               && idealClockMode())))
    // Input port capacitance changed, so invalidate delay
    // calculation from input driver.
    delaysInvalidFromFanin(vertex);
  else
    delaysInvalidFrom(vertex);
}

bool
Sta::idealClockMode()
{
  for (Clock *clk : sdc_->clks()) {
    if (clk->isPropagated())
      return false;
  }
  return true;
}

static bool
libertyPortCapsEqual(const LibertyPort *port1,
		     const LibertyPort *port2)
{
  return port1->capacitance(RiseFall::rise(), MinMax::min())
    == port2->capacitance(RiseFall::rise(), MinMax::min())
    && port1->capacitance(RiseFall::rise(), MinMax::max())
    == port2->capacitance(RiseFall::rise(), MinMax::max())
    && port1->capacitance(RiseFall::fall(), MinMax::min())
    == port2->capacitance(RiseFall::fall(), MinMax::min())
    && port1->capacitance(RiseFall::fall(), MinMax::max())
    == port2->capacitance(RiseFall::fall(), MinMax::max());
}

void
Sta::replaceCellBefore(const Instance *inst,
		       const LibertyCell *to_cell)
{
  if (graph_) {
    // Delete all graph edges between instance pins.
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      LibertyPort *port = network_->libertyPort(pin);
      if (port->direction()->isAnyInput()) {
	Vertex *vertex = graph_->pinLoadVertex(pin);
	replaceCellPinInvalidate(port, vertex, to_cell);

	VertexOutEdgeIterator edge_iter(vertex, graph_);
	while (edge_iter.hasNext()) {
	  Edge *edge = edge_iter.next();
	  Vertex *to_vertex = edge->to(graph_);
	  if (network_->instance(to_vertex->pin()) == inst)
	    deleteEdge(edge);
	}
      }
    }
    delete pin_iter;
  }
}

void
Sta::replaceCellAfter(const Instance *inst)
{
  if (graph_) {
    graph_->makeInstanceEdges(inst);
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      sim_->pinSetFuncAfter(pin);
      if (network_->direction(pin)->isAnyInput())
	parasitics_->loadPinCapacitanceChanged(pin);
    }
    delete pin_iter;
  }
}

void
Sta::connectPinAfter(const Pin *pin)
{
  if (graph_) {
    if (network_->isHierarchical(pin)) {
      graph_->makeWireEdgesThruPin(pin);
      EdgesThruHierPinIterator edge_iter(pin, network_, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	if (edge->role()->isWire()) {
	  connectDrvrPinAfter(edge->from(graph_));
	  connectLoadPinAfter(edge->to(graph_));
	}
      }
    }
    else {
      Vertex *vertex, *bidir_drvr_vertex;
      graph_->pinVertices(pin, vertex, bidir_drvr_vertex);
      if (vertex) {
        search_->arrivalInvalid(vertex);
        search_->requiredInvalid(vertex);
        if (bidir_drvr_vertex) {
          search_->arrivalInvalid(bidir_drvr_vertex);
          search_->requiredInvalid(bidir_drvr_vertex);
        }

        // Make interconnect edges from/to pin.
        if (network_->isDriver(pin)) {
          graph_->makeWireEdgesFromPin(pin);
          connectDrvrPinAfter(bidir_drvr_vertex ? bidir_drvr_vertex : vertex);
        }
        // Note that a bidirect is both a driver and a load so this
        // is NOT an else clause for the above "if".
        if (network_->isLoad(pin)) {
          graph_->makeWireEdgesToPin(pin);
          connectLoadPinAfter(vertex);
        }
      }
    }
  }
  sdc_->connectPinAfter(pin);
  sim_->connectPinAfter(pin);
}

void
Sta::connectDrvrPinAfter(Vertex *vertex)
{
  // Invalidate arrival at fanout vertices.
  VertexOutEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *to_vertex = edge->to(graph_);
    search_->arrivalInvalid(to_vertex);
    search_->endpointInvalid(to_vertex);
    sdc_->clkHpinDisablesChanged(to_vertex->pin());
  }
  Pin *pin = vertex->pin();
  sdc_->clkHpinDisablesChanged(pin);
  graph_delay_calc_->delayInvalid(vertex);
  search_->requiredInvalid(vertex);
  search_->endpointInvalid(vertex);
  levelize_->invalidFrom(vertex);
  clk_network_->connectPinAfter(pin);
}

void
Sta::connectLoadPinAfter(Vertex *vertex)
{
  // Invalidate delays and required at fanin vertices.
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *from_vertex = edge->from(graph_);
    graph_delay_calc_->delayInvalid(from_vertex);
    search_->requiredInvalid(from_vertex);
    sdc_->clkHpinDisablesChanged(from_vertex->pin());
  }
  Pin *pin = vertex->pin();
  sdc_->clkHpinDisablesChanged(pin);
  graph_delay_calc_->delayInvalid(vertex);
  levelize_->invalidFrom(vertex);
  search_->arrivalInvalid(vertex);
  search_->endpointInvalid(vertex);
  clk_network_->connectPinAfter(pin);
}

void
Sta::disconnectPinBefore(const Pin *pin)
{
  parasitics_->disconnectPinBefore(pin);
  sdc_->disconnectPinBefore(pin);
  sim_->disconnectPinBefore(pin);
  if (graph_) {
    if (network_->isDriver(pin)) {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      // Delete wire edges from pin.
      if (vertex) {
	VertexOutEdgeIterator edge_iter(vertex, graph_);
	while (edge_iter.hasNext()) {
	  Edge *edge = edge_iter.next();
	  if (edge->role()->isWire())
	    deleteEdge(edge);
	}
	clk_network_->disconnectPinBefore(pin);
      }
    }
    if (network_->isLoad(pin)) {
      // Delete wire edges to pin.
      Vertex *vertex = graph_->pinLoadVertex(pin);
      if (vertex) {
	VertexInEdgeIterator edge_iter(vertex, graph_);
	while (edge_iter.hasNext()) {
	  Edge *edge = edge_iter.next();
	  if (edge->role()->isWire())
	    deleteEdge(edge);
	}
	clk_network_->disconnectPinBefore(pin);
      }
    }
    if (network_->isHierarchical(pin)) {
      // Delete wire edges thru pin.
      EdgesThruHierPinIterator edge_iter(pin, network_, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	if (edge->role()->isWire()) {
	  deleteEdge(edge);
	  clk_network_->disconnectPinBefore(edge->from(graph_)->pin());
	}
      }
    }
  }
}

void
Sta::deleteEdge(Edge *edge)
{
  Vertex *from = edge->from(graph_);
  Vertex *to = edge->to(graph_);
  search_->arrivalInvalid(to);
  search_->requiredInvalid(from);
  graph_delay_calc_->delayInvalid(to);
  levelize_->relevelizeFrom(to);
  levelize_->deleteEdgeBefore(edge);
  sdc_->clkHpinDisablesChanged(edge->from(graph_)->pin());
  graph_->deleteEdge(edge);
}

void
Sta::deleteNetBefore(const Net *net)
{
  if (graph_) {
    NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      if (!network_->isHierarchical(pin)) {
	disconnectPinBefore(pin);
	// Delete wire edges on net pins.
	Vertex *vertex = graph_->pinDrvrVertex(pin);
	if (vertex) {
	  VertexOutEdgeIterator edge_iter(vertex, graph_);
	  while (edge_iter.hasNext()) {
	    Edge *edge = edge_iter.next();
	    if (edge->role()->isWire())
	      deleteEdge(edge);
	  }
	}
      }
    }
    delete pin_iter;
  }
  sdc_->deleteNetBefore(net);
}

void
Sta::deleteInstanceBefore(const Instance *inst)
{
  if (network_->isLeaf(inst)) {
    deleteInstancePinsBefore(inst);
    deleteLeafInstanceBefore(inst);
  }
  else {
    // Delete hierarchical instance children.
    InstanceChildIterator *child_iter = network_->childIterator(inst);
    while (child_iter->hasNext()) {
      Instance *child = child_iter->next();
      deleteInstanceBefore(child);
    }
    delete child_iter;
  }
}

void
Sta::deleteLeafInstanceBefore(const Instance *inst)
{
  sim_->deleteInstanceBefore(inst);
}

void
Sta::deleteInstancePinsBefore(const Instance *inst)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    deletePinBefore(pin);
  }
  delete pin_iter;
}

void
Sta::deletePinBefore(const Pin *pin)
{
  if (graph_) {
    if (network_->isLoad(pin)) {
      Vertex *vertex = graph_->pinLoadVertex(pin);
      if (vertex) {
        levelize_->deleteVertexBefore(vertex);
        graph_delay_calc_->deleteVertexBefore(vertex);
        search_->deleteVertexBefore(vertex);

        VertexInEdgeIterator in_edge_iter(vertex, graph_);
        while (in_edge_iter.hasNext()) {
          Edge *edge = in_edge_iter.next();
          if (edge->role()->isWire()) {
            Vertex *from = edge->from(graph_);
            // Only notify from vertex (to vertex will be deleted).
            search_->requiredInvalid(from);
          }
          levelize_->deleteEdgeBefore(edge);
        }
        graph_->deleteVertex(vertex);
      }
    }
    if (network_->isDriver(pin)) {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      if (vertex) {
        levelize_->deleteVertexBefore(vertex);
        graph_delay_calc_->deleteVertexBefore(vertex);
        search_->deleteVertexBefore(vertex);

        VertexOutEdgeIterator edge_iter(vertex, graph_);
        while (edge_iter.hasNext()) {
          Edge *edge = edge_iter.next();
          if (edge->role()->isWire()) {
            // Only notify to vertex (from will be deleted).
            Vertex *to = edge->to(graph_);
            // to->prev_paths point to vertex, so delete them.
            search_->arrivalInvalidDelete(to);
            graph_delay_calc_->delayInvalid(to);
            levelize_->relevelizeFrom(to);
          }
          levelize_->deleteEdgeBefore(edge);
        }
        graph_->deleteVertex(vertex);
      }
    }
    if (network_->direction(pin) == PortDirection::internal()) {
      // Internal pins are not loads or drivers.
      Vertex *vertex = graph_->pinLoadVertex(pin);
      if (vertex) {
        levelize_->deleteVertexBefore(vertex);
        graph_delay_calc_->deleteVertexBefore(vertex);
        search_->deleteVertexBefore(vertex);
        graph_->deleteVertex(vertex);
      }
    }
  }
  sim_->deletePinBefore(pin);
  clk_network_->deletePinBefore(pin);
}

void
Sta::delaysInvalidFrom(const Port *port)
{
  if (graph_) {
    Instance *top_inst = network_->topInstance();
    Pin *pin = network_->findPin(top_inst, port);
    delaysInvalidFrom(pin);
  }
}

void
Sta::delaysInvalidFrom(const Instance *inst)
{
  if (graph_) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      delaysInvalidFrom(pin);
    }
    delete pin_iter;
  }
}

void
Sta::delaysInvalidFrom(const Pin *pin)
{
  if (graph_) {
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    delaysInvalidFrom(vertex);
    if (bidirect_drvr_vertex)
      delaysInvalidFrom(bidirect_drvr_vertex);
  }
}

void
Sta::delaysInvalidFrom(Vertex *vertex)
{
  search_->arrivalInvalid(vertex);
  search_->requiredInvalid(vertex);
  graph_delay_calc_->delayInvalid(vertex);
}

void
Sta::delaysInvalidFromFanin(const Port *port)
{
  if (graph_) {
    Instance *top_inst = network_->topInstance();
    Pin *pin = network_->findPin(top_inst, port);
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    delaysInvalidFromFanin(vertex);
    if (bidirect_drvr_vertex)
      delaysInvalidFromFanin(bidirect_drvr_vertex);
  }
}

void
Sta::delaysInvalidFromFanin(const Pin *pin)
{
  if (graph_) {
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    if (vertex)
      delaysInvalidFromFanin(vertex);
    if (bidirect_drvr_vertex)
      delaysInvalidFromFanin(bidirect_drvr_vertex);
  }
}

void
Sta::delaysInvalidFromFanin(const Net *net)
{
  if (graph_) {
    NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      if (!network_->isHierarchical(pin)) {
	Vertex *vertex, *bidirect_drvr_vertex;
	graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
	if (vertex)
	  delaysInvalidFrom(vertex);
	if (bidirect_drvr_vertex)
	  delaysInvalidFrom(bidirect_drvr_vertex);
      }
    }
    delete pin_iter;
  }
}

void
Sta::delaysInvalidFromFanin(Vertex *vertex)
{
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *from_vertex = edge->from(graph_);
    delaysInvalidFrom(from_vertex);
    search_->requiredInvalid(from_vertex);
  }
}

////////////////////////////////////////////////////////////////

ClockSet
Sta::clocks(const Pin *pin)
{
  ensureClkArrivals();
  return search_->clocks(pin);
}

ClockSet
Sta::clockDomains(const Pin *pin)
{
  searchPreamble();
  search_->findAllArrivals();
  return search_->clockDomains(pin);
}

////////////////////////////////////////////////////////////////

InstanceSet
Sta::findRegisterInstances(ClockSet *clks,
			   const RiseFallBoth *clk_rf,
			   bool edge_triggered,
			   bool latches)
{
  findRegisterPreamble();
  return findRegInstances(clks, clk_rf, edge_triggered, latches, this);
}

PinSet
Sta::findRegisterDataPins(ClockSet *clks,
			  const RiseFallBoth *clk_rf,
			  bool edge_triggered,
			  bool latches)
{
  findRegisterPreamble();
  return findRegDataPins(clks, clk_rf, edge_triggered, latches, this);
}

PinSet
Sta::findRegisterClkPins(ClockSet *clks,
			 const RiseFallBoth *clk_rf,
			 bool edge_triggered,
			 bool latches)
{
  findRegisterPreamble();
  return findRegClkPins(clks, clk_rf, edge_triggered, latches, this);
}

PinSet
Sta::findRegisterAsyncPins(ClockSet *clks,
			   const RiseFallBoth *clk_rf,
			   bool edge_triggered,
			   bool latches)
{
  findRegisterPreamble();
  return findRegAsyncPins(clks, clk_rf, edge_triggered, latches, this);
}

PinSet
Sta::findRegisterOutputPins(ClockSet *clks,
			    const RiseFallBoth *clk_rf,
			    bool edge_triggered,
			    bool latches)
{
  findRegisterPreamble();
  return findRegOutputPins(clks, clk_rf, edge_triggered, latches, this);
}

void
Sta::findRegisterPreamble()
{
  ensureGraph();
  ensureGraphSdcAnnotated();
  sim_->ensureConstantsPropagated();
}

////////////////////////////////////////////////////////////////

class FanInOutSrchPred : public SearchPred
{
public:
  FanInOutSrchPred(bool thru_disabled,
		   bool thru_constants,
		   const StaState *sta);
  virtual bool searchFrom(const Vertex *from_vertex);
  virtual bool searchThru(Edge *edge);
  virtual bool searchTo(const Vertex *to_vertex);

protected:
  bool crossesHierarchy(Edge *edge);
  virtual bool searchThruRole(Edge *edge);

  bool thru_disabled_;
  bool thru_constants_;
  const StaState *sta_;
};

FanInOutSrchPred::FanInOutSrchPred(bool thru_disabled,
				   bool thru_constants,
				   const StaState *sta) :
  SearchPred(),
  thru_disabled_(thru_disabled),
  thru_constants_(thru_constants),
  sta_(sta)
{
}

bool
FanInOutSrchPred::searchFrom(const Vertex *from_vertex)
{
  return (thru_disabled_
	  || !from_vertex->isDisabledConstraint())
    && (thru_constants_
	|| !from_vertex->isConstant());
}

bool
FanInOutSrchPred::searchThru(Edge *edge)
{
  const Sdc *sdc = sta_->sdc();
  return searchThruRole(edge)
    && (thru_disabled_
	|| !(edge->isDisabledConstraint()
	     || edge->isDisabledCond()
	     || sdc->isDisabledCondDefault(edge)))
    && (thru_constants_
	|| edge->simTimingSense() != TimingSense::none);
}

bool
FanInOutSrchPred::searchThruRole(Edge *edge)
{
  TimingRole *role = edge->role();
  return role == TimingRole::wire()
    || role == TimingRole::combinational()
    || role == TimingRole::tristateEnable()
    || role == TimingRole::tristateDisable();
}

bool
FanInOutSrchPred::crossesHierarchy(Edge *edge)
{
  Network *network = sta_->network();
  Graph *graph = sta_->graph();
  Vertex *from = edge->from(graph);
  Vertex *to = edge->to(graph);
  Instance *from_inst = network->instance(from->pin());
  Instance *to_inst = network->instance(to->pin());
  return network->parent(from_inst) != network->parent(to_inst);
}

bool
FanInOutSrchPred::searchTo(const Vertex *to_vertex)
{
  return (thru_disabled_
	  || !to_vertex->isDisabledConstraint())
    && (thru_constants_
	|| !to_vertex->isConstant());
}

class FaninSrchPred : public FanInOutSrchPred
{
public:
  FaninSrchPred(bool thru_disabled,
		bool thru_constants,
		const StaState *sta);

protected:
  virtual bool searchThruRole(Edge *edge);
};

FaninSrchPred::FaninSrchPred(bool thru_disabled,
			     bool thru_constants,
			     const StaState *sta) :
  FanInOutSrchPred(thru_disabled, thru_constants, sta)
{
}

bool
FaninSrchPred::searchThruRole(Edge *edge)
{
  TimingRole *role = edge->role();
  return role == TimingRole::wire()
    || role == TimingRole::combinational()
    || role == TimingRole::tristateEnable()
    || role == TimingRole::tristateDisable()
    || role == TimingRole::regClkToQ()
    || role == TimingRole::latchEnToQ();
}

PinSet
Sta::findFaninPins(PinSeq *to,
		   bool flat,
		   bool startpoints_only,
		   int inst_levels,
		   int pin_levels,
		   bool thru_disabled,
		   bool thru_constants)
{
  ensureGraph();
  ensureLevelized();
  PinSet fanin(network_);
  FaninSrchPred pred(thru_disabled, thru_constants, this);
  PinSeq::Iterator to_iter(to);
  while (to_iter.hasNext()) {
    const Pin *pin = to_iter.next();
    if (network_->isHierarchical(pin)) {
      EdgesThruHierPinIterator edge_iter(pin, network_, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	findFaninPins(edge->from(graph_), flat, startpoints_only,
		      inst_levels, pin_levels, fanin, pred);
      }
    }
    else {
      Vertex *vertex = graph_->pinLoadVertex(pin);
      findFaninPins(vertex, flat, startpoints_only,
		    inst_levels, pin_levels, fanin, pred);
    }
  }
  return fanin;
}

void
Sta::findFaninPins(Vertex *vertex,
		   bool flat,
		   bool startpoints_only,
		   int inst_levels,
		   int pin_levels,
		   PinSet &fanin,
		   SearchPred &pred)
{
  VertexSet visited(graph_);
  findFaninPins(vertex, flat, inst_levels,
		pin_levels, visited, &pred, 0, 0);
  VertexSet::Iterator visited_iter(visited);
  while (visited_iter.hasNext()) {
    Vertex *visited_vertex = visited_iter.next();
    Pin *visited_pin = visited_vertex->pin();
    if (!startpoints_only
	|| network_->isRegClkPin(visited_pin)
	|| !hasFanin(visited_vertex, &pred, graph_))
      fanin.insert(visited_pin);
  }
}

void
Sta::findFaninPins(Vertex *to,
		   bool flat,
		   int inst_levels,
		   int pin_levels,
		   VertexSet &visited,
		   SearchPred *pred,
		   int inst_level,
		   int pin_level)
{
  debugPrint(debug_, "fanin", 1, "%s",
             to->name(sdc_network_));
  if (!visited.hasKey(to)) {
    visited.insert(to);
    Pin *to_pin = to->pin();
    bool is_reg_clk_pin = network_->isRegClkPin(to_pin);
    if (!is_reg_clk_pin
	&& (inst_levels <= 0
	    || inst_level < inst_levels)
	&& (pin_levels <= 0
	    || pin_level < pin_levels)
	&& pred->searchTo(to)) {
      VertexInEdgeIterator edge_iter(to, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	Vertex *from_vertex = edge->from(graph_);
	if (pred->searchThru(edge)
	    && (flat
		|| !crossesHierarchy(edge))
	    && pred->searchFrom(from_vertex)) {
	  findFaninPins(from_vertex, flat, inst_levels,
			pin_levels, visited, pred,
			edge->role()->isWire() ? inst_level : inst_level+1,
			pin_level+1);
	}
      }
    }
  }
}

InstanceSet
Sta::findFaninInstances(PinSeq *to,
			bool flat,
			bool startpoints_only,
			int inst_levels,
			int pin_levels,
			bool thru_disabled,
			bool thru_constants)
{
  PinSet pins = findFaninPins(to, flat, startpoints_only, inst_levels,
                              pin_levels, thru_disabled, thru_constants);
  return pinInstances(pins, network_);
}

PinSet
Sta::findFanoutPins(PinSeq *from,
		    bool flat,
		    bool endpoints_only,
		    int inst_levels,
		    int pin_levels,
		    bool thru_disabled,
		    bool thru_constants)
{
  ensureGraph();
  ensureLevelized();
  PinSet fanout(network_);
  FanInOutSrchPred pred(thru_disabled, thru_constants, this);
  PinSeq::Iterator from_iter(from);
  while (from_iter.hasNext()) {
    const Pin *pin = from_iter.next();
    if (network_->isHierarchical(pin)) {
      EdgesThruHierPinIterator edge_iter(pin, network_, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	findFanoutPins(edge->to(graph_), flat, endpoints_only,
		       inst_levels, pin_levels, fanout, pred);
      }
    }
    else {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      findFanoutPins(vertex, flat, endpoints_only,
		     inst_levels, pin_levels, fanout, pred);
    }
  }
  return fanout;
}

void
Sta::findFanoutPins(Vertex *vertex,
		    bool flat,
		    bool endpoints_only,
		    int inst_levels,
		    int pin_levels,
		    PinSet &fanout,
		    SearchPred &pred)
{
  VertexSet visited(graph_);
  findFanoutPins(vertex, flat, inst_levels,
		 pin_levels, visited, &pred, 0, 0);
  VertexSet::Iterator visited_iter(visited);
  while (visited_iter.hasNext()) {
    Vertex *visited_vertex = visited_iter.next();
    Pin *visited_pin = visited_vertex->pin();
    if (!endpoints_only
	|| search_->isEndpoint(visited_vertex, &pred))
      fanout.insert(visited_pin);
  }
}

// DFS to support level limits.
void
Sta::findFanoutPins(Vertex *from,
		    bool flat,
		    int inst_levels,
		    int pin_levels,
		    VertexSet &visited,
		    SearchPred *pred,
		    int inst_level,
		    int pin_level)
{
  debugPrint(debug_, "fanout", 1, "%s",
             from->name(sdc_network_));
  if (!visited.hasKey(from)) {
    visited.insert(from);
    if (!search_->isEndpoint(from, pred)
	&& (inst_levels <= 0
	    || inst_level < inst_levels)
	&& (pin_levels <= 0
	    || pin_level < pin_levels)
	&& pred->searchFrom(from)) {
      VertexOutEdgeIterator edge_iter(from, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	Vertex *to_vertex = edge->to(graph_);
	if (pred->searchThru(edge)
	    && (flat
		|| !crossesHierarchy(edge))
	    && pred->searchTo(to_vertex)) {
	  findFanoutPins(to_vertex, flat, inst_levels,
			 pin_levels, visited, pred,
			 edge->role()->isWire() ? inst_level : inst_level+1,
			 pin_level+1);
	}
      }
    }
  }
}

InstanceSet
Sta::findFanoutInstances(PinSeq *from,
			 bool flat,
			 bool endpoints_only,
			 int inst_levels,
			 int pin_levels,
			 bool thru_disabled,
			 bool thru_constants)
{
  PinSet pins = findFanoutPins(from, flat, endpoints_only, inst_levels,
                               pin_levels, thru_disabled, thru_constants);
  return pinInstances(pins, network_);
}

static InstanceSet
pinInstances(PinSet &pins,
	     const Network *network)
{
  InstanceSet insts(network);
  for (const Pin *pin : pins)
    insts.insert(network->instance(pin));
  return insts;
}

bool
Sta::crossesHierarchy(Edge *edge) const
{
  Vertex *from = edge->from(graph_);
  Vertex *to = edge->to(graph_);
  const Pin *from_pin = from->pin();
  Instance *from_inst = network_->instance(from_pin);
  Instance *to_inst = network_->instance(to->pin());
  Instance *from_parent, *to_parent;
  // Treat input/output port pins as "inside".
  if (network_->isTopInstance(from_inst))
    from_parent = from_inst;
  else 
    from_parent = network_->parent(from_inst);
  if (network_->isTopInstance(to_inst))
    to_parent = to_inst;
  else 
    to_parent = network_->parent(to_inst);
  return from_parent != to_parent;
}

////////////////////////////////////////////////////////////////

class InstanceMaxSlewGreater
{
public:
  explicit InstanceMaxSlewGreater(const StaState *sta);
  bool operator()(const Instance *inst1,
		  const Instance *inst2) const;

protected:
  Slew instMaxSlew(const Instance *inst) const;
  const StaState *sta_;
};

InstanceMaxSlewGreater::InstanceMaxSlewGreater(const StaState *sta) :
  sta_(sta)
{
}

bool
InstanceMaxSlewGreater::operator()(const Instance *inst1,
				   const Instance *inst2) const
{
  return delayGreater(instMaxSlew(inst1), instMaxSlew(inst2), sta_);
}

Slew
InstanceMaxSlewGreater::instMaxSlew(const Instance *inst) const
{
  Network *network = sta_->network();
  Graph *graph = sta_->graph();
  Slew max_slew = 0.0;
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network->isDriver(pin)) {
      Vertex *vertex = graph->pinDrvrVertex(pin);
      for (RiseFall *rf : RiseFall::range()) {
	for (DcalcAnalysisPt *dcalc_ap : sta_->corners()->dcalcAnalysisPts()) {
	  Slew slew = graph->slew(vertex, rf, dcalc_ap->index());
	  if (delayGreater(slew, max_slew, sta_))
	    max_slew = slew;
	}
      }
    }
  }
  delete pin_iter;
  return max_slew;
}

SlowDrvrIterator *
Sta::slowDrvrIterator()
{
  InstanceSeq *insts = new InstanceSeq;
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *leaf = leaf_iter->next();
    insts->push_back(leaf);
  }
  delete leaf_iter;

  sort(insts, InstanceMaxSlewGreater(this));
  return new SlowDrvrIterator(insts);
}

////////////////////////////////////////////////////////////////

void
Sta::checkSlewLimitPreamble()
{
  if (sdc_->haveClkSlewLimits())
    // Arrivals are needed to know pin clock domains.
    updateTiming(false);
  else
    findDelays();
  if (check_slew_limits_ == nullptr)
    makeCheckSlewLimits();
  ensureClkNetwork();
}

PinSeq
Sta::checkSlewLimits(Net *net,
                     bool violators,
                     const Corner *corner,
                     const MinMax *min_max)
{
  checkSlewLimitPreamble();
  return check_slew_limits_->checkSlewLimits(net, violators, corner, min_max);
}

void
Sta::reportSlewLimitShortHeader()
{
  report_path_->reportLimitShortHeader(report_path_->fieldSlew());
}

void
Sta::reportSlewLimitShort(Pin *pin,
			  const Corner *corner,
			  const MinMax *min_max)
{
  const Corner *corner1;
  const RiseFall *rf1;
  Slew slew1;
  float limit1, slack1;
  check_slew_limits_->checkSlew(pin, corner, min_max, true,
				corner1, rf1, slew1, limit1, slack1);
  report_path_->reportLimitShort(report_path_->fieldSlew(), pin,
				 delayAsFloat(slew1), limit1, slack1);
}

void
Sta::reportSlewLimitVerbose(Pin *pin,
			    const Corner *corner,
			    const MinMax *min_max)
{
  const Corner *corner1;
  const RiseFall *rf1;
  Slew slew1;
  float limit1, slack1;
  check_slew_limits_->checkSlew(pin, corner, min_max, true,
				corner1, rf1, slew1, limit1, slack1);
  report_path_->reportLimitVerbose(report_path_->fieldSlew(), pin, rf1,
				   delayAsFloat(slew1),
				   limit1, slack1, corner1, min_max);
}

void
Sta::checkSlew(const Pin *pin,
	       const Corner *corner,
	       const MinMax *min_max,
	       bool check_clks,
	       // Return values.
	       const Corner *&corner1,
	       const RiseFall *&rf,
	       Slew &slew,
	       float &limit,
	       float &slack)
{
  check_slew_limits_->checkSlew(pin, corner, min_max, check_clks,
				corner1, rf, slew, limit, slack);
}

void
Sta::maxSlewCheck(// Return values.
                  const Pin *&pin,
                  Slew &slew,
                  float &slack,
                  float &limit)
{
  checkSlewLimitPreamble();
  PinSeq pins = check_slew_limits_->checkSlewLimits(nullptr, false, nullptr,
                                                    MinMax::max());
  pin = nullptr;
  slew = 0.0;
  slack = INF;
  limit = INF;
  if (!pins.empty()) {
    pin = pins[0];
    const Corner *corner;
    const RiseFall *rf;
    check_slew_limits_->checkSlew(pin, nullptr, MinMax::max(), true,
                                  corner, rf, slew, limit, slack);
  }
}

void
Sta::findSlewLimit(const LibertyPort *port,
                   const Corner *corner,
                   const MinMax *min_max,
                   // Return values.
                   float &limit,
                   bool &exists)
{
  if (check_slew_limits_ == nullptr)
    makeCheckSlewLimits();
  check_slew_limits_->findLimit(port, corner, min_max,
                                limit, exists);
}

////////////////////////////////////////////////////////////////'

void
Sta::checkFanoutLimitPreamble()
{
  if (check_fanout_limits_ == nullptr)
    makeCheckFanoutLimits();
  ensureClkNetwork();
}

PinSeq
Sta::checkFanoutLimits(Net *net,
                       bool violators,
                       const MinMax *min_max)
{
  checkFanoutLimitPreamble();
  return check_fanout_limits_->checkFanoutLimits(net, violators, min_max);
}

void
Sta::reportFanoutLimitShortHeader()
{
  report_path_->reportLimitShortHeader(report_path_->fieldFanout());
}

void
Sta::reportFanoutLimitShort(Pin *pin,
			    const MinMax *min_max)
{
  float fanout, limit, slack;
  check_fanout_limits_->checkFanout(pin, min_max,
				    fanout, limit, slack);
  report_path_->reportLimitShort(report_path_->fieldFanout(),
				 pin, fanout, limit, slack);
}

void
Sta::reportFanoutLimitVerbose(Pin *pin,
			      const MinMax *min_max)
{
  float fanout, limit, slack;
  check_fanout_limits_->checkFanout(pin, min_max,
				    fanout, limit, slack);
  report_path_->reportLimitVerbose(report_path_->fieldFanout(),
				   pin, nullptr, fanout,
				   limit, slack, nullptr, min_max);
}

void
Sta::checkFanout(const Pin *pin,
		 const MinMax *min_max,
		 // Return values.
		 float &fanout,
		 float &limit,
		 float &slack)
{
  check_fanout_limits_->checkFanout(pin, min_max,
				    fanout, limit, slack);
}

void
Sta::maxFanoutCheck(// Return values.
                    const Pin *&pin,
                    float &fanout,
                    float &slack,
                    float &limit)
{
  checkFanoutLimitPreamble();
  PinSeq pins = check_fanout_limits_->checkFanoutLimits(nullptr, false, MinMax::max());
  pin = nullptr;
  fanout = 0;
  slack = INF;
  limit = INF;
  if (!pins.empty()) {
    pin = pins[0];
    check_fanout_limits_->checkFanout(pin, MinMax::max(),
                                      fanout, limit, slack);
  }
}

////////////////////////////////////////////////////////////////'

void
Sta::checkCapacitanceLimitPreamble()
{
  if (check_capacitance_limits_ == nullptr)
    makeCheckCapacitanceLimits();
  ensureClkNetwork();
}

PinSeq
Sta::checkCapacitanceLimits(Net *net,
                            bool violators,
                            const Corner *corner,
                            const MinMax *min_max)
{
  checkCapacitanceLimitPreamble();
  return check_capacitance_limits_->checkCapacitanceLimits(net, violators,
                                                           corner, min_max);
}

void
Sta::reportCapacitanceLimitShortHeader()
{
  report_path_->reportLimitShortHeader(report_path_->fieldCapacitance());
}

void
Sta::reportCapacitanceLimitShort(Pin *pin,
				 const Corner *corner,
				 const MinMax *min_max)
{
  const Corner *corner1;
  const RiseFall *rf;
  float capacitance, limit, slack;
  check_capacitance_limits_->checkCapacitance(pin, corner, min_max,
					      corner1, rf, capacitance,
					      limit, slack);
  report_path_->reportLimitShort(report_path_->fieldCapacitance(),
				 pin, capacitance, limit, slack);
}

void
Sta::reportCapacitanceLimitVerbose(Pin *pin,
				   const Corner *corner,
				   const MinMax *min_max)
{
  const Corner *corner1;
  const RiseFall *rf1;
  float capacitance1, limit1, slack1;
  check_capacitance_limits_->checkCapacitance(pin, corner, min_max,
					      corner1, rf1, capacitance1,
					      limit1, slack1);
  report_path_->reportLimitVerbose(report_path_->fieldCapacitance(),
				   pin, rf1, capacitance1,
				   limit1, slack1, corner1, min_max);
}

void
Sta::checkCapacitance(const Pin *pin,
		      const Corner *corner,
		      const MinMax *min_max,
		      // Return values.
		      const Corner *&corner1,
		      const RiseFall *&rf,
		      float &capacitance,
		      float &limit,
		      float &slack)
{
  check_capacitance_limits_->checkCapacitance(pin, corner, min_max,
					      corner1, rf, capacitance,
					      limit, slack);
}

void
Sta::maxCapacitanceCheck(// Return values.
                         const Pin *&pin,
                         float &capacitance,
                         float &slack,
                         float &limit)
{
  checkCapacitanceLimitPreamble();
  PinSeq pins = check_capacitance_limits_->checkCapacitanceLimits(nullptr, false,
                                                                  nullptr,
                                                                  MinMax::max());
  pin = nullptr;
  capacitance = 0.0;
  slack = INF;
  limit = INF;
  if (!pins.empty()) {
    pin = pins[0];
    const Corner *corner;
    const RiseFall *rf;
    check_capacitance_limits_->checkCapacitance(pin, nullptr, MinMax::max(),
                                                corner, rf, capacitance, limit, slack);
  }
}

////////////////////////////////////////////////////////////////

void
Sta::minPulseWidthPreamble()
{
  ensureClkArrivals();
  if (check_min_pulse_widths_ == nullptr)
    makeCheckMinPulseWidths();
}

MinPulseWidthCheckSeq &
Sta::minPulseWidthChecks(PinSeq *pins,
			 const Corner *corner)
{
  minPulseWidthPreamble();
  return check_min_pulse_widths_->check(pins, corner);
}

MinPulseWidthCheckSeq &
Sta::minPulseWidthChecks(const Corner *corner)
{
  minPulseWidthPreamble();
  return check_min_pulse_widths_->check(corner);
}

MinPulseWidthCheckSeq &
Sta::minPulseWidthViolations(const Corner *corner)
{
  minPulseWidthPreamble();
  return check_min_pulse_widths_->violations(corner);
}

MinPulseWidthCheck *
Sta::minPulseWidthSlack(const Corner *corner)
{
  minPulseWidthPreamble();
  return check_min_pulse_widths_->minSlackCheck(corner);
}

void
Sta::reportMpwChecks(MinPulseWidthCheckSeq *checks,
		     bool verbose)
{
  report_path_->reportMpwChecks(checks, verbose);
}

void
Sta::reportMpwCheck(MinPulseWidthCheck *check,
		    bool verbose)
{
  report_path_->reportMpwCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

MinPeriodCheckSeq &
Sta::minPeriodViolations()
{
  minPeriodPreamble();
  return check_min_periods_->violations();
}

MinPeriodCheck *
Sta::minPeriodSlack()
{
  minPeriodPreamble();
  return check_min_periods_->minSlackCheck();
}

void
Sta::minPeriodPreamble()
{
  // Need clk arrivals to know what clks arrive at the clk tree endpoints.
  ensureClkArrivals();
  if (check_min_periods_ == nullptr)
    makeCheckMinPeriods();
}

void
Sta::reportChecks(MinPeriodCheckSeq *checks,
		  bool verbose)
{
  report_path_->reportChecks(checks, verbose);
}

void
Sta::reportCheck(MinPeriodCheck *check,
		 bool verbose)
{
  report_path_->reportCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

MaxSkewCheckSeq &
Sta::maxSkewViolations()
{
  maxSkewPreamble();
  return check_max_skews_->violations();
}

MaxSkewCheck *
Sta::maxSkewSlack()
{
  maxSkewPreamble();
  return check_max_skews_->minSlackCheck();
}

void
Sta::maxSkewPreamble()
{
  ensureClkArrivals();
  if (check_max_skews_ == nullptr)
    makeCheckMaxSkews();
}

void
Sta::reportChecks(MaxSkewCheckSeq *checks,
		  bool verbose)
{
  report_path_->reportChecks(checks, verbose);
}

void
Sta::reportCheck(MaxSkewCheck *check,
		 bool verbose)
{
  report_path_->reportCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

void
Sta::makeEquivCells(LibertyLibrarySeq *equiv_libs,
		    LibertyLibrarySeq *map_libs)
{
  delete equiv_cells_;
  equiv_cells_ = new EquivCells(equiv_libs, map_libs);
}

LibertyCellSeq *
Sta::equivCells(LibertyCell *cell)
{
  if (equiv_cells_)
    return equiv_cells_->equivs(cell);
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

void
Sta::writeTimingModel(const char *lib_name,
                      const char *cell_name,
                      const char *filename,
                      const Corner *corner)
{
  LibertyLibrary *library = makeTimingModel(lib_name, cell_name, filename,
                                            corner, this);
  writeLiberty(library, filename, this);
}

////////////////////////////////////////////////////////////////

void
Sta::powerPreamble()
{
  // Use arrivals to find clocking info.
  searchPreamble();
  search_->findAllArrivals();
  ensureClkNetwork();
}

void
Sta::power(const Corner *corner,
	   // Return values.
	   PowerResult &total,
	   PowerResult &sequential,
	   PowerResult &combinational,
	   PowerResult &macro,
	   PowerResult &pad)
{
  powerPreamble();
  power_->power(corner, total, sequential, combinational, macro, pad);
}

PowerResult
Sta::power(const Instance *inst,
	   const Corner *corner)
{
  powerPreamble();
  return power_->power(inst, corner);
}

PwrActivity
Sta::findClkedActivity(const Pin *pin)
{
  powerPreamble();
  return power_->findClkedActivity(pin);
}

////////////////////////////////////////////////////////////////

void
Sta::ensureClkNetwork()
{
  ensureLevelized();
  clk_network_->ensureClkNetwork();
}

bool
Sta::isClock(const Pin *pin) const
{
  return clk_network_->isClock(pin);
}

bool
Sta::isClock(const Net *net) const
{
  return clk_network_->isClock(net);
}

bool
Sta::isIdealClock(const Pin *pin) const
{
  return clk_network_->isIdealClock(pin);
}

bool
Sta::isPropagatedClock(const Pin *pin) const
{
  return clk_network_->isPropagatedClock(pin);
}

const PinSet *
Sta::pins(const Clock *clk)
{
  return clk_network_->pins(clk);
}

void
Sta::clkPinsInvalid()
{
  clk_network_->clkPinsInvalid();
}

} // namespace
