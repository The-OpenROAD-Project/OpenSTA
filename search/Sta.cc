// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "Sta.hh"

#include <filesystem>

#include "Machine.hh"
#include "ContainerHelpers.hh"
#include "DispatchQueue.hh"
#include "ReportTcl.hh"
#include "Debug.hh"
#include "Stats.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "PatternMatch.hh"
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
#include "Mode.hh"
#include "Variables.hh"
#include "WriteSdc.hh"
#include "ExceptionPath.hh"
#include "Parasitics.hh"
#include "parasitics/SpefReader.hh"
#include "parasitics/ReportParasiticAnnotation.hh"
#include "DelayCalc.hh"
#include "ArcDelayCalc.hh"
#include "GraphDelayCalc.hh"
#include "sdf/SdfWriter.hh"
#include "Levelize.hh"
#include "Sim.hh"
#include "ClkInfo.hh"
#include "TagGroup.hh"
#include "Search.hh"
#include "Latches.hh"
#include "PathGroup.hh"
#include "CheckTiming.hh"
#include "CheckSlews.hh"
#include "CheckFanouts.hh"
#include "CheckCapacitances.hh"
#include "CheckMinPulseWidths.hh"
#include "CheckMinPeriods.hh"
#include "CheckMaxSkews.hh"
#include "ClkSkew.hh"
#include "ClkLatency.hh"
#include "FindRegister.hh"
#include "ReportPath.hh"
#include "Genclks.hh"
#include "ClkNetwork.hh"
#include "power/Power.hh"
#include "VisitPathEnds.hh"
#include "PathExpanded.hh"
#include "MakeTimingModel.hh"
#include "parasitics/ConcreteParasitics.hh"
#include "spice/WritePathSpice.hh"

namespace sta {

using std::string;
using std::min;
using std::max;

static bool
libertyPortCapsEqual(const LibertyPort *port1,
		     const LibertyPort *port2);
static bool
hasDisabledArcs(Edge *edge,
                const Mode *mode);
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
  StaDelayCalcObserver(Search *search);
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
  StaSimObserver(StaState *sta);
  void valueChangeAfter(const Pin *pin) override;
  void faninEdgesChangeAfter(const Pin *pin) override;
  void fanoutEdgesChangeAfter(const Pin *pin) override;
};

StaSimObserver::StaSimObserver(StaState *sta) :
  SimObserver(sta)
{
}

// When pins with constant values are incrementally connected to a net
// the downstream delays and arrivals will not be updated (removed)
// because the search predicate does not search through constants.
// This observer makes sure the delays and arrivals are invalidated.
void
StaSimObserver::valueChangeAfter(const Pin *pin)
{
  Vertex *vertex = graph_->pinDrvrVertex(pin);
  if (vertex) {
  search_->arrivalInvalid(vertex);
  search_->requiredInvalid(vertex);
  search_->endpointInvalid(vertex);
  }
}

void
StaSimObserver::faninEdgesChangeAfter(const Pin *pin)
{
  Vertex *vertex = graph_->pinDrvrVertex(pin);
  search_->arrivalInvalid(vertex);
  search_->endpointInvalid(vertex);
}

void
StaSimObserver::fanoutEdgesChangeAfter(const Pin *pin)
{
  Vertex *vertex = graph_->pinDrvrVertex(pin);
  search_->requiredInvalid(vertex);
  search_->endpointInvalid(vertex);
}

////////////////////////////////////////////////////////////////

class StaLevelizeObserver : public LevelizeObserver
{
public:
  StaLevelizeObserver(Search *search, GraphDelayCalc *graph_delay_calc);
  void levelsChangedBefore() override;
  void levelChangedBefore(Vertex *vertex) override;

private:
  Search *search_;
  GraphDelayCalc *graph_delay_calc_;
};

StaLevelizeObserver::StaLevelizeObserver(Search *search,
                                         GraphDelayCalc *graph_delay_calc) :
  search_(search),
  graph_delay_calc_(graph_delay_calc)
{
}

void
StaLevelizeObserver::levelsChangedBefore()
{
  search_->levelsChangedBefore();
  graph_delay_calc_->levelsChangedBefore();
}

void
StaLevelizeObserver::levelChangedBefore(Vertex *vertex)
{
  search_->levelChangedBefore(vertex);
  graph_delay_calc_->levelChangedBefore(vertex);
}

////////////////////////////////////////////////////////////////

void
initSta()
{
  initElapsedTime();
  PortDirection::init();
  initLiberty();
  initDelayConstants();
  registerDelayCalcs();
  initPathSenseThru();
}

void
deleteAllMemory()
{
  Sta *sta = Sta::sta();
  if (sta) {
    delete sta;
    Sta::setSta(nullptr);
  }
  deleteDelayCalcs();
  PortDirection::destroy();
  deleteLiberty();
  deleteTmpStrings();
}

////////////////////////////////////////////////////////////////

// Singleton used by TCL commands.
Sta *Sta::sta_;

Sta::Sta() :
  StaState(),
  cmd_scene_(nullptr),
  current_instance_(nullptr),
  verilog_reader_(nullptr),
  check_timing_(nullptr),
  check_slews_(nullptr),
  check_fanouts_(nullptr),
  check_capacitances_(nullptr),
  check_min_pulse_widths_(nullptr),
  check_min_periods_(nullptr),
  check_max_skews_(nullptr),
  clk_skews_(nullptr),
  report_path_(nullptr),
  power_(nullptr),
  update_genclks_(false),
  equiv_cells_(nullptr),
  properties_(this)
{
}

void
Sta::makeComponents()
{
  makeVariables();
  makeReport();
  makeDebug();
  makeUnits();
  makeNetwork();
  makeLevelize();
  makeDefaultScene();
  makeArcDelayCalc();
  makeGraphDelayCalc();
  makeSearch();
  makeLatches();
  makeSdcNetwork();
  makeReportPath();
  makePower();
  makeClkSkews();

  setCmdNamespace1(CmdNamespace::sdc);
  setThreadCount1(defaultThreadCount());
  updateComponentsState();

  makeObservers();
}

void
Sta::makeObservers()
{
  graph_delay_calc_->setObserver(new StaDelayCalcObserver(search_));
  for (Mode *mode : modes_)
    mode->sim()->setObserver(new StaSimObserver(this));
  levelize_->setObserver(new StaLevelizeObserver(search_, graph_delay_calc_));
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
  for (Mode *mode : modes_)
    mode->copyState(this);
  levelize_->copyState(this);
  arc_delay_calc_->copyState(this);
  search_->copyState(this);
  latches_->copyState(this);
  graph_delay_calc_->copyState(this);
  report_path_->copyState(this);
  if (check_timing_)
    check_timing_->copyState(this);
  clk_skews_->copyState(this);

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
Sta::makeLevelize()
{
  levelize_ = new Levelize(this);
}

void
Sta::makeArcDelayCalc()
{
  arc_delay_calc_ = makeDelayCalc("dmp_ceff_elmore", this);
}

void
Sta::makeGraphDelayCalc()
{
  graph_delay_calc_ = new GraphDelayCalc(this);
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
Sta::makeCheckSlews()
{
  check_slews_ = new CheckSlews(this);
}

void
Sta::makeCheckFanouts()
{
  check_fanouts_ = new CheckFanouts(this);
}

void
Sta::makeCheckCapacitances()
{
  check_capacitances_ = new CheckCapacitances(this);
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
Sta::makePower()
{
  power_ = new Power(this);
}

void
Sta::makeVariables()
{
  variables_ = new Variables();
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
  delete variables_;
  // Verilog modules refer to the network in the sta so it has
  // to deleted before the network.
  delete verilog_reader_;
  // Delete "top down" to minimize chance of referencing deleted memory.
  delete check_slews_;
  delete check_fanouts_;
  delete check_capacitances_;
  delete check_min_pulse_widths_;
  delete check_min_periods_;
  delete check_max_skews_;
  delete clk_skews_;
  delete check_timing_;
  delete report_path_;
  // Sdc references search filter, so delete search first.
  delete search_;
  delete latches_;
  delete arc_delay_calc_;
  delete graph_delay_calc_;
  delete levelize_;
  delete graph_;
  delete sdc_network_;
  delete network_;
  delete debug_;
  delete units_;
  delete report_;
  delete power_;
  delete equiv_cells_;
  delete dispatch_queue_;
  deleteContents(parasitics_name_map_);
  deleteContents(modes_);
  deleteContents(scenes_);
}

void
Sta::clear()
{
  clearNonSdc();
  for (Mode *mode : modes_)
    mode->sdc()->clear();
}

void
Sta::clearNonSdc()
{
  // Sdc holds search filter, so clear search first.
  search_->clear();
  levelize_->clear();
  deleteParasitics();
  graph_delay_calc_->clear();
  power_->clear();
  if (check_min_pulse_widths_)
    check_min_pulse_widths_->clear();
  if (check_min_periods_)
    check_min_periods_->clear();
  clk_skews_->clear();

  // scenes are NOT cleared because they are used to index liberty files.
  for (Mode *mode : modes_)
    mode->clkNetwork()->clkPinsInvalid();

  delete graph_;
  graph_ = nullptr;
  current_instance_ = nullptr;
  // Notify components that graph is toast.
  updateComponentsState();
}

Sdc *
Sta::cmdSdc() const
{
  return cmdMode()->sdc();
}

void
Sta::setCmdMode(const string &mode_name)
{
  if (!mode_name.empty()) {
    if (!mode_name_map_.contains(mode_name)) {
      if (modes_.size() == 1
          && modes_[0]->name() == "default") {
        // No need for default mode if one is defined.
        delete modes_[0];
        mode_name_map_.clear();
        modes_.clear();
      }
      Mode *mode = new Mode(mode_name, mode_name_map_.size(), this);
      mode_name_map_[mode_name] = mode;
      modes_.push_back(mode);
      mode->sim()->setMode(mode);
      mode->sim()->setObserver(new StaSimObserver(this));

      if (scenes_.size() == 1
          && scenes_[0]->name() == "default")
        scenes_[0]->setMode(mode);
      updateComponentsState();
    }
  }
}

Mode *
Sta::findMode(const std::string &mode_name) const
{
  return findKey(mode_name_map_, mode_name);
}

ModeSeq
Sta::findModes(const std::string &name) const
{
  ModeSeq matches;
  PatternMatch pattern(name.c_str());
  for (Mode *mode : modes_) {
    if (pattern.match(mode->name()))
      matches.push_back(mode);
  }
  return matches;
}

void
Sta::networkChanged()
{
  clear();
}

void
Sta::networkChangedNonSdc()
{
  clearNonSdc();
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
                 Scene *scene,
		 const MinMaxAll *min_max,
		 bool infer_latches)
{
  Stats stats(debug_, report_);
  LibertyLibrary *library = readLibertyFile(filename, scene, min_max,
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
                     Scene *scene,
		     const MinMaxAll *min_max,
		     bool infer_latches)
{
  LibertyLibrary *liberty = sta::readLibertyFile(filename, infer_latches,
						 network_);
  if (liberty) {
    // Don't map liberty cells if they are redefined by reading another
    // library with the same cell names.
    if (min_max == MinMaxAll::all()) {
      readLibertyAfter(liberty, scene, MinMax::min());
      readLibertyAfter(liberty, scene, MinMax::max());
    }
    else
      readLibertyAfter(liberty, scene, min_max->asMinMax());
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
                      Scene *scene,
		      const MinMax *min_max)
{
  scene->addLiberty(liberty, min_max);
  LibertyLibrary::makeSceneMap(liberty, scene->libertyIndex(min_max),
				network_, report_);
}

bool
Sta::readVerilog(const char *filename)
{
  NetworkReader *network = networkReader();
  if (network) {
    if (verilog_reader_ == nullptr)
      verilog_reader_ = new VerilogReader(network);
    readNetlistBefore();
    return verilog_reader_->read(filename);
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
Sta::linkDesign(const char *top_cell_name,
                bool make_black_boxes)
{
  clear();
  Stats stats(debug_, report_);
  bool status = network_->linkNetwork(top_cell_name,
				      make_black_boxes,
				      report_);
  stats.report("Link");
  return status;
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
Sta::setAnalysisType(AnalysisType analysis_type,
                     Sdc *sdc)
{
  if (analysis_type != sdc->analysisType()) {
    sdc->setAnalysisType(analysis_type);
    delaysInvalid();
    search_->deletePathGroups();
    if (graph_)
      graph_->setDelayCount(dcalcAnalysisPtCount());
  }
}

OperatingConditions *
Sta::operatingConditions(const MinMax *min_max,
                         const Sdc *sdc) const
{
  return sdc->operatingConditions(min_max);
}

void
Sta::setOperatingConditions(OperatingConditions *op_cond,
                            const MinMaxAll *min_max,
                            Sdc *sdc)
{
  sdc->setOperatingConditions(op_cond, min_max);
  delaysInvalid();
}

const Pvt *
Sta::pvt(Instance *inst,
         const MinMax *min_max,
         Sdc *sdc)
{
  return sdc->pvt(inst, min_max);
}

void
Sta::setPvt(Instance *inst,
	    const MinMaxAll *min_max,
	    float process,
	    float voltage,
            float temperature,
            Sdc *sdc)
{
  Pvt pvt(process, voltage, temperature);
  setPvt(inst, min_max, pvt, sdc);
}

void
Sta::setPvt(const Instance *inst,
	    const MinMaxAll *min_max,
            const Pvt &pvt,
            Sdc *sdc)
{
  sdc->setPvt(inst, min_max, pvt);
  delaysInvalidFrom(inst);
}

void
Sta::setVoltage(const MinMax *min_max,
                float voltage,
                Sdc *sdc)
{
  sdc->setVoltage(min_max, voltage);
}

void
Sta::setVoltage(const Net *net,
                const MinMax *min_max,
                float voltage,
                Sdc *sdc)
{
  sdc->setVoltage(net, min_max, voltage);
}

void
Sta::setTimingDerate(TimingDerateType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
                     float derate,
                     Sdc *sdc)
{
  sdc->setTimingDerate(type, clk_data, rf, early_late, derate);
  // Delay calculation results are still valid.
  // The search derates delays while finding arrival times.
  search_->arrivalsInvalid();
}

void
Sta::setTimingDerate(const Net *net,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
                     float derate,
                     Sdc *sdc)
{
  sdc->setTimingDerate(net, clk_data, rf, early_late, derate);
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
                     float derate,
                     Sdc *sdc)
{
  sdc->setTimingDerate(inst, type, clk_data, rf, early_late, derate);
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
                     float derate,
                     Sdc *sdc)
{
  sdc->setTimingDerate(cell, type, clk_data, rf, early_late, derate);
  // Delay calculation results are still valid.
  // The search derates delays while finding arrival times.
  search_->arrivalsInvalid();
}

void
Sta::unsetTimingDerate(Sdc *sdc)
{
  sdc->unsetTimingDerate();
  // Delay calculation results are still valid.
  // The search derates delays while finding arrival times.
  search_->arrivalsInvalid();
}

void
Sta::setInputSlew(const Port *port,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max,
                  float slew,
                  Sdc *sdc)
{
  sdc->setInputSlew(port, rf, min_max, slew);
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
                  const MinMaxAll *min_max,
                  Sdc *sdc)
{
  sdc->setDriveCell(library, cell, port, from_port, from_slews, to_port,
		     rf, min_max);
  delaysInvalidFrom(port);
}

void
Sta::setDriveResistance(const Port *port,
			const RiseFallBoth *rf,
			const MinMaxAll *min_max,
                        float res,
                        Sdc *sdc)
{
  sdc->setDriveResistance(port, rf, min_max, res);
  delaysInvalidFrom(port);
}

void
Sta::setLatchBorrowLimit(const Pin *pin,
                         float limit,
                         Sdc *sdc)
{
  sdc->setLatchBorrowLimit(pin, limit);
  search_->requiredInvalid(pin);
}

void
Sta::setLatchBorrowLimit(const Instance *inst,
                         float limit,
                         Sdc *sdc)
{
  sdc->setLatchBorrowLimit(inst, limit);
  search_->requiredInvalid(inst);
}

void
Sta::setLatchBorrowLimit(const Clock *clk,
                         float limit,
                         Sdc *sdc)
{
  sdc->setLatchBorrowLimit(clk, limit);
  search_->arrivalsInvalid();
}

void
Sta::setMinPulseWidth(const RiseFallBoth *rf,
                      float min_width,
                      Sdc *sdc)
{
  sdc->setMinPulseWidth(rf, min_width);
}

void
Sta::setMinPulseWidth(const Pin *pin,
		      const RiseFallBoth *rf,
                      float min_width,
                      Sdc *sdc)
{
  sdc->setMinPulseWidth(pin, rf, min_width);
}

void
Sta::setMinPulseWidth(const Instance *inst,
		      const RiseFallBoth *rf,
                      float min_width,
                      Sdc *sdc)
{
  sdc->setMinPulseWidth(inst, rf, min_width);
}

void
Sta::setMinPulseWidth(const Clock *clk,
		      const RiseFallBoth *rf,
                      float min_width,
                      Sdc *sdc)
{
  sdc->setMinPulseWidth(clk, rf, min_width);
}

void
Sta::setWireloadMode(WireloadMode mode,
                     Sdc *sdc)
{
  sdc->setWireloadMode(mode);
  delaysInvalid();
}

void
Sta::setWireload(Wireload *wireload,
                 const MinMaxAll *min_max,
                 Sdc *sdc)
{
  sdc->setWireload(wireload, min_max);
  delaysInvalid();
}

void
Sta::setWireloadSelection(WireloadSelection *selection,
                          const MinMaxAll *min_max,
                          Sdc *sdc)
{
  sdc->setWireloadSelection(selection, min_max);
  delaysInvalid();
}

void
Sta::setSlewLimit(Clock *clk,
		  const RiseFallBoth *rf,
		  const PathClkOrData clk_data,
		  const MinMax *min_max,
                  float slew,
                  Sdc *sdc)
{
  sdc->setSlewLimit(clk, rf, clk_data, min_max, slew);
}

void
Sta::setSlewLimit(Port *port,
		  const MinMax *min_max,
                  float slew,
                  Sdc *sdc)
{
  sdc->setSlewLimit(port, min_max, slew);
}

void
Sta::setSlewLimit(Cell *cell,
		  const MinMax *min_max,
                  float slew,
                  Sdc *sdc)
{
  sdc->setSlewLimit(cell, min_max, slew);
}

void
Sta::setCapacitanceLimit(Cell *cell,
			 const MinMax *min_max,
                         float cap,
                         Sdc *sdc)
{
  sdc->setCapacitanceLimit(cell, min_max, cap);
}

void
Sta::setCapacitanceLimit(Port *port,
			 const MinMax *min_max,
                         float cap,
                         Sdc *sdc)
{
  sdc->setCapacitanceLimit(port, min_max, cap);
}

void
Sta::setCapacitanceLimit(Pin *pin,
			 const MinMax *min_max,
                         float cap,
                         Sdc *sdc)
{
  sdc->setCapacitanceLimit(pin, min_max, cap);
}

void
Sta::setFanoutLimit(Cell *cell,
		    const MinMax *min_max,
                    float fanout,
                    Sdc *sdc)
{
  sdc->setFanoutLimit(cell, min_max, fanout);
}

void
Sta::setFanoutLimit(Port *port,
		    const MinMax *min_max,
                    float fanout,
                    Sdc *sdc)
{
  sdc->setFanoutLimit(port, min_max, fanout);
}

void
Sta::setMaxArea(float area,
                Sdc *sdc)
{
  sdc->setMaxArea(area);
}

void
Sta::makeClock(const char *name,
	       PinSet *pins,
	       bool add_to_pins,
	       float period,
	       FloatSeq *waveform,
               char *comment,
               const Mode *mode)
{
  mode->sdc()->makeClock(name, pins, add_to_pins, period, waveform, comment);
  update_genclks_ = true;
  search_->arrivalsInvalid();
  power_->activitiesInvalid();
  mode->clkNetwork()->clkPinsInvalid();
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
                        char *comment,
                        const Mode *mode)
{
  mode->sdc()->makeGeneratedClock(name, pins, add_to_pins,
			   src_pin, master_clk,
			   divide_by, multiply_by, duty_cycle,
			   invert, combinational,
			   edges, edge_shifts, comment);
  update_genclks_ = true;
  search_->arrivalsInvalid();
  power_->activitiesInvalid();
  mode->clkNetwork()->clkPinsInvalid();
}

void
Sta::removeClock(Clock *clk,
                 Sdc *sdc)
{
  sdc->removeClock(clk);
  search_->arrivalsInvalid();
  power_->activitiesInvalid();
}

bool
Sta::isClockSrc(const Pin *pin,
                const Sdc *sdc) const
{
  return sdc->isClock(pin);
}

void
Sta::setPropagatedClock(Clock *clk,
                        const Mode *mode)
{
  mode->sdc()->setPropagatedClock(clk);
  delaysInvalid();
  mode->clkNetwork()->clkPinsInvalid();
}

void
Sta::removePropagatedClock(Clock *clk,
                           const Mode *mode)
{
  mode->sdc()->removePropagatedClock(clk);
  delaysInvalid();
  mode->clkNetwork()->clkPinsInvalid();
}

void
Sta::setPropagatedClock(Pin *pin,
                        const Mode *mode)
{
  mode->sdc()->setPropagatedClock(pin);
  delaysInvalid();
  mode->clkNetwork()->clkPinsInvalid();
}

void
Sta::removePropagatedClock(Pin *pin,
                           const Mode *mode)
{
  mode->sdc()->removePropagatedClock(pin);
  delaysInvalid();
  mode->clkNetwork()->clkPinsInvalid();
}

void
Sta::setClockSlew(Clock *clk,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max,
                  float slew,
                  Sdc *sdc)
{
  sdc->setClockSlew(clk, rf, min_max, slew);
  clockSlewChanged(clk);
}

void
Sta::removeClockSlew(Clock *clk,
                     Sdc *sdc)
{
  sdc->removeClockSlew(clk);
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
                     float delay,
                     Sdc *sdc)
{
  sdc->setClockLatency(clk, pin, rf, min_max, delay);
  search_->arrivalsInvalid();
}

void
Sta::removeClockLatency(const Clock *clk,
                        const Pin *pin,
                        Sdc *sdc)
{
  sdc->removeClockLatency(clk, pin);
  search_->arrivalsInvalid();
}

void
Sta::setClockInsertion(const Clock *clk,
		       const Pin *pin,
		       const RiseFallBoth *rf,
		       const MinMaxAll *min_max,
		       const EarlyLateAll *early_late,
                       float delay,
                       Sdc *sdc)
{
  sdc->setClockInsertion(clk, pin, rf, min_max, early_late, delay);
  search_->arrivalsInvalid();
}

void
Sta::removeClockInsertion(const Clock *clk,
                          const Pin *pin,
                          Sdc *sdc)
{
  sdc->removeClockInsertion(clk, pin);
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
                         float uncertainty,
                         Sdc *sdc)
{
  sdc->setClockUncertainty(pin, setup_hold, uncertainty);
  search_->arrivalsInvalid();
}

void
Sta::removeClockUncertainty(Pin *pin,
                            const SetupHoldAll *setup_hold,
                            Sdc *sdc)
{
  sdc->removeClockUncertainty(pin, setup_hold);
  search_->arrivalsInvalid();
}

void
Sta::setClockUncertainty(Clock *from_clk,
			 const RiseFallBoth *from_rf,
			 Clock *to_clk,
			 const RiseFallBoth *to_rf,
			 const SetupHoldAll *setup_hold,
                         float uncertainty,
                         Sdc *sdc)
{
  sdc->setClockUncertainty(from_clk, from_rf, to_clk, to_rf,
			    setup_hold, uncertainty);
  search_->arrivalsInvalid();
}

void
Sta::removeClockUncertainty(Clock *from_clk,
			    const RiseFallBoth *from_rf,
			    Clock *to_clk,
			    const RiseFallBoth *to_rf,
                            const SetupHoldAll *setup_hold,
                            Sdc *sdc)
{
  sdc->removeClockUncertainty(from_clk, from_rf, to_clk, to_rf, setup_hold);
  search_->arrivalsInvalid();
}

ClockGroups *
Sta::makeClockGroups(const char *name,
		     bool logically_exclusive,
		     bool physically_exclusive,
		     bool asynchronous,
		     bool allow_paths,
                     const char *comment,
                     Sdc *sdc)
{
  ClockGroups *groups = sdc->makeClockGroups(name,
					      logically_exclusive,
					      physically_exclusive,
					      asynchronous,
					      allow_paths,
					      comment);
  search_->requiredsInvalid();
  return groups;
}

void
Sta::removeClockGroupsLogicallyExclusive(const char *name,
                                         Sdc *sdc)
{
  sdc->removeClockGroupsLogicallyExclusive(name);
  search_->requiredsInvalid();
}

void
Sta::removeClockGroupsPhysicallyExclusive(const char *name,
                                          Sdc *sdc)
{
  sdc->removeClockGroupsPhysicallyExclusive(name);
  search_->requiredsInvalid();
}

void
Sta::removeClockGroupsAsynchronous(const char *name,
                                   Sdc *sdc)
{
  sdc->removeClockGroupsAsynchronous(name);
  search_->requiredsInvalid();
}

void
Sta::makeClockGroup(ClockGroups *clk_groups,
                    ClockSet *clks,
                    Sdc *sdc)
{
  sdc->makeClockGroup(clk_groups, clks);
}

void
Sta::setClockSense(PinSet *pins,
		   ClockSet *clks,
                   ClockSense sense,
                   Sdc *sdc)
{
  sdc->setClockSense(pins, clks, sense);
  search_->arrivalsInvalid();
}

////////////////////////////////////////////////////////////////

void
Sta::setClockGatingCheck(const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
                         float margin,
                         Sdc *sdc)
{
  sdc->setClockGatingCheck(rf, setup_hold, margin);
  search_->arrivalsInvalid();
}

void
Sta::setClockGatingCheck(Clock *clk,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
                         float margin,
                         Sdc *sdc)
{
  sdc->setClockGatingCheck(clk, rf, setup_hold, margin);
  search_->arrivalsInvalid();
}

void
Sta::setClockGatingCheck(Instance *inst,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin,
                         LogicValue active_value,
                         Sdc *sdc)
{
  sdc->setClockGatingCheck(inst, rf, setup_hold, margin,active_value);
  search_->arrivalsInvalid();
}

void
Sta::setClockGatingCheck(Pin *pin,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin,
                         LogicValue active_value,
                         Sdc *sdc)
{
  sdc->setClockGatingCheck(pin, rf, setup_hold, margin,active_value);
  search_->arrivalsInvalid();
}

void
Sta::setDataCheck(Pin *from,
		  const RiseFallBoth *from_rf,
		  Pin *to,
		  const RiseFallBoth *to_rf,
		  Clock *clk,
		  const SetupHoldAll *setup_hold,
                  float margin,
                  Sdc *sdc)
{
  sdc->setDataCheck(from, from_rf, to, to_rf, clk, setup_hold,margin);
  search_->requiredInvalid(to);
}

void
Sta::removeDataCheck(Pin *from,
		     const RiseFallBoth *from_rf,
		     Pin *to,
		     const RiseFallBoth *to_rf,
		     Clock *clk,
                     const SetupHoldAll *setup_hold,
                     Sdc *sdc)
{
  sdc->removeDataCheck(from, from_rf, to, to_rf, clk, setup_hold);
  search_->requiredInvalid(to);
}

////////////////////////////////////////////////////////////////

void
Sta::disable(Pin *pin,
             Sdc *sdc)
{
  sdc->disable(pin);
  graph_delay_calc_->delayInvalid(pin);
  search_->arrivalsInvalid();
}

void
Sta::removeDisable(Pin *pin,
                   Sdc *sdc)
{
  sdc->removeDisable(pin);
  disableAfter();
  graph_delay_calc_->delayInvalid(pin);
  search_->arrivalsInvalid();
}

void
Sta::disable(Instance *inst,
	     LibertyPort *from,
             LibertyPort *to,
             Sdc *sdc)
{
  sdc->disable(inst, from, to);
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
  search_->arrivalsInvalid();
}

void
Sta::removeDisable(Instance *inst,
		   LibertyPort *from,
                   LibertyPort *to,
                   Sdc *sdc)
{
  sdc->removeDisable(inst, from, to);
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
  search_->arrivalsInvalid();
}

void
Sta::disable(LibertyCell *cell,
	     LibertyPort *from,
             LibertyPort *to,
             Sdc *sdc)
{
  sdc->disable(cell, from, to);
  disableAfter();
}

void
Sta::removeDisable(LibertyCell *cell,
		   LibertyPort *from,
                   LibertyPort *to,
                   Sdc *sdc)
{
  sdc->removeDisable(cell, from, to);
  disableAfter();
}

void
Sta::disable(LibertyPort *port,
             Sdc *sdc)
{
  sdc->disable(port);
  disableAfter();
}

void
Sta::removeDisable(LibertyPort *port,
                   Sdc *sdc)
{
  sdc->removeDisable(port);
  disableAfter();
}

void
Sta::disable(Port *port,
             Sdc *sdc)
{
  sdc->disable(port);
  disableAfter();
}

void
Sta::removeDisable(Port *port,
                   Sdc *sdc)
{
  sdc->removeDisable(port);
  disableAfter();
}

void
Sta::disable(Edge *edge,
             Sdc *sdc)
{
  sdc->disable(edge);
  disableAfter();
}

void
Sta::removeDisable(Edge *edge,
                   Sdc *sdc)
{
  sdc->removeDisable(edge);
  disableAfter();
}

void
Sta::disable(TimingArcSet *arc_set,
             Sdc *sdc)
{
  sdc->disable(arc_set);
  disableAfter();
}

void
Sta::removeDisable(TimingArcSet *arc_set,
                   Sdc *sdc)
{
  sdc->removeDisable(arc_set);
  disableAfter();
}

void
Sta::disableAfter()
{
  delaysInvalid();
}

////////////////////////////////////////////////////////////////

EdgeSeq
Sta::disabledEdges(const Mode *mode)
{
  const Sdc *sdc = mode->sdc();
  ensureLevelized();
  EdgeSeq disabled_edges;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (isDisabledConstant(edge, mode)
	  || isDisabledCondDefault(edge)
          || isDisabledConstraint(edge, sdc)
	  || edge->isDisabledLoop()
	  || isDisabledPresetClr(edge))
	disabled_edges.push_back(edge);
    }
  }
  return disabled_edges;
}


EdgeSeq
Sta::disabledEdgesSorted(const Mode *mode)
{
  EdgeSeq disabled_edges = disabledEdges(mode);
  sortEdges(&disabled_edges, network_, graph_);
  return disabled_edges;
}

bool
Sta::isDisabledConstraint(Edge *edge,
                          const Sdc *sdc)
{
  Pin *from_pin = edge->from(graph_)->pin();
  Pin *to_pin = edge->to(graph_)->pin();
  return sdc->isDisabledConstraint(from_pin)
    || sdc->isDisabledConstraint(to_pin)
    || sdc->isDisabledConstraint(edge);
}

bool
Sta::isConstant(const Pin *pin,
                const Mode *mode) const
{
  Sim *sim = mode->sim();
  sim->ensureConstantsPropagated();
  return sim->isConstant(pin);
}

bool
Sta::isDisabledConstant(Edge *edge,
                        const Mode *mode)
{
  Sim *sim = mode->sim();
  sim->ensureConstantsPropagated();
  const TimingRole *role = edge->role();
  Vertex *from_vertex = edge->from(graph_);
  Pin *from_pin = from_vertex->pin();
  Vertex *to_vertex = edge->to(graph_);
  Pin *to_pin = to_vertex->pin();
  const Instance *inst = network_->instance(from_pin);
  return sim->isConstant(from_vertex)
    || sim->isConstant(to_vertex)
    || (!role->isWire()
        && (sim->isDisabledCond(edge, inst, from_pin, to_pin)
            || sim->isDisabledMode(edge, inst)
            || hasDisabledArcs(edge, mode)));
}

static bool
hasDisabledArcs(Edge *edge,
                const Mode *mode)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  for (TimingArc *arc : arc_set->arcs()) {
    if (!searchThru(edge, arc, mode))
      return true;
  }
  return false;
}

bool
Sta::isDisabledLoop(Edge *edge) const
{
  return levelize_->isDisabledLoop(edge);
}

PinSet
Sta::disabledConstantPins(Edge *edge,
                          const Mode *mode)
{
  Sim *sim = mode->sim();
  sim->ensureConstantsPropagated();
  PinSet pins(network_);
  Vertex *from_vertex = edge->from(graph_);
  Pin *from_pin = from_vertex->pin();
  Vertex *to_vertex = edge->to(graph_);
  const Pin *to_pin = to_vertex->pin();
  if (sim->isConstant(from_vertex))
    pins.insert(from_pin);
  if (edge->role()->isWire()) {
    if (sim->isConstant(to_vertex))
      pins.insert(to_pin);
  }
  else {
    const Instance *inst = network_->instance(to_pin);
    bool is_disabled;
    FuncExpr *disable_cond;
    sim->isDisabledCond(edge, inst, from_pin, to_pin,
		   is_disabled, disable_cond);
    if (is_disabled)
      exprConstantPins(disable_cond, inst, mode, pins);
    sim->isDisabledMode(edge, inst, is_disabled, disable_cond);
    if (is_disabled)
      exprConstantPins(disable_cond, inst, mode, pins);
    if (hasDisabledArcs(edge, mode)) {
      LibertyPort *to_port = network_->libertyPort(to_pin);
      if (to_port) {
	FuncExpr *func = to_port->function();
	if (func
            && sim->functionSense(inst, from_pin, to_pin) != edge->sense())
          exprConstantPins(func, inst, mode, pins);
      }
    }
  }
  return pins;
}

TimingSense
Sta::simTimingSense(Edge *edge,
                    const Mode *mode)
{
  Pin *from_pin = edge->from(graph_)->pin();
  Pin *to_pin = edge->to(graph_)->pin();
  Instance *inst = network_->instance(from_pin);
  return mode->sim()->functionSense(inst, from_pin, to_pin);
}

void
Sta::exprConstantPins(FuncExpr *expr,
                      const Instance *inst,
                      const Mode *mode,
                      // Return value.
                      PinSet &pins)
{
  LibertyPortSet ports = expr->ports();
  for (LibertyPort *port : ports) {
    Pin *pin = network_->findPin(inst, port);
    if (pin) {
      LogicValue value = mode->sim()->simValue(pin);
      if (value != LogicValue::unknown)
	pins.insert(pin);
    }
  }
}

bool
Sta::isDisabledBidirectInstPath(Edge *edge) const
{
  return !variables_->bidirectInstPathsEnabled()
    && edge->isBidirectInstPath();
}

bool
Sta::isDisabledPresetClr(Edge *edge) const
{
  return !variables_->presetClrArcsEnabled()
    && edge->role() == TimingRole::regSetClr();
}

void
Sta::disableClockGatingCheck(Instance *inst,
                             Sdc *sdc)
{
  sdc->disableClockGatingCheck(inst);
  search_->endpointsInvalid();
}

void
Sta::disableClockGatingCheck(Pin *pin,
                             Sdc *sdc)
{
  sdc->disableClockGatingCheck(pin);
  search_->endpointsInvalid();
}

void
Sta::removeDisableClockGatingCheck(Instance *inst,
                                   Sdc *sdc)
{
  sdc->removeDisableClockGatingCheck(inst);
  search_->endpointsInvalid();
}

void
Sta::removeDisableClockGatingCheck(Pin *pin,
                                   Sdc *sdc)
{
  sdc->removeDisableClockGatingCheck(pin);
  search_->endpointsInvalid();
}

void
Sta::setLogicValue(Pin *pin,
                   LogicValue value,
                   Mode *mode)
{
  mode->sdc()->setLogicValue(pin, value);
  mode->sim()->constantsInvalid();
  // Constants disable edges which isolate downstream vertices of the
  // graph from the delay calculator's BFS search.  This means that
  // simply invaldating the delays downstream from the constant pin
  // fails.  This could be more incremental if the graph delay
  // calculator searched thru disabled edges but ignored their
  // results.
  delaysInvalid();
  power_->activitiesInvalid();
}

void
Sta::setCaseAnalysis(Pin *pin,
                     LogicValue value,
                     Mode *mode)
{
  // Levelization respects constant disabled edges.
  mode->sdc()->setCaseAnalysis(pin, value);
  mode->sim()->constantsInvalid();
  // Constants disable edges which isolate downstream vertices of the
  // graph from the delay calculator's BFS search.  This means that
  // simply invaldating the delays downstream from the constant pin
  // fails.  This could be handled incrementally by invalidating delays
  // on the output of gates one level downstream.
  delaysInvalid();
  power_->activitiesInvalid();
}

void
Sta::removeCaseAnalysis(Pin *pin,
                        Mode *mode)
{
  mode->sdc()->removeCaseAnalysis(pin);
  mode->sim()->constantsInvalid();
  // Constants disable edges which isolate downstream vertices of the
  // graph from the delay calculator's BFS search.  This means that
  // simply invaldating the delays downstream from the constant pin
  // fails.  This could be handled incrementally by invalidating delays
  // on the output of gates one level downstream.
  delaysInvalid();
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
                   float delay,
                   Sdc *sdc)
{
  sdc->setInputDelay(pin, rf, clk, clk_rf, ref_pin,
		      source_latency_included, network_latency_included,
		      min_max, add, delay);

  search_->arrivalInvalid(pin);
}

void 
Sta::removeInputDelay(const Pin *pin,
		      const RiseFallBoth *rf,
		      const Clock *clk,
		      const RiseFall *clk_rf,
                      const MinMaxAll *min_max,
                      Sdc *sdc)
{
  sdc->removeInputDelay(pin, rf, clk, clk_rf, min_max);
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
                    float delay,
                    Sdc *sdc)
{
  sdc->setOutputDelay(pin, rf, clk, clk_rf, ref_pin,
		       source_latency_included,network_latency_included,
		       min_max, add, delay);
  search_->requiredInvalid(pin);
}

void 
Sta::removeOutputDelay(const Pin *pin,
		       const RiseFallBoth *rf,
		       const Clock *clk,
		       const RiseFall *clk_rf,
                       const MinMaxAll *min_max,
                       Sdc *sdc)
{
  sdc->removeOutputDelay(pin, rf, clk, clk_rf, min_max);
  search_->arrivalInvalid(pin);
}

void
Sta::makeFalsePath(ExceptionFrom *from,
		   ExceptionThruSeq *thrus,
		   ExceptionTo *to,
		   const MinMaxAll *min_max,
                   const char *comment,
                   Sdc *sdc)
{
  sdc->makeFalsePath(from, thrus, to, min_max, comment);
  search_->arrivalsInvalid();
}

void
Sta::makeMulticyclePath(ExceptionFrom *from,
			ExceptionThruSeq *thrus,
			ExceptionTo *to,
			const MinMaxAll *min_max,
			bool use_end_clk,
			int path_multiplier,
                        const char *comment,
                        Sdc *sdc)
{
  sdc->makeMulticyclePath(from, thrus, to, min_max,
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
                   bool break_path,
		   float delay,
                   const char *comment,
                   Sdc *sdc)
{
  sdc->makePathDelay(from, thrus, to, min_max, 
		      ignore_clk_latency, break_path,
                      delay, comment);
  search_->endpointsInvalid();
  search_->arrivalsInvalid();
}

void
Sta::resetPath(ExceptionFrom *from,
	       ExceptionThruSeq *thrus,
	       ExceptionTo *to,
               const MinMaxAll *min_max,
               Sdc *sdc)
{
  sdc->resetPath(from, thrus, to, min_max);
  search_->arrivalsInvalid();
}

void
Sta::makeGroupPath(const char *name,
		   bool is_default,
		   ExceptionFrom *from,
		   ExceptionThruSeq *thrus,
		   ExceptionTo *to,
                   const char *comment,
                   Sdc *sdc)
{
  sdc->makeGroupPath(name, is_default, from, thrus, to, comment);
  search_->arrivalsInvalid();
}

bool
Sta::isGroupPathName(const char *group_name,
                     const Sdc *sdc)
{
  return isPathGroupName(group_name, sdc);
}

bool
Sta::isPathGroupName(const char *group_name,
                     const Sdc *sdc) const
{
  return sdc->findClock(group_name)
    || sdc->isGroupPathName(group_name)
    || stringEq(group_name, PathGroups::asyncPathGroupName())
    || stringEq(group_name, PathGroups::pathDelayGroupName())
    || stringEq(group_name, PathGroups::gatedClkGroupName())
    || stringEq(group_name, PathGroups::unconstrainedGroupName());
}

StdStringSeq
Sta::pathGroupNames(const Sdc *sdc) const
{
  StdStringSeq names;
  for (const Clock *clk : sdc->clocks())
    names.push_back(clk->name());

  for (auto const &[name, group] : sdc->groupPaths())
    names.push_back(name);

  names.push_back(PathGroups::asyncPathGroupName());
  names.push_back(PathGroups::pathDelayGroupName());
  names.push_back(PathGroups::gatedClkGroupName());
  names.push_back(PathGroups::unconstrainedGroupName());
  return names;
}

ExceptionFrom *
Sta::makeExceptionFrom(PinSet *from_pins,
		       ClockSet *from_clks,
		       InstanceSet *from_insts,
                       const RiseFallBoth *from_rf,
                       const Sdc *sdc)
{
  return sdc->makeExceptionFrom(from_pins, from_clks, from_insts, from_rf);
}

void
Sta::checkExceptionFromPins(ExceptionFrom *from,
			    const char *file,
                            int line,
                            const Sdc *sdc) const
{
  if (from) {
    PinSet *pins = from->pins();
    if (pins) {
      for (const Pin *pin : *pins) {
        if (!sdc->isExceptionStartpoint(pin)) {
	if (line)
	  report_->fileWarn(1554, file, line, "'%s' is not a valid start point.",
			    cmd_network_->pathName(pin));
	else
	  report_->warn(1550, "'%s' is not a valid start point.",
			cmd_network_->pathName(pin));
      }
    }
  }
  }
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
                       const RiseFallBoth *rf,
                       const Sdc *sdc)
{
  return sdc->makeExceptionThru(pins, nets, insts, rf);
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
                     const RiseFallBoth *end_rf,
                     const Sdc *sdc)
{
  return sdc->makeExceptionTo(to_pins, to_clks, to_insts, rf, end_rf);
}

void
Sta::deleteExceptionTo(ExceptionTo *to)
{
  delete to;
}

void
Sta::checkExceptionToPins(ExceptionTo *to,
			  const char *file,
                          int line,
                          const Sdc *sdc) const
{
  if (to) {
    PinSet *pins = to->pins();
    if (pins) {
      for (const Pin *pin : *pins) {
        if (!sdc->isExceptionEndpoint(pin)) {
	if (line)
	  report_->fileWarn(1551, file, line, "'%s' is not a valid endpoint.",
			    cmd_network_->pathName(pin));
	else
	  report_->warn(1552, "'%s' is not a valid endpoint.",
			cmd_network_->pathName(pin));
      }
    }
  }
  }
}

void
Sta::writeSdc(const Sdc *sdc,
              const char *filename,
	      bool leaf,
	      bool native,
	      int digits,
              bool gzip,
	      bool no_timestamp)
{
  ensureLibLinked();
  sta::writeSdc(sdc, network_->topInstance(), filename, "write_sdc",
                leaf, native, digits, gzip, no_timestamp);
}

////////////////////////////////////////////////////////////////

CheckErrorSeq &
Sta::checkTiming(const Mode *mode,
                 bool no_input_delay,
		 bool no_output_delay,
		 bool reg_multiple_clks,
		 bool reg_no_clks,
		 bool unconstrained_endpoints,
		 bool loops,
		 bool generated_clks)
{
  if (unconstrained_endpoints) {
    // Only arrivals to find unconstrained_endpoints.
  searchPreamble();
    search_->findAllArrivals();
  }
  else {
    ensureGraph();
    ensureLevelized();
    mode->sim()->ensureConstantsPropagated();
    mode->clkNetwork()->ensureClkNetwork();
  }
  if (check_timing_ == nullptr)
    makeCheckTiming();
  return check_timing_->check(mode, no_input_delay, no_output_delay,
			      reg_multiple_clks, reg_no_clks,
			      unconstrained_endpoints,
			      loops, generated_clks);
}

////////////////////////////////////////////////////////////////

bool
Sta::crprEnabled() const
{
  return variables_->crprEnabled();
}

void
Sta::setCrprEnabled(bool enabled)
{
  if (enabled != variables_->crprEnabled())
    search_->arrivalsInvalid();
  variables_->setCrprEnabled(enabled);
}

CrprMode
Sta::crprMode() const
{
  return variables_->crprMode();
}

void
Sta::setCrprMode(CrprMode mode)
{
  // Pessimism is only relevant for on_chip_variation analysis.
  if (variables_->crprEnabled()
      && variables_->crprMode() != mode)
    search_->arrivalsInvalid();
  variables_->setCrprMode(mode);
}

bool
Sta::pocvEnabled() const
{
  return variables_->pocvEnabled();
}

void
Sta::setPocvEnabled(bool enabled)
{
  if (enabled != variables_->pocvEnabled())
    delaysInvalid();
  variables_->setPocvEnabled(enabled);
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
  return variables_->propagateGatedClockEnable();
}

void
Sta::setPropagateGatedClockEnable(bool enable)
{
  if (variables_->propagateGatedClockEnable() != enable)
    search_->arrivalsInvalid();
  variables_->setPropagateGatedClockEnable(enable);
}

bool
Sta::presetClrArcsEnabled() const
{
  return variables_->presetClrArcsEnabled();
}

void
Sta::setPresetClrArcsEnabled(bool enable)
{
  if (variables_->presetClrArcsEnabled() != enable) {
    levelize_->invalid();
    delaysInvalid();
  }
  variables_->setPresetClrArcsEnabled(enable);
}

bool
Sta::condDefaultArcsEnabled() const
{
  return variables_->condDefaultArcsEnabled();
}

void
Sta::setCondDefaultArcsEnabled(bool enabled)
{
  if (variables_->condDefaultArcsEnabled() != enabled) {
    delaysInvalid();
    variables_->setCondDefaultArcsEnabled(enabled);
  }
}

bool
Sta::bidirectInstPathsEnabled() const
{
  return variables_->bidirectInstPathsEnabled();
}

void
Sta::setBidirectInstPathsEnabled(bool enabled)
{
  if (variables_->bidirectInstPathsEnabled() != enabled) {
    levelize_->invalid();
    delaysInvalid();
    variables_->setBidirectInstPathsEnabled(enabled);
  }
}

bool
Sta::recoveryRemovalChecksEnabled() const
{
  return variables_->recoveryRemovalChecksEnabled();
} 

void
Sta::setRecoveryRemovalChecksEnabled(bool enabled)
{
  if (variables_->recoveryRemovalChecksEnabled() != enabled) {
    search_->arrivalsInvalid();
    variables_->setRecoveryRemovalChecksEnabled(enabled);
  }
}

bool
Sta::gatedClkChecksEnabled() const
{
  return variables_->gatedClkChecksEnabled();
}

void
Sta::setGatedClkChecksEnabled(bool enabled)
{
  if (variables_->gatedClkChecksEnabled() != enabled) {
    search_->arrivalsInvalid();
    variables_->setGatedClkChecksEnabled(enabled);
  }
}

bool
Sta::dynamicLoopBreaking() const
{
  return variables_->dynamicLoopBreaking();
}

void
Sta::setDynamicLoopBreaking(bool enable)
{
  if (variables_->dynamicLoopBreaking() != enable) {
    for (Mode *mode : modes_) {
      Sdc *sdc = mode->sdc();
      if (enable)
        sdc->makeLoopExceptions();
      else
        sdc->deleteLoopExceptions();
    }
    search_->arrivalsInvalid();
    variables_->setDynamicLoopBreaking(enable);
  }
}

bool
Sta::useDefaultArrivalClock() const
{
  return variables_->useDefaultArrivalClock();
}

void
Sta::setUseDefaultArrivalClock(bool enable)
{
  if (variables_->useDefaultArrivalClock() != enable) {
    variables_->setUseDefaultArrivalClock(enable);
    search_->arrivalsInvalid();
  }
}

bool
Sta::propagateAllClocks() const
{
  return variables_->propagateAllClocks();
}

void
Sta::setPropagateAllClocks(bool prop)
{
  variables_->setPropagateAllClocks(prop);
}

bool
Sta::clkThruTristateEnabled() const
{
  return variables_->clkThruTristateEnabled();
}

void
Sta::setClkThruTristateEnabled(bool enable)
{
  if (enable != variables_->clkThruTristateEnabled()) {
    search_->arrivalsInvalid();
    variables_->setClkThruTristateEnabled(enable);
  }
}

////////////////////////////////////////////////////////////////

// Init one scene named "default".
void
Sta::makeDefaultScene()
{
  const char *name = "default";
  StringSeq scene_names;
  scene_names.push_back(name);
  Parasitics *parasitics = makeConcreteParasitics(name, "");

  Mode *mode = new Mode(name, 0, this);
  modes_.push_back(mode);
  mode_name_map_[name] = mode;
  mode->sim()->setMode(mode);
  mode->sim()->setObserver(new StaSimObserver(this));

  deleteScenes();
  makeScene(name, mode, parasitics);

  cmd_scene_ = scenes_[0];
}

// define_corners (before read_liberty).
void
Sta::makeScenes(StringSeq *scene_names)
{
  if (scene_names->size() > scene_count_max)
    report_->error(1553, "maximum scene count exceeded");
  Parasitics *parasitics = findParasitics("default");
  Mode *mode = modes_[0];
  mode->sdc()->makeSceneBefore();
  mode->clear();

  deleteScenes();
  for (const char *name : *scene_names)
    makeScene(name, mode, parasitics);

  cmd_scene_ = scenes_[0];
  updateComponentsState();
  if (graph_)
    graph_->makeSceneAfter();
}

void
Sta::makeScene(const std::string &name,
               const std::string &mode_name,
               const StdStringSeq &liberty_min_files,
               const StdStringSeq &liberty_max_files,
               const std::string &spef_min_file,
               const std::string &spef_max_file)
{
  Mode *mode = findMode(mode_name);
  Parasitics *parasitics_default = findParasitics("default");
  Parasitics *parasitics_min = parasitics_default;
  Parasitics *parasitics_max = parasitics_default;
  if (!spef_min_file.empty() && !spef_max_file.empty()) {
    parasitics_min = findParasitics(spef_min_file);
    parasitics_max = findParasitics(spef_max_file);
    if (parasitics_min == nullptr)
      report_->error(1558, "Spef file %s not found.", spef_min_file.c_str());
    if (parasitics_max == nullptr
        && spef_max_file != spef_min_file)
      report_->error(1559, "Spef file %s not found.", spef_max_file.c_str());
  }

  mode->sdc()->makeSceneBefore();
  Scene *scene = makeScene(name, mode, parasitics_min, parasitics_max);
  updateComponentsState();
  if (graph_)
    graph_->makeSceneAfter();
  updateSceneLiberty(scene, liberty_min_files, MinMax::min());
  updateSceneLiberty(scene, liberty_max_files, MinMax::max());
  cmd_scene_ = scene;
}

Scene *
Sta::makeScene(const std::string &name,
               Mode *mode,
               Parasitics *parasitics)
{
  Scene *scene = new Scene(name, scenes_.size(), mode, parasitics);
  scene_name_map_[name] = scene;
  scenes_.push_back(scene);
  mode->addScene(scene);
  return scene;
}

void
Sta::deleteScenes()
{
  for (Scene *scene : scenes_) {
    scene->mode()->removeScene(scene);
    delete scene;
  }
  scenes_.clear();
  scene_name_map_.clear();
}

Scene *
Sta::makeScene(const std::string &name,
               Mode *mode,
               Parasitics *parasitics_min,
               Parasitics *parasitics_max)
{
  if (scenes_.size() == 1
      && findScene("default"))
    deleteScenes();

  Scene *scene = new Scene(name, scenes_.size(), mode,
                           parasitics_min, parasitics_max);
  scene_name_map_[name] = scene;
  scenes_.push_back(scene);
  mode->addScene(scene);
  return scene;
}

Scene *
Sta::findScene(const std::string &name) const
{
  return findKey(scene_name_map_, name);
}

SceneSeq
Sta::findScenes(const std::string &name) const
{
  SceneSeq matches;
  PatternMatch pattern(name.c_str());
  for (Scene *scene : scenes_) {
    if (pattern.match(scene->name()))
      matches.push_back(scene);
  }
  return matches;
}

SceneSeq
Sta::findScenes(const std::string &name,
                ModeSeq &modes) const
{
  SceneSeq matches;
  PatternMatch pattern(name.c_str());
  for (Mode *mode : modes) {
    for (Scene *scene : mode->scenes()) {
      if (pattern.match(scene->name()))
        matches.push_back(scene);
    }
  }
  return matches;
}

void
Sta::updateSceneLiberty(Scene *scene,
                        const StdStringSeq &liberty_files,
                        const MinMax *min_max)
{
  for (const std::string &lib_file : liberty_files) {
    LibertyLibrary *lib = findLibertyFileBasename(lib_file);
    if (lib)
      LibertyLibrary::makeSceneMap(lib, scene->libertyIndex(min_max),
                                   network_, report_);
    else
      report_->warn(1555, "liberty filename %s not found.", lib_file.c_str());
  }
}

LibertyLibrary *
Sta::findLibertyFileBasename(const std::string &filename) const
{
  LibertyLibraryIterator *lib_iter = network_->libertyLibraryIterator();
  while (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    auto lib_file = std::filesystem::path(lib->filename()).filename().stem();
    auto stem = lib_file.stem();
    if (stem.string() == filename) {
      delete lib_iter;
      return lib;
    }
  }
  delete lib_iter;
  return nullptr;
}

void
Sta::updateLibertyScenes()
{
  for (Scene *scene : scenes_) {
    LibertyLibraryIterator *iter = network_->libertyLibraryIterator();
    while (iter->hasNext()) {
      LibertyLibrary *lib = iter->next();
      for (const MinMax *min_max : MinMax::range()) {
        LibertyLibrary::makeSceneMap(lib, scene->libertyIndex(min_max),
                                     network_, report_);
      }
    }
  }
}

Scene *
Sta::cmdScene() const
{
  return cmd_scene_;
}

void
Sta::setCmdScene(Scene *scene)
{
  cmd_scene_ = scene;
}

SceneSeq
Sta::makeSceneSeq(Scene *scene) const
{
  SceneSeq scenes;
  if (scene)
    scenes.push_back(scene);
  else
    scenes = scenes_;
  return scenes;
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
                  const SceneSeq &scenes,
		  const MinMaxAll *min_max,
		  int group_path_count,
		  int endpoint_path_count,
		  bool unique_pins,
		  bool unique_edges,
		  float slack_min,
		  float slack_max,
		  bool sort_by_slack,
                  StdStringSeq &group_names,
		  bool setup,
		  bool hold,
		  bool recovery,
		  bool removal,
		  bool clk_gating_setup,
		  bool clk_gating_hold)
{
  searchPreamble();
  clk_skews_->clear();
  return search_->findPathEnds(from, thrus, to, unconstrained,
                               scenes, min_max, group_path_count,
			       endpoint_path_count,
                               unique_pins, unique_edges, slack_min, slack_max,
			       sort_by_slack, group_names,
			       setup, hold,
			       recovery, removal,
			       clk_gating_setup, clk_gating_hold);
}

////////////////////////////////////////////////////////////////

// Overall flow:
//  make graph
//  levelize
//  delay calculation
//  update generated clocks
//  propagate constants
//  find arrivals

void
Sta::searchPreamble()
{
  findDelays();
  for (Mode *mode : modes_) {
    mode->sim()->ensureConstantsPropagated();
    mode->sdc()->searchPreamble();
  }
  updateGeneratedClks();
  // Delete results from last findPathEnds because they point to filtered arrivals.
  search_->deletePathGroups();
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
                         bool report_hier_pins,
			 bool report_net,
			 bool report_cap,
			 bool report_slew,
			 bool report_fanout,
			 bool report_src_attr)
{
  report_path_->setReportFields(report_input_pin, report_hier_pins, report_net,
                                report_cap, report_slew, report_fanout,
                                report_src_attr);
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
		   PathEnd *prev_end,
                   bool last)
{
  report_path_->reportPathEnd(end, prev_end, last);
}

void
Sta::reportPathEnds(PathEndSeq *ends)
{
  report_path_->reportPathEnds(ends);
}

void
Sta::reportPath(const Path *path)
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
Sta::reportClkSkew(ConstClockSeq &clks,
                   const SceneSeq &scenes,
		   const SetupHold *setup_hold,
                   bool include_internal_latency,
		   int digits)
{
  clkSkewPreamble();
  clk_skews_->reportClkSkew(clks, scenes, setup_hold,
                            include_internal_latency, digits);
}

float
Sta::findWorstClkSkew(const SetupHold *setup_hold,
                      bool include_internal_latency)
{

  clkSkewPreamble();
  return clk_skews_->findWorstClkSkew(scenes_, setup_hold,
                                      include_internal_latency);
}

void
Sta::clkSkewPreamble()
{
  ensureClkArrivals();
}

void
Sta::makeClkSkews()
{
  clk_skews_ = new ClkSkews(this);
}

////////////////////////////////////////////////////////////////

void
Sta::reportClkLatency(ConstClockSeq &clks,
                      const SceneSeq &scenes,
                      bool include_internal_latency,
                      int digits)
{
  ensureClkArrivals();
  ClkLatency clk_latency(this);
  clk_latency.reportClkLatency(clks, scenes, include_internal_latency, digits);
}

ClkDelays
Sta::findClkDelays(const Clock *clk,
                   const Scene *scene,
                   bool include_internal_latency)
{
  ensureClkArrivals();
  ClkLatency clk_latency(this);
  return clk_latency.findClkDelays(clk, scene, include_internal_latency);
}

////////////////////////////////////////////////////////////////

void
Sta::delaysInvalid() const
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

VertexSet &
Sta::endpoints()
{
  ensureGraph();
  return search_->endpoints();
}

PinSet
Sta::endpointPins()
{
  ensureGraph();
  PinSet pins(network_);
  for (Vertex *vertex : search_->endpoints())
    pins.insert(vertex->pin());
  return pins;
}

int
Sta::endpointViolationCount(const MinMax *min_max)
{
  int violations = 0;
  for (Vertex *end : search_->endpoints()) {
    if (delayLess(slack(end, min_max), 0.0, this))
      violations++;
  }
  return violations;
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

Path *
Sta::vertexWorstArrivalPath(Vertex *vertex,
			    const MinMax *min_max)
{
  return vertexWorstArrivalPath(vertex, nullptr, min_max);
}

Path *
Sta::vertexWorstArrivalPath(Vertex *vertex,
			    const RiseFall *rf,
			    const MinMax *min_max)
{
  Path *worst_path = nullptr;
  Arrival worst_arrival = min_max->initValue();
  VertexPathIterator path_iter(vertex, rf, min_max, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Arrival arrival = path->arrival();
    if (!path->tag(this)->isGenClkSrcPath()
	&& delayGreater(arrival, worst_arrival, min_max, this)) {
      worst_arrival = arrival;
      worst_path = path;
    }
  }
  return worst_path;
}

Path *
Sta::vertexWorstRequiredPath(Vertex *vertex,
                             const MinMax *min_max)
{
  return vertexWorstRequiredPath(vertex, nullptr, min_max);
}

Path *
Sta::vertexWorstRequiredPath(Vertex *vertex,
                             const RiseFall *rf,
                             const MinMax *min_max)
{
  Path *worst_path = nullptr;
  const MinMax *req_min_max = min_max->opposite();
  Arrival worst_req = req_min_max->initValue();
  VertexPathIterator path_iter(vertex, rf, min_max, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    const Required path_req = path->required();
    if (!path->tag(this)->isGenClkSrcPath()
	&& delayGreater(path_req, worst_req, req_min_max, this)) {
      worst_req = path_req;
      worst_path = path;
    }
  }
  return worst_path;
}

Path *
Sta::vertexWorstSlackPath(Vertex *vertex,
			  const RiseFall *rf,
			  const MinMax *min_max)
{
  Path *worst_path = nullptr;
  Slack min_slack = MinMax::min()->initValue();
  VertexPathIterator path_iter(vertex, rf, min_max, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Slack slack = path->slack(this);
    if (!path->tag(this)->isGenClkSrcPath()
	&& delayLess(slack, min_slack, this)) {
      min_slack = slack;
      worst_path = path;
    }
  }
  return worst_path;
}

Path *
Sta::vertexWorstSlackPath(Vertex *vertex,
			  const MinMax *min_max)

{
  return vertexWorstSlackPath(vertex, nullptr, min_max);
}

Arrival
Sta::arrival(const Pin *pin,
             const RiseFallBoth *rf,
                const MinMax *min_max)
{
  Vertex *vertex, *bidirect_vertex;
  graph_->pinVertices(pin, vertex, bidirect_vertex);
  Arrival worst_arrival = min_max->initValue();
  if (vertex)
    worst_arrival = arrival(vertex, rf, scenes_, min_max);
  if (bidirect_vertex) {
    Arrival arrival2 = arrival(bidirect_vertex, rf, scenes_, min_max);
    if (delayGreater(arrival2, worst_arrival, min_max, this))
      worst_arrival = arrival2;
  }
  return worst_arrival;
}

Arrival
Sta::arrival(Vertex *vertex,
             const RiseFallBoth *rf,
             const SceneSeq &scenes,
                   const MinMax *min_max)
{
  searchPreamble();
  search_->findArrivals(vertex->level());
  const SceneSet scenes_set = Scene::sceneSet(scenes);
  Arrival arrival = min_max->initValue();
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    const Arrival &path_arrival = path->arrival();
    const ClkInfo *clk_info = path->clkInfo(search_);
    if (!clk_info->isGenClkSrcPath()
        && (rf == RiseFallBoth::riseFall()
            || path->transition(this)->asRiseFallBoth() == rf)
        && path->minMax(this) == min_max
        && scenes_set.contains(path->scene(this))
	&& delayGreater(path->arrival(), arrival, min_max, this))
      arrival = path_arrival;
  }
  return arrival;
}

Required
Sta::required(Vertex *vertex,
              const RiseFallBoth *rf,
              const SceneSeq &scenes,
		    const MinMax *min_max)
{
  findRequired(vertex);
  const SceneSet scenes_set = Scene::sceneSet(scenes);
  const MinMax *req_min_max = min_max->opposite();
  Required required = req_min_max->initValue();
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    const Path *path = path_iter.next();
    const Required path_required = path->required();
    if ((rf == RiseFallBoth::riseFall()
         || path->transition(this)->asRiseFallBoth() == rf)
        && path->minMax(this) == min_max
        && scenes_set.contains(path->scene(this))
	&& delayGreater(path_required, required, req_min_max, this))
      required = path_required;
  }
  return required;
}

////////////////////////////////////////////////////////////////

Slack
Sta::slack(const Net *net,
	      const MinMax *min_max)
{
  ensureGraph();
  Slack min_slack = MinMax::min()->initValue();
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isLoad(pin)) {
      Vertex *vertex = graph_->pinLoadVertex(pin);
      Slack pin_slack = slack(vertex, min_max);
      if (delayLess(pin_slack, min_slack, this))
        min_slack = pin_slack;
    }
  }
  delete pin_iter;
  return min_slack;
}

Slack
Sta::slack(const Pin *pin,
           const RiseFallBoth *rf,
           const SceneSeq &scenes,
	      const MinMax *min_max)
{
  ensureGraph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slack min_slack = MinMax::min()->initValue();
  if (vertex)
    min_slack = slack(vertex, rf, scenes, min_max);
  if (bidirect_drvr_vertex) {
    Slack slack1 = slack(bidirect_drvr_vertex, rf, scenes, min_max);
    if (delayLess(slack1, min_slack, this))
      min_slack = slack1;
  }
  return min_slack;
}

Slack
Sta::slack(Vertex *vertex,
           const MinMax *min_max)
{
  return slack(vertex, RiseFallBoth::riseFall(), scenes_, min_max);
}

Slack
Sta::slack(Vertex *vertex,
	      const RiseFall *rf,
	      const MinMax *min_max)
{
  return slack(vertex, rf->asRiseFallBoth(), scenes_, min_max);
}

Slack
Sta::slack(Vertex *vertex,
           const RiseFallBoth *rf,
           const SceneSeq &scenes,
           const MinMax *min_max)
{
  findRequired(vertex);
  const SceneSet scenes_set = Scene::sceneSet(scenes);
  const MinMax *min = MinMax::min();
  Slack slack = min->initValue();
  VertexPathIterator path_iter(vertex, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Slack path_slack = path->slack(this);
    if ((rf == RiseFallBoth::riseFall()
         || path->transition(this)->asRiseFallBoth() == rf)
        && path->minMax(this) == min_max
        && scenes_set.contains(path->scene(this))
        && delayLess(path_slack, slack, this))
      slack = path_slack;
  }
  return slack;
}

void
Sta::slacks(Vertex *vertex,
            Slack (&slacks)[RiseFall::index_count][MinMax::index_count])
{
  findRequired(vertex);
  for (int rf_index : RiseFall::rangeIndex()) {
    for (const MinMax *min_max : MinMax::range()) {
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

class EndpointPathEndVisitor : public PathEndVisitor
{
public:
  EndpointPathEndVisitor(const std::string &path_group_name,
			 const MinMax *min_max,
			 const StaState *sta);
  PathEndVisitor *copy() const;
  void visit(PathEnd *path_end);
  Slack slack() const { return slack_; }

private:
  const std::string &path_group_name_;
  const MinMax *min_max_;
  Slack slack_;
  const StaState *sta_;
};

EndpointPathEndVisitor::EndpointPathEndVisitor(const std::string &path_group_name,
					       const MinMax *min_max,
					       const StaState *sta) :
  path_group_name_(path_group_name),
  min_max_(min_max),
  slack_(MinMax::min()->initValue()),
  sta_(sta)
{
}

PathEndVisitor *
EndpointPathEndVisitor::copy() const
{
  return new EndpointPathEndVisitor(path_group_name_, min_max_, sta_);
}

void
EndpointPathEndVisitor::visit(PathEnd *path_end)
{
  if (path_end->minMax(sta_) == min_max_) {
    StdStringSeq group_names = PathGroups::pathGroupNames(path_end, sta_);
    for (std::string &group_name : group_names) {
      if (group_name == path_group_name_) {
	Slack end_slack = path_end->slack(sta_);
	if (delayLess(end_slack, slack_, sta_))
	  slack_ = end_slack;
      }
    }
  }
}

Slack
Sta::endpointSlack(const Pin *pin,
		   const std::string &path_group_name,
		   const MinMax *min_max)
{
  ensureGraph();
  Vertex *vertex = graph_->pinLoadVertex(pin);
  if (vertex) {
    findRequired(vertex);
    VisitPathEnds visit_ends(this);
    EndpointPathEndVisitor path_end_visitor(path_group_name, min_max, this);
    visit_ends.visitPathEnds(vertex, &path_end_visitor);
    return path_end_visitor.slack();
  }
  else
    return INF;
}

////////////////////////////////////////////////////////////////

void
Sta::reportArrivalWrtClks(const Pin *pin,
                          const Scene *scene,
                          int digits)
{
  reportDelaysWrtClks(pin, scene, digits,
                      [] (const Path *path) {
                        return path->arrival();
                      });
}

void
Sta::reportRequiredWrtClks(const Pin *pin,
                           const Scene *scene,
                           int digits)
{
  reportDelaysWrtClks(pin, scene, digits,
                      [] (const Path *path) {
                        return path->required();
                      });
}

void
Sta::reportSlackWrtClks(const Pin *pin,
                        const Scene *scene,
                        int digits)
{
  reportDelaysWrtClks(pin, scene, digits,
                      [this] (const Path *path) {
                        return path->slack(this);
                      });
}

void
Sta::reportDelaysWrtClks(const Pin *pin,
                         const Scene *scene,
                         int digits,
                         PathDelayFunc get_path_delay)
{
  ensureGraph();
  Vertex *vertex, *bidir_vertex;
  graph_->pinVertices(pin, vertex, bidir_vertex);
  if (vertex)
    reportDelaysWrtClks(vertex, scene, digits, get_path_delay);
  if (bidir_vertex)
    reportDelaysWrtClks(vertex, scene, digits, get_path_delay);
}

void
Sta::reportDelaysWrtClks(Vertex *vertex,
                         const Scene *scene,
                         int digits,
                         PathDelayFunc get_path_delay)
{
  findRequired(vertex);
  const Sdc *sdc = scene->sdc();
  reportDelaysWrtClks(vertex, nullptr, scene, digits, get_path_delay);
  const ClockEdge *default_clk_edge = sdc->defaultArrivalClock()->edge(RiseFall::rise());
  reportDelaysWrtClks(vertex, default_clk_edge, scene, digits, get_path_delay);
  for (const Clock *clk : sdc->sortedClocks()) {
    for (const RiseFall *rf : RiseFall::range()) {
      const ClockEdge *clk_edge = clk->edge(rf);
      reportDelaysWrtClks(vertex, clk_edge, scene, digits, get_path_delay);
    }
  }
}

void
Sta::reportDelaysWrtClks(Vertex *vertex,
                         const ClockEdge *clk_edge,
                         const Scene *scene,
                         int digits,
                         PathDelayFunc get_path_delay)
{
  RiseFallMinMaxDelay delays = findDelaysWrtClks(vertex, clk_edge, scene,
                                                 get_path_delay);
  if (!delays.empty()) {
    std::string clk_name;
    if (clk_edge) {
      clk_name = " (";
      clk_name += clk_edge->name();
      clk_name += ')';
    }
    report_->reportLine("%s r %s:%s f %s:%s",
                        clk_name.c_str(),
                        formatDelay(RiseFall::rise(), MinMax::min(),
                                    delays, digits).c_str(),
                        formatDelay(RiseFall::rise(), MinMax::max(),
                                    delays, digits).c_str(),
                        formatDelay(RiseFall::fall(), MinMax::min(),
                                    delays, digits).c_str(),
                        formatDelay(RiseFall::fall(), MinMax::max(),
                                    delays, digits).c_str());
  }
}

RiseFallMinMaxDelay
Sta::findDelaysWrtClks(Vertex *vertex,
                       const ClockEdge *clk_edge,
                       const Scene *scene,
                       PathDelayFunc get_path_delay)
{
  RiseFallMinMaxDelay delays;
  VertexPathIterator path_iter(vertex, scene, nullptr, nullptr, this);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Delay delay = get_path_delay(path);
    const RiseFall *rf = path->transition(this);
    const MinMax *min_max = path->minMax(this);
    const ClockEdge *path_clk_edge = path->clkEdge(this);
    if (path_clk_edge == clk_edge
        && !delayInf(delay))
      delays.mergeValue(rf, min_max, delay, this);
  }
  return delays;
}

std::string
Sta::formatDelay(const RiseFall *rf,
                 const MinMax *min_max,
                 const RiseFallMinMaxDelay &delays,
                 int digits)
{
  Delay delay;
  bool exists;
  delays.value(rf, min_max, delay, exists);
  if (exists)
    return delayAsString(delay, this, digits);
  else
    return "---";
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
  const Path *start = expanded.startPath();
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
  for (Vertex *vertex : search_->endpoints()) {
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
  if (search_->isEndpoint(vertex)
      // Need to include downstream required times if there is fanout.
      && !hasFanout(vertex, search_->searchAdj(), graph_, cmdMode()))
    search_->seedRequired(vertex);
  else
    search_->findRequireds(vertex->level());
}

Slack
Sta::totalNegativeSlack(const MinMax *min_max)
{
  searchPreamble();
  return search_->totalNegativeSlack(min_max);
}

Slack
Sta::totalNegativeSlack(const Scene *scene,
			const MinMax *min_max)
{
  searchPreamble();
  return search_->totalNegativeSlack(scene, min_max);
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
Sta::worstSlack(const Scene *scene,
		const MinMax *min_max,
		// Return values.
		Slack &worst_slack,
		Vertex *&worst_vertex)
{
  searchPreamble();
  return search_->worstSlack(scene, min_max, worst_slack, worst_vertex);
}

////////////////////////////////////////////////////////////////

string
Sta::reportDelayCalc(Edge *edge,
		     TimingArc *arc,
                     const Scene *scene,
		     const MinMax *min_max,
		     int digits)
{
  findDelays();
  return graph_delay_calc_->reportDelayCalc(edge, arc, scene, min_max, digits);
}

void
Sta::setArcDelayCalc(const char *delay_calc_name)
{
  delete arc_delay_calc_;
  arc_delay_calc_ = makeDelayCalc(delay_calc_name, sta_);
  // Update pointers to arc_delay_calc.
  updateComponentsState();
  delaysInvalid();
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
  ensureLevelized();
  for (Mode *mode : modes_)
    mode->clkNetwork()->ensureClkNetwork();
}

void
Sta::setIncrementalDelayTolerance(float tol)
{
  graph_delay_calc_->setIncrementalDelayTolerance(tol);
}

ArcDelay
Sta::arcDelay(Edge *edge,
	      TimingArc *arc,
              DcalcAPIndex ap_index)
{
  findDelays(edge->to(graph_));
  return graph_->arcDelay(edge, arc, ap_index);
}

bool
Sta::arcDelayAnnotated(Edge *edge,
		       TimingArc *arc,
                       const Scene *scene,
                       const MinMax *min_max)
{
  DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
  return graph_->arcDelayAnnotated(edge, arc, ap_index);
}

void
Sta::setArcDelayAnnotated(Edge *edge,
			  TimingArc *arc,
                          const Scene *scene,
                          const MinMax *min_max,
			  bool annotated)
{
  DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
  graph_->setArcDelayAnnotated(edge, arc, ap_index, annotated);
  Vertex *to = edge->to(graph_);
  search_->arrivalInvalid(to);
  search_->requiredInvalid(edge->from(graph_));
  if (!annotated)
    graph_delay_calc_->delayInvalid(to);
}

Slew
Sta::slew(Vertex *vertex,
          const RiseFallBoth *rf,
          const SceneSeq &scenes,
		const MinMax *min_max)
{
  findDelays(vertex);
  Slew mm_slew = min_max->initValue();
  for (const Scene *scene : scenes) {
    DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
    for (const RiseFall *rf : rf->range()) {
      Slew slew = graph_->slew(vertex, rf, ap_index);
      if (delayGreater(slew, mm_slew, min_max, this))
        mm_slew = slew;
    }
  }
  return mm_slew;
}

////////////////////////////////////////////////////////////////

// Make sure the network has been read and linked.
// Throwing an error means the caller doesn't have to check the result.
Network *
Sta::ensureLinked()
{
  if (network_ == nullptr || !network_->isLinked())
    report_->error(1570, "No network has been linked.");
  // Return cmd/sdc network.
  return cmd_network_;
}

Network *
Sta::ensureLibLinked()
{
  if (network_ == nullptr || !network_->isLinked())
    report_->error(1571, "No network has been linked.");
  // OpenROAD db is inherently linked but may not have associated
  // liberty files so check for them here.
  if (network_->defaultLibertyLibrary() == nullptr)
    report_->error(2141, "No liberty libraries found.");
  // Return cmd/sdc network.
  return cmd_network_;
}

Graph *
Sta::ensureGraph()
{
  ensureLibLinked();
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
  graph_ = new Graph(this, 2, dcalcAnalysisPtCount());
  graph_->makeGraph();
}

void
Sta::ensureLevelized()
{
  ensureGraph();
  levelize_->ensureLevelized();
}

void
Sta::updateGeneratedClks()
{
  if (update_genclks_) {
    ensureLevelized();
    for (Mode *mode : modes_) {
      Genclks *genclks = mode->genclks();
      Sdc *sdc = mode->sdc();
    bool gen_clk_changed = true;
    while (gen_clk_changed) {
      gen_clk_changed = false;
        for (Clock *clk : sdc->clocks()) {
	if (clk->isGenerated() && !clk->waveformValid()) {
            genclks->ensureMaster(clk, sdc);
	  Clock *master_clk = clk->masterClk();
	  if (master_clk && master_clk->waveformValid()) {
	    clk->generate(master_clk);
	    gen_clk_changed = true;
	  }
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

GraphLoopSeq &
Sta::graphLoops()
{
  ensureLevelized();
  return levelize_->loops();
}

Vertex *
Sta::maxPathCountVertex() const
{
  Vertex *max_vertex = nullptr;
  int max_count = 0;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    int count = vertexPathCount(vertex);
    if (count > max_count) {
      max_count = count;
      max_vertex = vertex;
    }
  }
  return max_vertex;
}

int
Sta::vertexPathCount(Vertex  *vertex) const
{
  TagGroup *tag_group = search_->tagGroup(vertex);
  if (tag_group)
    return tag_group->pathCount();
  else
    return 0;
}

int
Sta::pathCount() const
{
  int count = 0;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    count += vertexPathCount(vertex);
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
                 const Scene *scene,
		 const MinMaxAll *min_max,
		 ArcDelay delay)
{
  ensureGraph();
  for (const MinMax *mm : min_max->range()) {
    DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(mm);
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
                      const Scene *scene,
		      const MinMaxAll *min_max,
		      const RiseFallBoth *rf,
		      float slew)
{
  ensureGraph();
  for (const MinMax *mm : min_max->range()) {
    DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(mm);
    for (const RiseFall *rf1 : rf->range()) {
      graph_->setSlew(vertex, rf1, ap_index, slew);
      // Don't let delay calculation clobber the value.
      vertex->setSlewAnnotated(true, rf1, ap_index);
    }
  }
  graph_delay_calc_->delayInvalid(vertex);
}

void
Sta::writeSdf(const char *filename,
              const Scene *scene,
	      char divider,
	      bool include_typ,
              int digits,
	      bool gzip,
	      bool no_timestamp,
	      bool no_version)
{
  findDelays();
  sta::writeSdf(filename, scene, divider, include_typ, digits, gzip,
                no_timestamp, no_version, this);
}

void
Sta::removeDelaySlewAnnotations()
{
  if (graph_) {
    graph_->removeDelaySlewAnnotations();
    delaysInvalid();
  }
}

LogicValue
Sta::simLogicValue(const Pin *pin,
                   const Mode *mode)
{
  ensureGraph();
  Sim *sim = mode->sim();
  sim->ensureConstantsPropagated();
  return sim->simValue(pin);
}

////////////////////////////////////////////////////////////////

// These constants are mode/sdc independent.

void
Sta::findLogicConstants()
{
  ensureGraph();
  // Sdc independent constants so any mode should return the same values.
  Sim *sim = cmdMode()->sim();
  sim->findLogicConstants();
}

void
Sta::clearLogicConstants()
{
  Sim *sim = cmdMode()->sim();
  sim->clear();
}

////////////////////////////////////////////////////////////////

void
Sta::setPortExtPinCap(const Port *port,
		      const RiseFallBoth *rf,
		      const MinMaxAll *min_max,
                      float cap,
                      Sdc *sdc)
{
  for (const RiseFall *rf1 : rf->range()) {
    for (const MinMax *mm : min_max->range())
      sdc->setPortExtPinCap(port, rf1, mm, cap);
  }
  delaysInvalidFromFanin(port);
}

void
Sta::portExtCaps(const Port *port,
                 const MinMax *min_max,
                 const Sdc *sdc,
                 float &pin_cap,
                 float &wire_cap,
                 int &fanout)
{
  bool pin_exists = false;
  bool wire_exists = false;
  bool fanout_exists = false;
  pin_cap = min_max->initValue();
  wire_cap = min_max->initValue();
  fanout = min_max->initValueInt();
  for (const RiseFall *rf : RiseFall::range()) {
    float pin_cap1, wire_cap1;
    int fanout1;
    bool pin_exists1, wire_exists1, fanout_exists1;
    sdc->portExtCap(port, rf, min_max,
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
		       const RiseFallBoth *rf,
		       const MinMaxAll *min_max,
                       float cap,
                       Sdc *sdc)
{
  for (const RiseFall *rf1 : rf->range()) {
    for (const MinMax *mm : min_max->range())
      sdc->setPortExtWireCap(port, rf1, mm, cap);
  }
  delaysInvalidFromFanin(port);
}

void
Sta::removeNetLoadCaps(Sdc *sdc) const
{
  sdc->removeNetLoadCaps();
  delaysInvalid();
}

void
Sta::setPortExtFanout(const Port *port,
		      int fanout,
                      const MinMaxAll *min_max,
                      Sdc *sdc)
{
  for (const MinMax *mm : min_max->range())
    sdc->setPortExtFanout(port, mm, fanout);
  delaysInvalidFromFanin(port);
}

void
Sta::setNetWireCap(const Net *net,
		   bool subtract_pin_cap,
		   const MinMaxAll *min_max,
                   float cap,
                   Sdc *sdc)
{
  for (const MinMax *mm : min_max->range())
    sdc->setNetWireCap(net, subtract_pin_cap, mm, cap);
  delaysInvalidFromFanin(net);
}

void
Sta::connectedCap(const Pin *drvr_pin,
		  const RiseFall *rf,
                  const Scene *scene,
		  const MinMax *min_max,
		  float &pin_cap,
		  float &wire_cap) const
{
  graph_delay_calc_->loadCap(drvr_pin, rf, scene, min_max,
                             pin_cap, wire_cap);
}

void
Sta::connectedCap(const Net *net,
                  Scene *scene,
		  const MinMax *min_max,
		  float &pin_cap,
		  float &wire_cap) const
{
  const Pin *drvr_pin = findNetParasiticDrvrPin(net);
  if (drvr_pin) {
    pin_cap = min_max->initValue();
    wire_cap = min_max->initValue();
    for (const Scene *scene : makeSceneSeq(scene)) {
      for (const RiseFall *rf : RiseFall::range()) {
        float pin_cap1, wire_cap1;
        connectedCap(drvr_pin, rf, scene, min_max, pin_cap1, wire_cap1);
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

float
Sta::capacitance(const LibertyPort *port,
                 Scene *scene,
                 const MinMax *min_max)
{
  float cap = min_max->initValue();
  for (const Scene *scene : makeSceneSeq(scene)) {
    const Sdc *sdc = scene->sdc();
    OperatingConditions *op_cond = operatingConditions(min_max, sdc);
    const LibertyPort *scene_port = port->scenePort(scene, min_max);
    for (const RiseFall *rf : RiseFall::range())
      cap = min_max->minMax(cap, scene_port->capacitance(rf, min_max, op_cond, op_cond));
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
                   float res,
                   Sdc *sdc)
{
  sdc->setResistance(net, min_max, res);
}

////////////////////////////////////////////////////////////////

bool
Sta::readSpef(const std::string &name,
              const std::string &filename,
	      Instance *instance,
              Scene *scene,   // -scene deprecated 11/20/2025
	      const MinMaxAll *min_max,
	      bool pin_cap_included,
	      bool keep_coupling_caps,
	      float coupling_cap_factor,
	      bool reduce)
{
  ensureLibLinked();
  Parasitics *parasitics = nullptr;
  // Use -name to distinguish rel 2.7 args for compatibility.
  if (name.empty()) {
    std::string spef_name = "default";
    if (scene
        || min_max != MinMaxAll::minMax()) {
      if (scene)
        spef_name = scene->name();
      if (min_max != MinMaxAll::minMax()) {
        spef_name += "_";
        spef_name += min_max->to_string();
      }
      parasitics = makeConcreteParasitics(spef_name, filename);
    }
    else
      parasitics = findParasitics(spef_name);
    if (scene)
      scene->setParasitics(parasitics, min_max);
    else {
      parasitics = findParasitics(spef_name);
      for (Scene *scene : scenes_)
        scene->setParasitics(parasitics, min_max);
    }
  }
  else {
    parasitics = findParasitics(name);
    if (parasitics == nullptr)
      parasitics = makeConcreteParasitics(name, filename);
  }

  bool success = readSpefFile(filename.c_str(), instance,
			      pin_cap_included, keep_coupling_caps,
                              coupling_cap_factor, reduce,
                              scene, min_max, parasitics, this);
  delaysInvalid();
  return success;
}

Parasitics *
Sta::findParasitics(const std::string &name)
{
  return findKey(parasitics_name_map_, name);
}

void
Sta::reportParasiticAnnotation(const string &spef_name,
                               bool report_unannotated)
{
  ensureLibLinked();
  ensureGraph();
  Parasitics *parasitics = nullptr;
  if (!spef_name.empty()) {
    parasitics = findParasitics(spef_name);
    if (parasitics == nullptr)
      report_->error(1560, "spef %s not found.", spef_name.c_str());
  }
  else
    parasitics = cmd_scene_->parasitics(MinMax::max());
  sta::reportParasiticAnnotation(parasitics, report_unannotated,
                                 cmd_scene_, this);
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
  Scene *scene = cmd_scene_;
  const Parasitics *parasitics = scene->parasitics(min_max);
  Parasitic *pi_elmore = parasitics->findPiElmore(drvr_pin, rf, min_max);
  if (pi_elmore) {
    parasitics->piModel(pi_elmore, c2, rpi, c1);
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
  const Scene *scene = cmd_scene_;
  for (const MinMax *mm : min_max->range()) {
    Parasitics *parasitics = scene->parasitics(mm);
    parasitics->makePiElmore(drvr_pin, rf, mm, c2, rpi, c1);
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
  Scene *scene = cmd_scene_;
  const Parasitics *parasitics = scene->parasitics(min_max);
  Parasitic *pi_elmore = parasitics->findPiElmore(drvr_pin, rf, min_max);
  if (pi_elmore)
    parasitics->findElmore(pi_elmore, load_pin, elmore, exists);
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
  const Scene *scene = cmd_scene_;
  for (const MinMax *mm : min_max->range()) {
    Parasitics *parasitics = scene->parasitics(mm);
    Parasitic *pi_elmore = parasitics->findPiElmore(drvr_pin, rf, mm);
    if (pi_elmore)
      parasitics->setElmore(pi_elmore, load_pin, elmore);
  }
  delaysInvalidFrom(drvr_pin);
}

void
Sta::deleteParasitics()
{
  Parasitics *parasitics_default = findParasitics("default");
  for (auto [name, parasitics] : parasitics_name_map_) {
    if (parasitics != parasitics_default)
      delete parasitics;
  }
  parasitics_name_map_.clear();

  parasitics_name_map_[parasitics_default->name()] = parasitics_default;
  parasitics_default->clear();

  for (Scene *scene : scenes_)
    scene->setParasitics(parasitics_default, MinMaxAll::minMax());

  delaysInvalid();
}

Parasitics *
Sta::makeConcreteParasitics(std::string name,
                            std::string filename)
{
  Parasitics *parasitics = new ConcreteParasitics(name, filename, this);
  parasitics_name_map_[name] = parasitics;
  return parasitics;
}

Parasitic *
Sta::makeParasiticNetwork(const Net *net,
                          bool includes_pin_caps,
                          const Scene *scene,
                          const MinMax *min_max)
{
  Parasitics *parasitics = scene->parasitics(min_max);
  Parasitic *parasitic = parasitics->makeParasiticNetwork(net, includes_pin_caps);
  delaysInvalidFromFanin(net);
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
  if (sta::equivCellsArcs(from_lib_cell, to_lib_cell)) {
    // Replace celll optimized for less disruption to graph
    // when ports and timing arcs are equivalent.
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

void
Sta::makePortPin(const char *port_name,
                 PortDirection *dir)
{
  ensureLinked();
  NetworkReader *network = dynamic_cast<NetworkReader*>(network_);
  Instance *top_inst = network->topInstance();
  Cell *top_cell = network->cell(top_inst);
  Port *port = network->makePort(top_cell, port_name);
  network->setDirection(port, dir);
  Pin *pin = network->makePin(top_inst, port, nullptr);
  makePortPinAfter(pin);
}
 
////////////////////////////////////////////////////////////////
//
// Network edit before/after methods.
//
////////////////////////////////////////////////////////////////

void
Sta::makeInstanceAfter(const Instance *inst)
{
  debugPrint(debug_, "network_edit", 1, "make instance %s",
             sdc_network_->pathName(inst));
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
      power_->powerInvalid();
    }
  }
}

void
Sta::makePortPinAfter(Pin *pin)
{
  if (graph_) {
    Vertex *vertex, *bidir_drvr_vertex;
    graph_->makePinVertices(pin, vertex, bidir_drvr_vertex);
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
                report_->critical(1556, "corresponding timing arc set not found in equiv cells");
            }
          }
        }
        else {
          // Force delay calculation on output pins.
          Vertex *vertex = graph_->pinDrvrVertex(pin);
	  if (vertex)
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
      if (network_->direction(pin)->isAnyInput()) {
        for (auto [name, parasitics] : parasitics_name_map_)
          parasitics->loadPinCapacitanceChanged(pin);
      }
    }
    delete pin_iter;
    clk_skews_->clear();
    power_->powerInvalid();
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
  for (Mode *mode : modes_) {
    Sdc *sdc = mode->sdc();
    for (Clock *clk : sdc->clocks()) {
    if (clk->isPropagated())
      return false;
  }
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
      for (const Mode *mode : modes_)
        mode->sim()->pinSetFuncAfter(pin);
      if (network_->direction(pin)->isAnyInput()) {
        for (auto [name, parasitics] : parasitics_name_map_)
          parasitics->loadPinCapacitanceChanged(pin);
      }
    }
    delete pin_iter;
  }
}

void
Sta::connectPinAfter(const Pin *pin)
{
  debugPrint(debug_, "network_edit", 1, "connect %s to %s",
             sdc_network_->pathName(pin),
             sdc_network_->pathName(network_->net(pin)));
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
  for (Mode *mode : modes_) {
    mode->sdc()->connectPinAfter(pin);
    mode->sim()->connectPinAfter(pin);
  }
  clk_skews_->clear();
  power_->powerInvalid();
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
    for (Mode *mode : modes_)
      mode->sdc()->clkHpinDisablesChanged(to_vertex->pin());
  }
  Pin *pin = vertex->pin();
  for (Mode *mode : modes_) {
    mode->sdc()->clkHpinDisablesChanged(pin);
    mode->clkNetwork()->connectPinAfter(pin);
  }
  graph_delay_calc_->delayInvalid(vertex);
  search_->requiredInvalid(vertex);
  search_->endpointInvalid(vertex);
  levelize_->relevelizeFrom(vertex);
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
    for (Mode *mode : modes_)
      mode->sdc()->clkHpinDisablesChanged(from_vertex->pin());
    levelize_->relevelizeFrom(from_vertex);
  }
  Pin *pin = vertex->pin();
  for (Mode *mode : modes_) {
    mode->sdc()->clkHpinDisablesChanged(pin);
    mode->clkNetwork()->connectPinAfter(pin);
  }
  graph_delay_calc_->delayInvalid(vertex);
  search_->arrivalInvalid(vertex);
  search_->endpointInvalid(vertex);
}

void
Sta::disconnectPinBefore(const Pin *pin)
{
  debugPrint(debug_, "network_edit", 1, "disconnect %s from %s",
             sdc_network_->pathName(pin),
             sdc_network_->pathName(network_->net(pin)));

  for (auto [name, parasitics] : parasitics_name_map_)
    parasitics->disconnectPinBefore(pin);
  bool is_hierarchical = network_->isHierarchical(pin);
  for (Mode *mode : modes_) {
    mode->sim()->disconnectPinBefore(pin);
    if (!is_hierarchical)
      mode->clkNetwork()->disconnectPinBefore(pin);
  }

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
      }
    }
    if (is_hierarchical) {
      // Delete wire edges thru pin.
      EdgesThruHierPinIterator edge_iter(pin, network_, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	if (edge->role()->isWire()) {
	  deleteEdge(edge);
          const Pin *from_pin = edge->from(graph_)->pin();
          for (Mode *mode : modes_)
            mode->clkNetwork()->disconnectPinBefore(from_pin);
	}
      }
    }
    clk_skews_->clear();
    power_->powerInvalid();
  }
}

void
Sta::deleteEdge(Edge *edge)
{
  debugPrint(debug_, "network_edit", 2, "delete edge %s -> %s",
             edge->from(graph_)->name(sdc_network_),
             edge->to(graph_)->name(sdc_network_));
  Vertex *to = edge->to(graph_);
  search_->deleteEdgeBefore(edge);
  graph_delay_calc_->delayInvalid(to);
  levelize_->relevelizeFrom(to);
  levelize_->deleteEdgeBefore(edge);
  for (Mode *mode : modes_)
    mode->sdc()->clkHpinDisablesChanged(edge->from(graph_)->pin());
  graph_->deleteEdge(edge);
}

void
Sta::deleteNetBefore(const Net *net)
{
  debugPrint(debug_, "network_edit", 1, "delete net %s",
             sdc_network_->pathName(net));
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
  for (Mode *mode : modes_)
    mode->sdc()->deleteNetBefore(net);
  clk_skews_->clear();
  power_->powerInvalid();
}

void
Sta::deleteInstanceBefore(const Instance *inst)
{
  debugPrint(debug_, "network_edit", 1, "delete instance %s",
             sdc_network_->pathName(inst));
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
  for (Mode *mode : modes_) {
    mode->sim()->deleteInstanceBefore(inst);
    mode->sdc()->deleteInstanceBefore(inst);
  }
  clk_skews_->clear();
  power_->powerInvalid();
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
    debugPrint(debug_, "network_edit", 1, "delete pin %s",
	       sdc_network_->pathName(pin));
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
            // Only notify to_vertex (from_vertex will be deleted).
            search_->requiredInvalid(from);
          }
          levelize_->deleteEdgeBefore(edge);
        }
	// Deletes edges to/from vertex also.
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
            search_->arrivalInvalid(to);
            graph_delay_calc_->delayInvalid(to);
            levelize_->relevelizeFrom(to);
          }
          levelize_->deleteEdgeBefore(edge);
        }
	// Deletes edges to/from vertex also.
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

  for (const Mode *mode : modes_) {
    mode->sdc()->deletePinBefore(pin);
    mode->sim()->deletePinBefore(pin);
    mode->clkNetwork()->deletePinBefore(pin);
  }
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
Sta::clocks(const Pin *pin,
            const Mode *mode)
{
  ensureClkArrivals();
  return search_->clocks(pin, mode);
}

ClockSet
Sta::clockDomains(const Pin *pin,
                  const Mode *mode)
{
  searchPreamble();
  search_->findAllArrivals();
  return search_->clockDomains(pin, mode);
}

////////////////////////////////////////////////////////////////

InstanceSet
Sta::findRegisterInstances(ClockSet *clks,
			   const RiseFallBoth *clk_rf,
			   bool edge_triggered,
                           bool latches,
                           const Mode *mode)
{
  findRegisterPreamble(mode);
  return findRegInstances(clks, clk_rf, edge_triggered, latches,
                          mode, this);
}

PinSet
Sta::findRegisterDataPins(ClockSet *clks,
			  const RiseFallBoth *clk_rf,
			  bool edge_triggered,
                          bool latches,
                          const Mode *mode)
{
  findRegisterPreamble(mode);
  return findRegDataPins(clks, clk_rf, edge_triggered, latches,
                         mode, this);
}

PinSet
Sta::findRegisterClkPins(ClockSet *clks,
			 const RiseFallBoth *clk_rf,
			 bool edge_triggered,
                         bool latches,
                         const Mode *mode)
{
  findRegisterPreamble(mode);
  return findRegClkPins(clks, clk_rf, edge_triggered, latches,
                        mode, this);
}

PinSet
Sta::findRegisterAsyncPins(ClockSet *clks,
			   const RiseFallBoth *clk_rf,
			   bool edge_triggered,
                           bool latches,
                           const Mode *mode)
{
  findRegisterPreamble(mode);
  return findRegAsyncPins(clks, clk_rf, edge_triggered, latches,
                          mode, this);
}

PinSet
Sta::findRegisterOutputPins(ClockSet *clks,
			    const RiseFallBoth *clk_rf,
			    bool edge_triggered,
                            bool latches,
                            const Mode *mode)
{
  findRegisterPreamble(mode);
  return findRegOutputPins(clks, clk_rf, edge_triggered, latches,
                           mode, this);
}

void
Sta::findRegisterPreamble(const Mode *mode)
{
  ensureLibLinked();
  ensureGraph();
  mode->sim()->ensureConstantsPropagated();
}

////////////////////////////////////////////////////////////////

class FanInOutSrchPred : public SearchPred
{
public:
  FanInOutSrchPred(bool thru_disabled,
		   bool thru_constants,
		   const StaState *sta);
  bool searchFrom(const Vertex *from_vertex,
                  const Mode *mode) const override;
  bool searchThru(Edge *edge,
                  const Mode *mode) const override;
  bool searchTo(const Vertex *to_vertex,
                const Mode *mode) const override;

protected:
  bool crossesHierarchy(Edge *edge);
  virtual bool searchThruRole(Edge *edge) const;

  bool thru_disabled_;
  bool thru_constants_;
  const StaState *sta_;
};

FanInOutSrchPred::FanInOutSrchPred(bool thru_disabled,
				   bool thru_constants,
				   const StaState *sta) :
  SearchPred(sta),
  thru_disabled_(thru_disabled),
  thru_constants_(thru_constants),
  sta_(sta)
{
}

bool
FanInOutSrchPred::searchFrom(const Vertex *from_vertex,
                             const Mode *mode) const
{
  const Pin *from_pin = from_vertex->pin();
  const Sdc *sdc = mode->sdc();
  return (thru_disabled_
          || !sdc->isDisabledConstraint(from_pin))
    && (thru_constants_
        || !mode->sim()->isConstant(from_vertex));
}

bool
FanInOutSrchPred::searchThru(Edge *edge,
                             const Mode *mode) const
{
  const Sdc *sdc = mode->sdc();
  const Sim *sim = mode->sim();
  return searchThruRole(edge)
    && (thru_disabled_
        || !(sdc->isDisabledConstraint(edge)
             || sim->isDisabledCond(edge)
	     || sta_->isDisabledCondDefault(edge)))
    && (thru_constants_
        || sim->simTimingSense(edge) != TimingSense::none);
}

bool
FanInOutSrchPred::searchThruRole(Edge *edge) const
{
  const TimingRole *role = edge->role();
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
FanInOutSrchPred::searchTo(const Vertex *to_vertex,
                           const Mode *mode) const
{
  const Pin *to_pin = to_vertex->pin();
  const Sdc *sdc = mode->sdc();
  return (thru_disabled_
          || !sdc->isDisabledConstraint(to_pin))
    && (thru_constants_
        || !mode->sim()->isConstant(to_vertex));
}

class FaninSrchPred : public FanInOutSrchPred
{
public:
  FaninSrchPred(bool thru_disabled,
		bool thru_constants,
		const StaState *sta);

protected:
  bool searchThruRole(Edge *edge) const override;
};

FaninSrchPred::FaninSrchPred(bool thru_disabled,
			     bool thru_constants,
			     const StaState *sta) :
  FanInOutSrchPred(thru_disabled, thru_constants, sta)
{
}

bool
FaninSrchPred::searchThruRole(Edge *edge) const
{
  const TimingRole *role = edge->role();
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
                   bool thru_constants,
                   const Mode *mode)
{
  ensureGraph();
  ensureLevelized();
  mode->sim()->ensureConstantsPropagated();
  PinSet fanin(network_);
  FaninSrchPred pred(thru_disabled, thru_constants, this);
  for (const Pin *pin : *to) {
    if (network_->isHierarchical(pin)) {
      EdgesThruHierPinIterator edge_iter(pin, network_, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	findFaninPins(edge->from(graph_), flat, startpoints_only,
                      inst_levels, pin_levels, fanin, pred, mode);
      }
    }
    else {
      Vertex *vertex = graph_->pinLoadVertex(pin);
      findFaninPins(vertex, flat, startpoints_only,
                    inst_levels, pin_levels, fanin, pred, mode);
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
                   SearchPred &pred,
                   const Mode *mode)
{
  VertexSet visited = makeVertexSet(this);
  findFaninPins(vertex, flat, inst_levels,
                pin_levels, visited, &pred, 0, 0, mode);
  for (Vertex *visited_vertex : visited) {
    Pin *visited_pin = visited_vertex->pin();
    if (!startpoints_only
	|| network_->isRegClkPin(visited_pin)
        || !hasFanin(visited_vertex, &pred, graph_, mode))
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
                   int pin_level,
                   const Mode *mode)
{
  debugPrint(debug_, "fanin", 1, "%s",
             to->to_string(this).c_str());
  if (!visited.contains(to)) {
    visited.insert(to);
    Pin *to_pin = to->pin();
    bool is_reg_clk_pin = network_->isRegClkPin(to_pin);
    if (!is_reg_clk_pin
	&& (inst_levels <= 0
	    || inst_level < inst_levels)
	&& (pin_levels <= 0
	    || pin_level < pin_levels)
        && pred->searchTo(to, mode)) {
      VertexInEdgeIterator edge_iter(to, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	Vertex *from_vertex = edge->from(graph_);
        if (pred->searchThru(edge, mode)
	    && (flat
		|| !crossesHierarchy(edge))
            && pred->searchFrom(from_vertex, mode)) {
	  findFaninPins(from_vertex, flat, inst_levels,
			pin_levels, visited, pred,
			edge->role()->isWire() ? inst_level : inst_level+1,
                        pin_level+1, mode);
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
                        bool thru_constants,
                        const Mode *mode)
{
  PinSet pins = findFaninPins(to, flat, startpoints_only, inst_levels,
                              pin_levels, thru_disabled, thru_constants, mode);
  return pinInstances(pins, network_);
}

PinSet
Sta::findFanoutPins(PinSeq *from,
		    bool flat,
		    bool endpoints_only,
		    int inst_levels,
		    int pin_levels,
		    bool thru_disabled,
                    bool thru_constants,
                    const Mode *mode)
{
  ensureGraph();
  ensureLevelized();
  mode->sim()->ensureConstantsPropagated();
  PinSet fanout(network_);
  FanInOutSrchPred pred(thru_disabled, thru_constants, this);
  for (const Pin *pin : *from) {
    if (network_->isHierarchical(pin)) {
      EdgesThruHierPinIterator edge_iter(pin, network_, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	findFanoutPins(edge->to(graph_), flat, endpoints_only,
                       inst_levels, pin_levels, fanout, pred, mode);
      }
    }
    else {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      findFanoutPins(vertex, flat, endpoints_only,
                     inst_levels, pin_levels, fanout, pred, mode);
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
                    SearchPred &pred,
                    const Mode *mode)
{
  VertexSet visited = makeVertexSet(this);
  findFanoutPins(vertex, flat, inst_levels,
                 pin_levels, visited, &pred, 0, 0, mode);
  for (Vertex *visited_vertex : visited) {
    Pin *visited_pin = visited_vertex->pin();
    if (!endpoints_only
        || search_->isEndpoint(visited_vertex, &pred, mode))
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
                    int pin_level,
                    const Mode *mode)
{
  debugPrint(debug_, "fanout", 1, "%s",
             from->to_string(this).c_str());
  if (!visited.contains(from)) {
    visited.insert(from);
    if (!search_->isEndpoint(from, pred, mode)
	&& (inst_levels <= 0
	    || inst_level < inst_levels)
	&& (pin_levels <= 0
	    || pin_level < pin_levels)
        && pred->searchFrom(from, mode)) {
      VertexOutEdgeIterator edge_iter(from, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	Vertex *to_vertex = edge->to(graph_);
        if (pred->searchThru(edge, mode)
	    && (flat
		|| !crossesHierarchy(edge))
            && pred->searchTo(to_vertex, mode)) {
	  findFanoutPins(to_vertex, flat, inst_levels,
			 pin_levels, visited, pred,
			 edge->role()->isWire() ? inst_level : inst_level+1,
                         pin_level+1, mode);
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
                         bool thru_constants,
                         const Mode *mode)
{
  PinSet pins = findFanoutPins(from, flat, endpoints_only, inst_levels,
                               pin_levels, thru_disabled, thru_constants, mode);
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

static Slew
instMaxSlew(const Instance *inst,
            Sta *sta)
{
  Network *network = sta->network();
  Graph *graph = sta->graph();
  Slew max_slew = 0.0;
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network->isDriver(pin)) {
      Vertex *vertex = graph->pinDrvrVertex(pin);
      Slew slew = sta->slew(vertex, RiseFallBoth::riseFall(),
                            sta->scenes(), MinMax::max());
      if (delayGreater(slew, max_slew, sta))
        max_slew = slew;
    }
  }
  delete pin_iter;
  return max_slew;
}

InstanceSeq
Sta::slowDrivers(int count)
{
  findDelays();
  InstanceSeq insts = network_->leafInstances();
  sort(insts, [this] (const Instance *inst1,
                      const Instance *inst2) {
    return delayGreater(instMaxSlew(inst1, this),
                        instMaxSlew(inst2, this),
                        this);
  });
  insts.resize(count);
  return insts;
}

////////////////////////////////////////////////////////////////

void
Sta::reportSlewChecks(const Net *net,
                      size_t max_count,
                      bool violators,
                      bool verbose,
                      const SceneSeq &scenes,
                      const MinMax *min_max)
{
  checkSlewsPreamble();
  SlewCheckSeq &checks = check_slews_->check(net, max_count, violators, scenes, min_max);
  if (!checks.empty()) {
    report_->reportLine("%s slew", min_max->to_string().c_str());
    report_->reportLine("");

    if (!verbose)
      report_path_->reportLimitShortHeader(report_path_->fieldSlew());
    for (SlewCheck &check : checks) {
      if (verbose)
        report_path_->reportLimitVerbose(report_path_->fieldSlew(),
                                         check.pin(), check.edge(),
                                         delayAsFloat(check.slew()),
                                         check.limit(), check.slack(),
                                         check.scene(), min_max);
      else
        report_path_->reportLimitShort(report_path_->fieldSlew(), check.pin(),
                                       delayAsFloat(check.slew()),
                                       check.limit(), check.slack());
    }
    report_->reportLine("");
  }
}

void
Sta::checkSlewsPreamble()
{
  ensureLevelized();
  bool have_clk_slew_limits = false;
  for (Mode *mode : modes_) {
    if (mode->sdc()->haveClkSlewLimits())
      have_clk_slew_limits = true;
    mode->clkNetwork()->ensureClkNetwork();
 }
  if (have_clk_slew_limits)
    // Arrivals are needed to know pin clock domains.
    updateTiming(false);
  else
    findDelays();
  if (check_slews_ == nullptr)
    makeCheckSlews();
}

void
Sta::checkSlew(const Pin *pin,
               const SceneSeq &scenes,
	       const MinMax *min_max,
	       bool check_clks,
	       // Return values.
	       Slew &slew,
	       float &limit,
               float &slack,
               const RiseFall *&rf,
               const Scene *&scene)
{
  check_slews_->check(pin, scenes, min_max, check_clks,
                      slew, limit, slack, rf, scene);
}

size_t
Sta::maxSlewViolationCount()
{
  checkSlewsPreamble();
  return check_slews_->check(nullptr, 1, true, scenes_, MinMax::max()).size();
}

void
Sta::maxSlewCheck(// Return values.
                  const Pin *&pin,
                  Slew &slew,
                  float &slack,
                  float &limit)
{
  checkSlewsPreamble();
  SlewCheckSeq &checks = check_slews_->check(nullptr, 1, false, scenes_, MinMax::max());
  if (!checks.empty()) {
    SlewCheck &check = checks[0];
    pin = check.pin();
    slew = check.slew();
    slack = check.slack();
    limit = check.limit();
  }
  else {
  pin = nullptr;
  slew = 0.0;
  slack = INF;
  }
}

void
Sta::findSlewLimit(const LibertyPort *port,
                   const Scene *scene,
                   const MinMax *min_max,
                   // Return values.
                   float &limit,
                   bool &exists)
{
  if (check_slews_ == nullptr)
    makeCheckSlews();
  check_slews_->findLimit(port, scene, min_max,
                                limit, exists);
}

////////////////////////////////////////////////////////////////'

void
Sta::reportFanoutChecks(const Net *net,
                        size_t max_count,
                       bool violators,
                        bool verbose,
                        const SceneSeq &scenes,
                       const MinMax *min_max)
{
  checkFanoutPreamble();
  const ModeSeq modes = Scene::modesSorted(scenes); 
  FanoutCheckSeq &checks = check_fanouts_->check(net, max_count, violators,
                                                 modes, min_max);
  if (!checks.empty()) {
    report_->reportLine("%s fanout", min_max->to_string().c_str());
    report_->reportLine("");

    if (!verbose)
  report_path_->reportLimitShortHeader(report_path_->fieldFanout());

    for (const FanoutCheck &check : checks) {
      if (verbose)
  report_path_->reportLimitVerbose(report_path_->fieldFanout(),
                                         check.pin(), nullptr, check.fanout(),
                                         check.limit(), check.slack(), nullptr,
                                         min_max);
      else
        report_path_->reportLimitShort(report_path_->fieldFanout(),
                                       check.pin(), check.fanout(),
                                       check.limit(), check.slack());
    }
    report_->reportLine("");
  }
}

void
Sta::checkFanoutPreamble()
{
  if (check_fanouts_ == nullptr)
    makeCheckFanouts();
  ensureLevelized();
  for (Mode *mode : modes_) {
    mode->sim()->ensureConstantsPropagated();
    mode->clkNetwork()->ensureClkNetwork();
  }
}

size_t
Sta::fanoutViolationCount(const MinMax *min_max,
                          const ModeSeq &modes)
{
  FanoutCheckSeq &checks = check_fanouts_->check(nullptr, 0, true, modes, min_max);
  return checks.size();
}

void
Sta::checkFanout(const Pin *pin,
                 const Mode *mode,
		 const MinMax *min_max,
		 // Return values.
		 float &fanout,
		 float &limit,
		 float &slack)
{
  FanoutCheck check = check_fanouts_->check(pin, mode, min_max);
  pin = check.pin();
  fanout = check.fanout();
  limit = check.limit();
  slack = check.slack();
}

void
Sta::maxFanoutMinSlackPin(const ModeSeq &modes,
                          // Return values.
                    const Pin *&pin,
                    float &fanout,
                          float &limit,
                    float &slack,
                          const Mode *&mode)
{
  checkFanoutPreamble();
  FanoutCheckSeq &checks = check_fanouts_->check(nullptr, 1, false,
                                                 modes, MinMax::max());
  if (!checks.empty()) {
    FanoutCheck &check = checks[0];
    pin = check.pin();
    fanout = check.fanout();
    limit = check.limit();
    slack = check.slack();
    mode = check.mode();
  }
  else {
  pin = nullptr;
  fanout = 0;
  limit = INF;
    slack = INF;
    mode = nullptr;
  }
}

////////////////////////////////////////////////////////////////'

void
Sta::reportCapacitanceChecks(const Net *net,
                             size_t max_count,
                            bool violators,
                             bool verbose,
                             const SceneSeq &scenes,
                            const MinMax *min_max)
{
  checkCapacitancesPreamble(scenes);
  CapacitanceCheckSeq &checks = check_capacitances_->check(net, max_count,
                                                           violators,
                                                           scenes, min_max);
  if (!checks.empty()) {
    report_->reportLine("%s capacitance", min_max->to_string().c_str());
    report_->reportLine("");

    if (!verbose)
  report_path_->reportLimitShortHeader(report_path_->fieldCapacitance());
    for (CapacitanceCheck &check : checks) {
      if (verbose)
  report_path_->reportLimitVerbose(report_path_->fieldCapacitance(),
                                         check.pin(), check.rf(), check.capacitance(),
                                         check.limit(), check.slack(), check.scene(),
                                         min_max);
      else
        report_path_->reportLimitShort(report_path_->fieldCapacitance(),
                                     check.pin(), check.capacitance(),
                                     check.limit(), check.slack());
      report_->reportLine("");
    }
  }
}

void
Sta::checkCapacitancesPreamble(const SceneSeq &scenes)
{
  ensureLevelized();
  if (check_capacitances_ == nullptr)
    makeCheckCapacitances();
  for (Mode *mode : Scene::modes(scenes)) {
    mode->sim()->ensureConstantsPropagated();
    mode->clkNetwork()->ensureClkNetwork();
  }
}

void
Sta::checkCapacitance(const Pin *pin,
                      const SceneSeq &scenes,
		      const MinMax *min_max,
		      // Return values.
		      float &capacitance,
		      float &limit,
                      float &slack,
                      const RiseFall *&rf,
                      const Scene *&scene)
{
  CapacitanceCheck check = check_capacitances_->check(pin, scenes, min_max);
  pin = check.pin();
  capacitance = check.capacitance();
  limit = check.limit();
  slack = check.slack();
  rf = check.rf();
  scene = check.scene();
}

size_t
Sta::maxCapacitanceViolationCount()
{
  checkCapacitancesPreamble(scenes_);
  return check_capacitances_->check(nullptr, 1, true, scenes_,
                                    MinMax::max()).size();
}

void
Sta::maxCapacitanceCheck(// Return values.
                         const Pin *&pin,
                         float &capacitance,
                         float &slack,
                         float &limit)
{
  checkCapacitancesPreamble(scenes_);
  CapacitanceCheckSeq &checks = check_capacitances_->check(nullptr, 1, false,
                                                           scenes_,
                                                                  MinMax::max());
  pin = nullptr;
  capacitance = 0.0;
  slack = INF;
  limit = INF;
  if (!checks.empty()) {
    const CapacitanceCheck &check = checks[0];
    pin = check.pin();
    capacitance = check.capacitance();
    limit = check.limit();
    slack = check.slack();
  }
}

////////////////////////////////////////////////////////////////

void
Sta::reportMinPulseWidthChecks(const Net *net,
                               size_t max_count,
                               bool violators,
                               bool verbose,
                               const SceneSeq &scenes)
{
  ensureClkArrivals();
  if (check_min_pulse_widths_ == nullptr)
    makeCheckMinPulseWidths();
  MinPulseWidthCheckSeq &checks =
    check_min_pulse_widths_->check(net, max_count, violators, scenes);
  report_path_->reportMpwChecks(checks, verbose);
}

////////////////////////////////////////////////////////////////

void
Sta::reportMinPeriodChecks(const Net *net,
                           size_t max_count,
                           bool violators,
                           bool verbose,
                           const SceneSeq &scenes)
{
  // Need clk arrivals to know what clks arrive at the clk tree endpoints.
  ensureClkArrivals();
  if (check_min_periods_ == nullptr)
    makeCheckMinPeriods();
  MinPeriodCheckSeq &checks = check_min_periods_->check(net, max_count,
                                                        violators, scenes);
  report_path_->reportChecks(checks, verbose);
}

////////////////////////////////////////////////////////////////

void
Sta::reportMaxSkewChecks(const Net *net,
                         size_t max_count,
                         bool violators,
                         bool verbose,
                         const SceneSeq &scenes)
{
  maxSkewPreamble();
  MaxSkewCheckSeq &checks = check_max_skews_->check(net, max_count, violators, scenes);
  report_path_->reportChecks(checks, verbose);
}

void
Sta::maxSkewPreamble()
{
  ensureClkArrivals();
  if (check_max_skews_ == nullptr)
    makeCheckMaxSkews();
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
                      const Scene *scene)
{
  ensureLibLinked();
  ensureGraph();
  LibertyLibrary *library = makeTimingModel(lib_name, cell_name, filename,
                                            scene, this);
  writeLiberty(library, filename, this);
}

////////////////////////////////////////////////////////////////

void
Sta::reportPowerDesign(const Scene *scene,
                       int digits)
{
  powerPreamble(scene);
  power_->reportDesign(scene, digits);
}

void
Sta::reportPowerInsts(const InstanceSeq &insts,
                      const Scene *scene,
                      int digits)
{
  powerPreamble(scene);
  power_->reportInsts(insts, scene, digits);
}

void
Sta::reportPowerHighestInsts(size_t count,
                             const Scene *scene,
                             int digits)
{
  powerPreamble(scene);
  power_->reportHighestInsts(count, scene, digits);
}

void
Sta::reportPowerDesignJson(const Scene *scene,
                           int digits)
{
  powerPreamble(scene);
  power_->reportDesignJson(scene, digits);
}

void
Sta::reportPowerInstsJson(const InstanceSeq &insts,
                          const Scene *scene,
                          int digits)
{
  powerPreamble(scene);
  power_->reportInstsJson(insts, scene, digits);
}

void
Sta::powerPreamble(const Scene *scene)
{
  ensureLibLinked();
  // Use arrivals to find clocking info.
  searchPreamble();
  search_->findAllArrivals();
  if (scene)
    scene->mode()->clkNetwork()->ensureClkNetwork();
  else {
    for (Mode *mode : modes_)
      mode->clkNetwork()->ensureClkNetwork();
  }
}

void
Sta::power(const Scene *scene,
	   // Return values.
	   PowerResult &total,
	   PowerResult &sequential,
	   PowerResult &combinational,
	   PowerResult &clock,
	   PowerResult &macro,
	   PowerResult &pad)
{
  powerPreamble(scene);
  power_->power(scene, total, sequential, combinational, clock, macro, pad);
}

PowerResult
Sta::power(const Instance *inst,
           const Scene *scene)
{
  powerPreamble(scene);
  return power_->power(inst, scene);
}

PwrActivity
Sta::activity(const Pin *pin,
              const Scene *scene)
{
  powerPreamble(scene);
  return power_->pinActivity(pin, scene);
}

////////////////////////////////////////////////////////////////

void
Sta::writePathSpice(Path *path,
                    const char *spice_filename,
                    const char *subckt_filename,
                    const char *lib_subckt_filename,
                    const char *model_filename,
                    const char *power_name,
                    const char *gnd_name,
                    CircuitSim ckt_sim)
{
  ensureLibLinked();
  sta::writePathSpice(path, spice_filename, subckt_filename,
                      lib_subckt_filename, model_filename,
                      power_name, gnd_name, ckt_sim, this);
}

////////////////////////////////////////////////////////////////

void
Sta::ensureClkNetwork(const Mode *mode)
{
  ensureLevelized();
  mode->clkNetwork()->ensureClkNetwork();
}

bool
Sta::isClock(const Pin *pin,
             const Mode *mode) const
{
  return mode->clkNetwork()->isClock(pin);
}

bool
Sta::isClock(const Net *net,
             const Mode *mode) const
{
  return mode->clkNetwork()->isClock(net);
}

bool
Sta::isIdealClock(const Pin *pin,
                  const Mode *mode) const
{
  return mode->clkNetwork()->isIdealClock(pin);
}

bool
Sta::isPropagatedClock(const Pin *pin,
                       const Mode *mode) const
{
  return mode->clkNetwork()->isPropagatedClock(pin);
}

const PinSet *
Sta::pins(const Clock *clk,
          const Mode *mode)
{
  return mode->clkNetwork()->pins(clk);
}

void
Sta::clkPinsInvalid(const Mode *mode)
{
  mode->clkNetwork()->clkPinsInvalid();
}

} // namespace
