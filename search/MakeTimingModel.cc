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

#include "MakeTimingModel.hh"
#include "MakeTimingModelPvt.hh"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ArcDelayCalc.hh"
#include "ClkDelays.hh"
#include "Clock.hh"
#include "ContainerHelpers.hh"
#include "Debug.hh"
#include "Delay.hh"
#include "DelayScalar.hh"
#include "DispatchQueue.hh"
#include "ExceptionPath.hh"
#include "FuncExpr.hh"
#include "Fuzzy.hh"
#include "Graph.hh"
#include "GraphClass.hh"
#include "GraphDelayCalc.hh"
#include "Liberty.hh"
#include "LibertyClass.hh"
#include "Mode.hh"
#include "Network.hh"
#include "NetworkClass.hh"
#include "Path.hh"
#include "PathEnd.hh"
#include "PortDirection.hh"
#include "RiseFallMinMax.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "SdcClass.hh"
#include "Search.hh"
#include "SearchPred.hh"
#include "Sim.hh"
#include "Sta.hh"
#include "StaState.hh"
#include "TableModel.hh"
#include "Tag.hh"
#include "TimingArc.hh"
#include "TimingRole.hh"
#include "Transition.hh"
#include "Units.hh"
#include "VisitPathEnds.hh"
#include "liberty/LibertyBuilder.hh"

namespace sta {

LibertyLibrary *
makeTimingModel(std::string_view lib_name,
                std::string_view cell_name,
                std::string_view filename,
                const Scene *scene,
                Sta *sta)
{
  MakeTimingModel maker(lib_name, cell_name, filename, scene, sta);
  return maker.makeTimingModel();
}

MakeTimingModel::MakeTimingModel(std::string_view lib_name,
                                 std::string_view cell_name,
                                 std::string_view filename,
                                 const Scene *scene,
                                 Sta *sta) :
  StaState(sta),
  lib_name_(lib_name),
  cell_name_(cell_name),
  filename_(filename),
  scene_(scene),
  min_max_(MinMax::max()),
  lib_builder_(new LibertyBuilder(debug_,
                                  report_)),
  sdc_(scene->sdc()),
  sta_(sta)
{
  scenes_.insert(scene_);
}

MakeTimingModel::~MakeTimingModel() { delete lib_builder_; }

LibertyLibrary *
MakeTimingModel::makeTimingModel()
{
  saveSdc();

  tbl_template_index_ = 1;
  makeLibrary();
  makeCell();
  makePorts();

  sta_->searchPreamble();

  setConstantOutputs();
  findTimingFromInputs();
  findClkedOutputPaths();
  findClkTreeDelays();

  cell_->finish(false, report_, debug_);
  restoreSdc();

  return library_;
}

// Move sdc commands used by makeTimingModel to the side.
void
MakeTimingModel::saveSdc()
{
  sdc_backup_ = new Sdc(sdc_->mode(), this);
  swapSdcWithBackup();
  sta_->delaysInvalid();
}

void
MakeTimingModel::restoreSdc()
{
  swapSdcWithBackup();
  delete sdc_backup_;
  sta_->delaysInvalid();
}

void
MakeTimingModel::swapSdcWithBackup()
{
  Sdc::swapPortDelays(sdc_, sdc_backup_);
  Sdc::swapPortExtCaps(sdc_, sdc_backup_);
  Sdc::swapDeratingFactors(sdc_, sdc_backup_);
  Sdc::swapClockInsertions(sdc_, sdc_backup_);
}

void
MakeTimingModel::makeLibrary()
{
  library_ = network_->makeLibertyLibrary(lib_name_, filename_);
  const LibertySeq &scene_libs = scene_->libertyLibraries(MinMax::max());
  if (!scene_libs.empty()) {
    const LibertyLibrary *scene_lib = scene_libs[0];
    *library_->units() = *scene_lib->units();

    for (const RiseFall *rf : RiseFall::range()) {
      library_->setInputThreshold(rf, scene_lib->inputThreshold(rf));
      library_->setOutputThreshold(rf, scene_lib->outputThreshold(rf));
      library_->setSlewLowerThreshold(rf, scene_lib->slewLowerThreshold(rf));
      library_->setSlewUpperThreshold(rf, scene_lib->slewUpperThreshold(rf));
    }

    library_->setDelayModelType(scene_lib->delayModelType());
    library_->setNominalProcess(scene_lib->nominalProcess());
    library_->setNominalVoltage(scene_lib->nominalVoltage());
    library_->setNominalTemperature(scene_lib->nominalTemperature());
  }
  else
    report_->error(1381, "scene {} has no liberty libraries.", scene_->name());
}

void
MakeTimingModel::makeCell()
{
  cell_ = lib_builder_->makeCell(library_, cell_name_, filename_);
  cell_->setIsMacro(true);
  cell_->setArea(findArea());
}

float
MakeTimingModel::findArea()
{
  float area = 0.0;
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    const Instance *inst = leaf_iter->next();
    const LibertyCell *cell = network_->libertyCell(inst);
    if (cell)
      area += cell->area();
  }
  delete leaf_iter;
  return area;
}

void
MakeTimingModel::makePorts()
{
  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  CellPortIterator *port_iter = network_->portIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    std::string port_name(network_->name(port));
    if (network_->isBus(port)) {
      int from_index = network_->fromIndex(port);
      int to_index = network_->toIndex(port);
      BusDcl *bus_dcl = library_->makeBusDcl(port_name, from_index, to_index);
      LibertyPort *lib_port =
          lib_builder_->makeBusPort(cell_, port_name, from_index, to_index, bus_dcl);
      lib_port->setDirection(network_->direction(port));
      PortMemberIterator *member_iter = network_->memberIterator(port);
      while (member_iter->hasNext()) {
        Port *bit_port = member_iter->next();
        Pin *pin = network_->findPin(top_inst, bit_port);
        LibertyPort *lib_bit_port = modelPort(pin);
        float load_cap = graph_delay_calc_->loadCap(pin, scene_, min_max_);
        lib_bit_port->setCapacitance(load_cap);
        setPortLimits(pin, lib_bit_port);
      }
      delete member_iter;
    }
    else {
      LibertyPort *lib_port = lib_builder_->makePort(cell_, port_name);
      lib_port->setDirection(network_->direction(port));
      Pin *pin = network_->findPin(top_inst, port);
      float load_cap = graph_delay_calc_->loadCap(pin, scene_, min_max_);
      lib_port->setCapacitance(load_cap);
      setPortLimits(pin, lib_port);
    }
  }
  delete port_iter;
}

// An output a constant drives is that constant in the model, as a tie
// cell's output is: a parent propagates it, where an output with no
// arc to it would leave what it feeds unconstrained. Only the netlist's
// constants: one the block's SDC sets (set_case_analysis,
// set_logic_zero/one) holds in the mode it was characterized in, not in
// every mode the parent is timed in, and written into the model it
// would silently remove the parent's paths through that output.
void
MakeTimingModel::setConstantOutputs()
{
  Sim netlist_sim(sta_);
  netlist_sim.setMode(scene_->mode());
  netlist_sim.findNetlistConstants();
  Sim *mode_sim = scene_->mode()->sim();
  mode_sim->ensureConstantsPropagated();
  InstancePinIterator *pin_iter = network_->pinIterator(network_->topInstance());
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (!network_->direction(pin)->isOutput())
      continue;
    LogicValue value = netlist_sim.simValue(pin);
    if (value == LogicValue::zero)
      modelPort(pin)->setFunction(FuncExpr::makeZero());
    else if (value == LogicValue::one)
      modelPort(pin)->setFunction(FuncExpr::makeOne());
    else {
      LogicValue mode_value = mode_sim->simValue(pin);
      if (mode_value == LogicValue::zero || mode_value == LogicValue::one)
        report_->warn(1382,
                      "output {} is constant {} only in this mode "
                      "(set_case_analysis, set_logic_zero/one); "
                      "the model has no arcs to it.",
                      network_->pathName(pin), logicValueString(mode_value));
    }
  }
  delete pin_iter;
}

// Find port max_cap/max_slew limits.
void
MakeTimingModel::setPortLimits(const Pin *pin,
                               LibertyPort *lib_port)
{
  const PortDirection *dir = network_->direction(pin);
  float slew_limit = std::numeric_limits<float>::max();
  float cap_limit = std::numeric_limits<float>::max();
  bool slew_exists = false;
  bool cap_exists = false;
  PinConnectedPinIterator *pin_iter = network_->connectedPinIterator(pin);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    const LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      const LibertyLibrary *lib = port->libertyCell()->libertyLibrary();
      float limit;
      bool exists;
      port->slewLimit(min_max_, limit, exists);
      if (!exists)
        lib->defaultMaxSlew(limit, exists);
      if (exists && limit < slew_limit) {
        slew_limit = limit;
        slew_exists = true;
      }

      if (dir->isAnyOutput()) {
        port->capacitanceLimit(min_max_, limit, exists);
        if (!exists)
          lib->defaultMaxCapacitance(limit, exists);
        if (exists && limit < cap_limit) {
          cap_limit = limit;
          cap_exists = true;
        }
      }
    }
  }
  delete pin_iter;
  if (slew_exists)
    lib_port->setSlewLimit(slew_limit, min_max_);
  if (cap_exists)
    lib_port->setCapacitanceLimit(cap_limit, min_max_);
}

void
MakeTimingModel::checkClock(Clock *clk)
{
  for (const Pin *pin : clk->leafPins()) {
    if (!network_->isTopLevelPort(pin))
      report_->warn(1380, "clock {} pin {} is inside model block.", clk->name(),
                    network_->pathName(pin));
  }
}

////////////////////////////////////////////////////////////////

class MakeEndTimingArcs : public PathEndVisitor
{
public:
  MakeEndTimingArcs(Sta *sta);
  MakeEndTimingArcs(const MakeEndTimingArcs &) = default;
  PathEndVisitor *copy() const override;
  void visit(PathEnd *path_end) override;
  void setInputRf(const RiseFall *input_rf);
  const ClockEdgeDelays &margins() const { return margins_; }
  // Take the larger margin of this and other's, as visit does.
  void mergeMargins(const MakeEndTimingArcs &other);

private:
  const RiseFall *input_rf_{nullptr};
  ClockEdgeDelays margins_;
  Sta *sta_;
};

MakeEndTimingArcs::MakeEndTimingArcs(Sta *sta) :
  sta_(sta)
{
}

PathEndVisitor *
MakeEndTimingArcs::copy() const
{
  return new MakeEndTimingArcs(*this);
}

void
MakeEndTimingArcs::setInputRf(const RiseFall *input_rf)
{
  input_rf_ = input_rf;
}

void
MakeEndTimingArcs::visit(PathEnd *path_end)
{
  Path *src_path = path_end->path();
  const Sdc *sdc = src_path->sdc(sta_);
  const Clock *src_clk = src_path->clock(sta_);
  const ClockEdge *tgt_clk_edge = path_end->targetClkEdge(sta_);
  if (src_clk == sdc->defaultArrivalClock() && tgt_clk_edge) {
    Network *network = sta_->network();
    Debug *debug = sta_->debug();
    const MinMax *min_max = path_end->minMax(sta_);
    Arrival data_delay = src_path->arrival();
    Delay clk_latency = path_end->targetClkDelay(sta_);
    ArcDelay check_margin = path_end->margin(sta_);
    Delay margin = (min_max == MinMax::max())
      ? delaySum(delayDiff(data_delay, clk_latency, sta_), check_margin, sta_)
      : delaySum(delayDiff(clk_latency, data_delay, sta_), check_margin, sta_);
    float delay1 = delayAsFloat(margin, MinMax::max(), sta_);
    debugPrint(debug, "make_timing_model", 2, "{} -> {} clock {} {} {} {}",
               input_rf_->shortName(), network->pathName(src_path->pin(sta_)),
               tgt_clk_edge->name(), path_end->typeName(),
               min_max->to_string(), delayAsString(margin, sta_));
    if (debug->check("make_timing_model", 3))
      sta_->reportPathEnd(path_end);

    RiseFallMinMax &margins = margins_[tgt_clk_edge];
    float max_margin;
    bool max_exists;
    margins.value(input_rf_, min_max, max_margin, max_exists);
    // Always max margin, even for min/hold checks.
    margins.setValue(input_rf_, min_max,
                     max_exists ? std::max(max_margin, delay1) : delay1);
  }
}

void
MakeEndTimingArcs::mergeMargins(const MakeEndTimingArcs &other)
{
  for (const auto &[clk_edge, other_margins] : other.margins_) {
    RiseFallMinMax &margins = margins_[clk_edge];
    for (const RiseFall *rf : RiseFall::range()) {
      for (const MinMax *min_max : MinMax::range()) {
        float other_margin;
        bool other_exists;
        other_margins.value(rf, min_max, other_margin, other_exists);
        if (other_exists) {
          float margin;
          bool exists;
          margins.value(rf, min_max, margin, exists);
          margins.setValue(rf, min_max,
                           exists ? std::max(margin, other_margin) : other_margin);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

// The inputs' timing on a flat copy of the search graph. After delay
// calculation an arc's delay does not depend on the path that reaches
// it, so the arrival the search keeps for one input's tag at a vertex
// is a sum of fixed arc delays: the candidates of the fanin edges and
// their arcs, offered in the order the search offers them and kept by
// the same fuzzy comparison, without tags, paths or tag groups. The
// checks and output delays are read back as MakeEndTimingArcs and
// findOutputDelays read them from the search.
struct FlatEndTerm
{
  uint8_t rf;
  uint8_t path_mm;
  const MinMax *end_mm;
  const ClockEdge *clk_edge;
  Delay clk_latency;
  Delay check_margin;
};

class FlatTimingModel : public StaState
{
public:
  FlatTimingModel(const Scene *scene,
                  size_t thread_count,
                  const StaState *sta);
  // False when the flat graph does not stand for the search of these
  // inputs. Inputs whose cone reaches a latch are left to the search,
  // in fallback_pins. Inputs arrive on the default arrival clock.
  bool findTiming(const PinSeq &input_pins,
                  InputMargins &margins,
                  InputOutputDelays &output_delays,
                  PinSeq &fallback_pins);

private:
  struct Cand
  {
    uint32_t from;
    uint8_t from_rf;
    uint8_t to_rf;
    uint8_t valid;
    float delay[MinMax::index_count];
  };
  struct Worker
  {
    std::vector<uint32_t> mark;
    std::vector<uint32_t> local;
    uint32_t gen{0};
    std::vector<uint32_t> cone;
    std::vector<uint32_t> added;
    std::vector<float> arrivals;
    InputMargins margins;
    InputOutputDelays output_delays;
  };

  bool staticEdge(Edge *edge,
                  Vertex *from_vertex,
                  Vertex *to_vertex) const;
  bool exceptionsUniform(const PinSeq &input_pins) const;
  bool findSeeds(const PinSeq &input_pins);
  void findUnion();
  bool findCands();
  bool findEndTerms();
  void findInputs(std::vector<size_t> &flat_inputs,
                  PinSeq &fallback_pins);
  void findGroup(const std::vector<size_t> &group,
                 Worker &worker);
  void addCone(size_t input,
               Worker &worker);
  void findBatch(const std::vector<size_t> &batch,
                 Worker &worker);
  bool checkDesignArrivals(const PinSeq &fallback_pins);
  void forEach(size_t count,
               const std::function<void(size_t,
                                        size_t)> &fn);

  const Scene *scene_;
  const Mode *mode_;
  const Sdc *sdc_;
  size_t threads_;
  DcalcAPIndex dcalc_ap_[MinMax::index_count];

  PinSeq inputs_;
  // Seed arrivals by input, rf, min/max; NaN where there is none.
  std::vector<float> seeds_;
  std::vector<uint32_t> input_ui_;
  std::vector<bool> input_has_seed_;
  // Vertices the inputs reach, in level order, and their index there.
  std::vector<Vertex *> verts_;
  std::vector<uint32_t> ui_of_;
  std::vector<uint64_t> cand_begin_;
  std::vector<Cand> cands_;
  std::vector<uint64_t> fanout_begin_;
  std::vector<uint32_t> fanouts_;
  std::vector<int32_t> end_index_;
  std::vector<std::vector<FlatEndTerm>> end_terms_;
  std::vector<bool> is_output_;
  std::vector<bool> reaches_latch_;
  // Extremes over the inputs by vertex, rf, min/max, as float bits.
  std::unique_ptr<std::atomic<uint32_t>[]> extremes_;
  std::vector<Worker> workers_;
  size_t batch_bytes_{size_t(256) << 20};
};

static constexpr uint32_t flat_none = std::numeric_limits<uint32_t>::max();

static uint32_t
floatBits(float value)
{
  uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

static float
bitsFloat(uint32_t bits)
{
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

FlatTimingModel::FlatTimingModel(const Scene *scene,
                                 size_t thread_count,
                                 const StaState *sta) :
  StaState(sta),
  scene_(scene),
  mode_(scene->mode()),
  sdc_(scene->sdc()),
  threads_(std::max(thread_count,
                    size_t(1)))
{
  for (const MinMax *mm : MinMax::range())
    dcalc_ap_[mm->index()] = scene->dcalcAnalysisPtIndex(mm);
}

void
FlatTimingModel::forEach(size_t count,
                         const std::function<void(size_t,
                                                  size_t)> &fn)
{
  if (threads_ == 1 || dispatch_queue_ == nullptr) {
    for (size_t i = 0; i < count; i++)
      fn(i, 0);
  }
  else {
    for (size_t i = 0; i < count; i++)
      dispatch_queue_->dispatch([&fn, i](size_t thread) { fn(i, thread); });
    dispatch_queue_->finishTasks();
  }
}

// The edges a data path from an input is propagated over: those of
// PathVisitor::visitFaninPaths, visitEdge's search predicates and the
// data branch of visitFromPath. A path from an input does not pass a
// register's clock to output arc; latch inputs are left to the search.
bool
FlatTimingModel::staticEdge(Edge *edge,
                            Vertex *from_vertex,
                            Vertex *to_vertex) const
{
  const TimingRole *role = edge->role();
  if (role->isTimingCheck() || role == TimingRole::latchDtoQ()
      || role->genericRole() == TimingRole::regClkToQ())
    return false;
  const SearchPred *pred = search_->evalPred();
  return pred->searchFrom(from_vertex, mode_) && pred->searchThru(edge, mode_)
      && pred->searchTo(to_vertex, mode_)
      && !sdc_->isPathDelayInternalFromBreak(to_vertex->pin())
      && !sdc_->isPathDelayInternalToBreak(from_vertex->pin());
}

// An exception that starts at some of the inputs and not at others
// gives them different tags; only a false path from inputs, which
// leaves them without paths, is allowed.
bool
FlatTimingModel::exceptionsUniform(const PinSeq &input_pins) const
{
  std::unordered_set<const Pin *> inputs(input_pins.begin(), input_pins.end());
  for (ExceptionPath *exception : sdc_->exceptions()) {
    if (exception->isFilter())
      continue;
    if (exception->isLoop() || (exception->thrus() && !exception->thrus()->empty()))
      return false;
    ExceptionFrom *from = exception->from();
    if (from) {
      InstanceSet *insts = from->instances();
      if (insts && !insts->empty())
        return false;
      PinSet *pins = from->pins();
      if (pins && !pins->empty()) {
        if (from->transition() != RiseFallBoth::riseFall())
          return false;
        size_t count = 0;
        for (const Pin *pin : *pins) {
          if (inputs.contains(pin))
            count++;
        }
        if (count != 0 && count != inputs.size()
            && !(exception->isFalse() && exception->to() == nullptr))
          return false;
      }
    }
  }
  return true;
}

// An input's seeds are the design's paths at it on the default arrival
// clock, which the search retags with the input's own clocks.
bool
FlatTimingModel::findSeeds(const PinSeq &input_pins)
{
  inputs_ = input_pins;
  size_t count = inputs_.size();
  seeds_.assign(count * 4, std::numeric_limits<float>::quiet_NaN());
  input_has_seed_.assign(count, false);
  const Clock *default_clk = sdc_->defaultArrivalClock();
  for (size_t i = 0; i < count; i++) {
    const Pin *pin = inputs_[i];
    if (network_->direction(pin)->isBidirect())
      return false;
    Vertex *vertex = graph_->pinDrvrVertex(pin);
    if (vertex == nullptr)
      continue;
    VertexInEdgeIterator edge_iter(vertex, graph_);
    if (edge_iter.hasNext())
      return false;
    VertexPathIterator path_iter(vertex, this);
    while (path_iter.hasNext()) {
      Path *path = path_iter.next();
      if (path->clock(this) == default_clk && !path->isClock(this)
          && path->scene(this) == scene_ && !path->tag(this)->isSegmentStart()) {
        size_t k = i * 4 + path->transition(this)->index() * 2
            + path->minMax(this)->index();
        if (!std::isnan(seeds_[k]))
          return false;
        seeds_[k] = delayAsFloat(path->arrival());
        input_has_seed_[i] = true;
      }
    }
  }
  return true;
}

void
FlatTimingModel::findUnion()
{
  size_t vertex_max = 0;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext())
    vertex_max = std::max(vertex_max, size_t(graph_->id(vertex_iter.next())) + 1);
  ui_of_.assign(vertex_max, flat_none);
  std::vector<Vertex *> queue;
  for (size_t i = 0; i < inputs_.size(); i++) {
    if (input_has_seed_[i]) {
      Vertex *vertex = graph_->pinDrvrVertex(inputs_[i]);
      VertexId id = graph_->id(vertex);
      if (ui_of_[id] == flat_none) {
        ui_of_[id] = 0;
        queue.push_back(vertex);
      }
    }
  }
  for (size_t q = 0; q < queue.size(); q++) {
    Vertex *from_vertex = queue[q];
    VertexOutEdgeIterator edge_iter(from_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph_);
      VertexId id = graph_->id(to_vertex);
      if (ui_of_[id] == flat_none && staticEdge(edge, from_vertex, to_vertex)) {
        ui_of_[id] = 0;
        queue.push_back(to_vertex);
      }
    }
  }
  std::sort(queue.begin(), queue.end(), [this](Vertex *v1, Vertex *v2) {
    Level level1 = v1->level();
    Level level2 = v2->level();
    return level1 < level2 || (level1 == level2 && graph_->id(v1) < graph_->id(v2));
  });
  verts_ = std::move(queue);
  for (size_t ui = 0; ui < verts_.size(); ui++)
    ui_of_[graph_->id(verts_[ui])] = ui;
  input_ui_.assign(inputs_.size(), flat_none);
  for (size_t i = 0; i < inputs_.size(); i++) {
    if (input_has_seed_[i])
      input_ui_[i] = ui_of_[graph_->id(graph_->pinDrvrVertex(inputs_[i]))];
  }
}

// Each vertex's candidates in the order ArrivalVisitor offers them:
// the fanin edges, the fanin's paths by transition, the arcs from it.
bool
FlatTimingModel::findCands()
{
  size_t count = verts_.size();
  size_t chunk = 4096;
  size_t chunks = (count + chunk - 1) / chunk;
  std::vector<std::vector<Cand>> chunk_cands(chunks);
  std::vector<std::vector<uint32_t>> chunk_counts(chunks);
  std::atomic<bool> ok{true};
  forEach(chunks, [&](size_t c, size_t) {
    std::vector<Cand> &cands = chunk_cands[c];
    std::vector<uint32_t> &counts = chunk_counts[c];
    size_t end = std::min(count, (c + 1) * chunk);
    for (size_t ui = c * chunk; ui < end; ui++) {
      Vertex *to_vertex = verts_[ui];
      size_t begin = cands.size();
      VertexInEdgeIterator edge_iter(to_vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        Vertex *from_vertex = edge->from(graph_);
        uint32_t from_ui = ui_of_[graph_->id(from_vertex)];
        if (from_ui == flat_none || !staticEdge(edge, from_vertex, to_vertex))
          continue;
        if (from_vertex->level() >= to_vertex->level())
          ok = false;
        TimingArcSet *arc_set = edge->timingArcSet();
        for (const RiseFall *from_rf : RiseFall::range()) {
          TimingArc *arcs[2];
          arc_set->arcsFrom(from_rf, arcs[0], arcs[1]);
          for (TimingArc *arc : arcs) {
            if (arc == nullptr)
              continue;
            const RiseFall *to_rf = arc->toEdge()->asRiseFall();
            if (to_rf == nullptr
                || !searchThru(from_vertex, from_rf, edge, to_vertex, to_rf, mode_))
              continue;
            Cand cand{from_ui,
                      uint8_t(from_rf->index()),
                      uint8_t(to_rf->index()),
                      0,
                      {0.0, 0.0}};
            for (const MinMax *mm : MinMax::range()) {
              ArcDelay delay = search_->deratedDelay(
                  from_vertex, arc, edge, false, mm, dcalc_ap_[mm->index()], sdc_);
              if (!delayInf(delay, this)) {
                cand.delay[mm->index()] = delayAsFloat(delay);
                cand.valid |= 1 << mm->index();
              }
            }
            if (cand.valid)
              cands.push_back(cand);
          }
        }
      }
      counts.push_back(cands.size() - begin);
    }
  });
  if (!ok)
    return false;
  cand_begin_.assign(count + 1, 0);
  size_t ui = 0;
  for (size_t c = 0; c < chunks; c++) {
    for (uint32_t n : chunk_counts[c]) {
      cand_begin_[ui + 1] = cand_begin_[ui] + n;
      ui++;
    }
  }
  cands_.reserve(cand_begin_[count]);
  for (size_t c = 0; c < chunks; c++) {
    cands_.insert(cands_.end(), chunk_cands[c].begin(), chunk_cands[c].end());
    std::vector<Cand>().swap(chunk_cands[c]);
  }
  fanout_begin_.assign(count + 1, 0);
  for (const Cand &cand : cands_)
    fanout_begin_[cand.from + 1]++;
  for (size_t i = 0; i < count; i++)
    fanout_begin_[i + 1] += fanout_begin_[i];
  fanouts_.resize(cands_.size());
  std::vector<uint64_t> fill(fanout_begin_.begin(), fanout_begin_.end() - 1);
  for (size_t to = 0; to < count; to++) {
    for (uint64_t k = cand_begin_[to]; k < cand_begin_[to + 1]; k++)
      fanouts_[fill[cands_[k].from]++] = to;
  }
  // Whether a vertex reaches a latch input, from the last level back.
  reaches_latch_.assign(count, false);
  for (size_t i = count; i-- > 0;) {
    if (network_->isLatchData(verts_[i]->pin()))
      reaches_latch_[i] = true;
    if (reaches_latch_[i]) {
      for (uint64_t k = cand_begin_[i]; k < cand_begin_[i + 1]; k++)
        reaches_latch_[cands_[k].from] = true;
    }
  }
  return true;
}

// The capture terms of each endpoint, from the path ends of the
// design's paths on the default arrival clock: every input's paths
// carry the same exception states and differ only in arrival.
class FlatEndTerms : public PathEndVisitor
{
public:
  FlatEndTerms(const Clock *default_clk,
               const StaState *sta) :
    default_clk_(default_clk),
    sta_(sta)
  {
  }
  PathEndVisitor *copy() const override { return new FlatEndTerms(*this); }
  void visit(PathEnd *path_end) override
  {
    Path *src_path = path_end->path();
    const ClockEdge *tgt_clk_edge = path_end->targetClkEdge(sta_);
    if (src_path->clock(sta_) == default_clk_ && !src_path->isClock(sta_)
        && tgt_clk_edge) {
      size_t rf = src_path->transition(sta_)->index();
      size_t mm = src_path->minMax(sta_)->index();
      const Path *&path = paths_[rf][mm];
      if (path && path != src_path)
        ambiguous_ = true;
      path = src_path;
      terms_.push_back({uint8_t(rf), uint8_t(mm), path_end->minMax(sta_),
                        tgt_clk_edge, path_end->targetClkDelay(sta_),
                        path_end->margin(sta_)});
    }
  }
  std::vector<FlatEndTerm> terms_;
  bool ambiguous_{false};

private:
  const Clock *default_clk_;
  const Path *paths_[RiseFall::index_count][MinMax::index_count]{};
  const StaState *sta_;
};

bool
FlatTimingModel::findEndTerms()
{
  size_t count = verts_.size();
  is_output_.assign(count, false);
  end_index_.assign(count, -1);
  std::vector<uint32_t> ends;
  for (size_t ui = 0; ui < count; ui++) {
    Vertex *vertex = verts_[ui];
    const Pin *pin = vertex->pin();
    if (network_->isTopLevelPort(pin) && network_->direction(pin)->isOutput())
      is_output_[ui] = true;
    if (search_->isEndpoint(vertex)) {
      end_index_[ui] = ends.size();
      ends.push_back(ui);
    }
  }
  end_terms_.assign(ends.size(), {});
  std::atomic<bool> ok{true};
  SceneSet scenes;
  scenes.insert(scene_);
  const Clock *default_clk = sdc_->defaultArrivalClock();
  size_t chunk = 256;
  forEach((ends.size() + chunk - 1) / chunk, [&](size_t c, size_t) {
    VisitPathEnds visit_ends(this);
    size_t end = std::min(ends.size(), (c + 1) * chunk);
    for (size_t e = c * chunk; e < end; e++) {
      FlatEndTerms visitor(default_clk, this);
      visit_ends.visitPathEnds(verts_[ends[e]], scenes, MinMaxAll::all(), false,
                               &visitor);
      if (visitor.ambiguous_)
        ok = false;
      end_terms_[e] = std::move(visitor.terms_);
    }
  });
  return ok;
}

void
FlatTimingModel::addCone(size_t input,
                         Worker &worker)
{
  worker.added.clear();
  uint32_t start = input_ui_[input];
  if (worker.mark[start] != worker.gen) {
    worker.mark[start] = worker.gen;
    worker.added.push_back(start);
  }
  for (size_t q = 0; q < worker.added.size(); q++) {
    uint32_t from = worker.added[q];
    for (uint64_t k = fanout_begin_[from]; k < fanout_begin_[from + 1]; k++) {
      uint32_t to = fanouts_[k];
      if (worker.mark[to] != worker.gen) {
        worker.mark[to] = worker.gen;
        worker.added.push_back(to);
      }
    }
  }
}

// Inputs join a batch while its arrivals stay within batch_bytes_; the
// result of an input does not depend on the batch it is found in.
void
FlatTimingModel::findGroup(const std::vector<size_t> &group,
                           Worker &worker)
{
  if (worker.mark.empty()) {
    worker.mark.assign(verts_.size(), 0);
    worker.local.assign(verts_.size(), 0);
  }
  std::vector<size_t> batch;
  auto newBatch = [&]() {
    batch.clear();
    worker.cone.clear();
    if (++worker.gen == 0) {
      std::fill(worker.mark.begin(), worker.mark.end(), 0);
      worker.gen = 1;
    }
  };
  newBatch();
  for (size_t input : group) {
    addCone(input, worker);
    size_t cone = worker.cone.size() + worker.added.size();
    size_t bytes = cone * 4 * 2 * (batch.size() + 1) * sizeof(float);
    if (!batch.empty() && bytes > batch_bytes_) {
      for (uint32_t ui : worker.added)
        worker.mark[ui] = 0;
      findBatch(batch, worker);
      newBatch();
      addCone(input, worker);
    }
    worker.cone.insert(worker.cone.end(), worker.added.begin(), worker.added.end());
    batch.push_back(input);
  }
  if (!batch.empty())
    findBatch(batch, worker);
}

void
FlatTimingModel::findBatch(const std::vector<size_t> &batch,
                           Worker &worker)
{
  std::vector<uint32_t> &cone = worker.cone;
  std::sort(cone.begin(), cone.end());
  for (size_t l = 0; l < cone.size(); l++)
    worker.local[cone[l]] = l;
  size_t sources = batch.size() * 2;
  std::vector<float> &arrivals = worker.arrivals;
  arrivals.assign(cone.size() * 4 * sources,
                  std::numeric_limits<float>::quiet_NaN());
  auto slot = [sources](size_t l, size_t rf, size_t mm) {
    return (l * 4 + rf * 2 + mm) * sources;
  };
  for (size_t j = 0; j < batch.size(); j++) {
    size_t input = batch[j];
    size_t l = worker.local[input_ui_[input]];
    for (size_t rf = 0; rf < 2; rf++) {
      for (size_t mm = 0; mm < 2; mm++)
        arrivals[slot(l, rf, mm) + j * 2 + rf] = seeds_[input * 4 + rf * 2 + mm];
    }
  }
  const size_t mm_max = MinMax::max()->index();
  for (size_t l = 0; l < cone.size(); l++) {
    uint32_t ui = cone[l];
    for (uint64_t k = cand_begin_[ui]; k < cand_begin_[ui + 1]; k++) {
      const Cand &cand = cands_[k];
      if (worker.mark[cand.from] != worker.gen)
        continue;
      size_t from_l = worker.local[cand.from];
      for (size_t mm = 0; mm < 2; mm++) {
        if (!(cand.valid & (1 << mm)))
          continue;
        float delay = cand.delay[mm];
        const float *from = &arrivals[slot(from_l, cand.from_rf, mm)];
        float *to = &arrivals[slot(l, cand.to_rf, mm)];
        if (mm == mm_max) {
          for (size_t s = 0; s < sources; s++) {
            float arrival = from[s];
            if (!std::isnan(arrival)) {
              float to_arrival = arrival + delay;
              float &keep = to[s];
              if (std::isnan(keep) || fuzzyGreater(to_arrival, keep))
                keep = to_arrival;
            }
          }
        }
        else {
          for (size_t s = 0; s < sources; s++) {
            float arrival = from[s];
            if (!std::isnan(arrival)) {
              float to_arrival = arrival + delay;
              float &keep = to[s];
              if (std::isnan(keep) || fuzzyLess(to_arrival, keep))
                keep = to_arrival;
            }
          }
        }
      }
    }
    for (size_t rf = 0; rf < 2; rf++) {
      for (size_t mm = 0; mm < 2; mm++) {
        const float *values = &arrivals[slot(l, rf, mm)];
        float extreme = std::numeric_limits<float>::quiet_NaN();
        for (size_t s = 0; s < sources; s++) {
          float value = values[s];
          if (!std::isnan(value)
              && (std::isnan(extreme)
                  || (mm == mm_max ? value > extreme : value < extreme)))
            extreme = value;
        }
        if (!std::isnan(extreme)) {
          std::atomic<uint32_t> &bits = extremes_[ui * 4 + rf * 2 + mm];
          uint32_t old_bits = bits.load(std::memory_order_relaxed);
          while (true) {
            float old = bitsFloat(old_bits);
            if (!(std::isnan(old) || (mm == mm_max ? extreme > old : extreme < old)))
              break;
            if (bits.compare_exchange_weak(old_bits, floatBits(extreme),
                                           std::memory_order_relaxed))
              break;
          }
        }
      }
    }
    int32_t end = end_index_[ui];
    if (end >= 0) {
      for (const FlatEndTerm &term : end_terms_[end]) {
        const float *values = &arrivals[slot(l, term.rf, term.path_mm)];
        for (size_t s = 0; s < sources; s++) {
          float arrival = values[s];
          if (std::isnan(arrival))
            continue;
          const Pin *input_pin = inputs_[batch[s / 2]];
          const RiseFall *input_rf = RiseFall::find(s % 2);
          Delay data_delay(arrival);
          Delay margin = (term.end_mm == MinMax::max())
              ? delaySum(delayDiff(data_delay, term.clk_latency, this),
                         term.check_margin, this)
              : delaySum(delayDiff(term.clk_latency, data_delay, this),
                         term.check_margin, this);
          float delay1 = delayAsFloat(margin, MinMax::max(), this);
          RiseFallMinMax &margins = worker.margins[input_pin][term.clk_edge];
          float max_margin;
          bool max_exists;
          margins.value(input_rf, term.end_mm, max_margin, max_exists);
          // Always max margin, even for min/hold checks.
          margins.setValue(input_rf, term.end_mm,
                           max_exists ? std::max(max_margin, delay1) : delay1);
        }
      }
    }
    if (is_output_[ui]) {
      const Pin *output_pin = verts_[ui]->pin();
      for (size_t rf = 0; rf < 2; rf++) {
        const RiseFall *output_rf = RiseFall::find(rf);
        for (const MinMax *mm : MinMax::range()) {
          const float *values = &arrivals[slot(l, rf, mm->index())];
          for (size_t s = 0; s < sources; s++) {
            float arrival = values[s];
            if (std::isnan(arrival))
              continue;
            const Pin *input_pin = inputs_[batch[s / 2]];
            OutputDelays &delays = worker.output_delays[input_pin][output_pin];
            delays.delays.mergeValue(output_rf, mm,
                                     delayAsFloat(Delay(arrival), mm, this));
            delays.rf_path_exists[s % 2][rf] = true;
          }
        }
      }
    }
  }
}

void
FlatTimingModel::findInputs(std::vector<size_t> &flat_inputs,
                            PinSeq &fallback_pins)
{
  for (size_t i = 0; i < inputs_.size(); i++) {
    if (!input_has_seed_[i])
      continue;
    if (reaches_latch_[input_ui_[i]])
      fallback_pins.push_back(inputs_[i]);
    else
      flat_inputs.push_back(i);
  }
}

// The extremes over the inputs are the design's own arrivals on the
// default arrival clock, but for the order the search merged them in.
bool
FlatTimingModel::checkDesignArrivals(const PinSeq &fallback_pins)
{
  std::vector<bool> skip(verts_.size(), false);
  std::vector<uint32_t> queue;
  for (const Pin *pin : fallback_pins) {
    uint32_t ui = ui_of_[graph_->id(graph_->pinDrvrVertex(pin))];
    if (!skip[ui]) {
      skip[ui] = true;
      queue.push_back(ui);
    }
  }
  for (size_t q = 0; q < queue.size(); q++) {
    uint32_t from = queue[q];
    for (uint64_t k = fanout_begin_[from]; k < fanout_begin_[from + 1]; k++) {
      uint32_t to = fanouts_[k];
      if (!skip[to]) {
        skip[to] = true;
        queue.push_back(to);
      }
    }
  }
  std::vector<Vertex *> vertices;
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext())
    vertices.push_back(vertex_iter.next());
  const Clock *default_clk = sdc_->defaultArrivalClock();
  std::atomic<bool> ok{true};
  size_t chunk = 4096;
  forEach((vertices.size() + chunk - 1) / chunk, [&](size_t c, size_t) {
    size_t end = std::min(vertices.size(), (c + 1) * chunk);
    for (size_t v = c * chunk; v < end && ok; v++) {
      Vertex *vertex = vertices[v];
      uint32_t ui = ui_of_[graph_->id(vertex)];
      if (ui != flat_none && skip[ui])
        continue;
      float design[4];
      std::fill(design, design + 4, std::numeric_limits<float>::quiet_NaN());
      VertexPathIterator path_iter(vertex, this);
      while (path_iter.hasNext()) {
        Path *path = path_iter.next();
        if (path->clock(this) == default_clk && !path->isClock(this)
            && path->scene(this) == scene_ && !path->tag(this)->isSegmentStart()) {
          size_t k =
              path->transition(this)->index() * 2 + path->minMax(this)->index();
          if (!std::isnan(design[k]))
            ok = false;
          design[k] = delayAsFloat(path->arrival());
        }
      }
      for (size_t k = 0; k < 4; k++) {
        float flat = ui == flat_none
            ? std::numeric_limits<float>::quiet_NaN()
            : bitsFloat(extremes_[ui * 4 + k].load(std::memory_order_relaxed));
        if (std::isnan(flat) != std::isnan(design[k]))
          ok = false;
        else if (!std::isnan(flat)
                 && std::abs(flat - design[k])
                     > 1e-4F * std::max(std::abs(flat), std::abs(design[k])))
          ok = false;
      }
    }
  });
  return ok;
}

bool
FlatTimingModel::findTiming(const PinSeq &input_pins,
                            InputMargins &margins,
                            InputOutputDelays &output_delays,
                            PinSeq &fallback_pins)
{
  if (dynamic_cast<const DelayOpsScalar *>(delayOps()) == nullptr
      || variables_->dynamicLoopBreaking() || !exceptionsUniform(input_pins)
      || !findSeeds(input_pins))
    return false;
  findUnion();
  if (!findCands() || !findEndTerms())
    return false;
  size_t count = verts_.size();
  extremes_ = std::make_unique<std::atomic<uint32_t>[]>(count * 4);
  uint32_t nan_bits = floatBits(std::numeric_limits<float>::quiet_NaN());
  for (size_t k = 0; k < count * 4; k++)
    extremes_[k].store(nan_bits, std::memory_order_relaxed);
  std::vector<size_t> flat_inputs;
  findInputs(flat_inputs, fallback_pins);
  constexpr size_t group_size = 64;
  std::vector<std::vector<size_t>> groups;
  for (size_t begin = 0; begin < flat_inputs.size(); begin += group_size) {
    size_t end = std::min(begin + group_size, flat_inputs.size());
    groups.emplace_back(flat_inputs.begin() + begin, flat_inputs.begin() + end);
  }
  workers_.resize(threads_);
  forEach(groups.size(),
          [&](size_t g, size_t thread) { findGroup(groups[g], workers_[thread]); });
  for (Worker &worker : workers_) {
    std::vector<uint32_t>().swap(worker.mark);
    std::vector<uint32_t>().swap(worker.local);
    std::vector<float>().swap(worker.arrivals);
  }
  bool ok = checkDesignArrivals(fallback_pins);
  debugPrint(debug_, "make_timing_model", 1,
             "flat graph {} vertices {} candidates, {} inputs flat, "
             "{} searched, design arrivals {}",
             count, cands_.size(), flat_inputs.size(), fallback_pins.size(),
             ok ? "match" : "differ");
  if (!ok)
    return false;
  for (Worker &worker : workers_) {
    for (auto &[pin, clk_margins] : worker.margins)
      margins[pin] = std::move(clk_margins);
    for (auto &[pin, pin_delays] : worker.output_delays)
      output_delays[pin] = std::move(pin_delays);
  }
  return true;
}

// input -> register setup/hold
// input -> output combinational paths
// Use default input arrival (set_input_delay with no clock) from inputs
// to find downstream register checks and output ports.
void
MakeTimingModel::findTimingFromInputs()
{
  search_->deleteFilteredArrivals();

  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  PinSeq input_pins;
  CellPortBitIterator *port_iter = network_->portBitIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *input_port = port_iter->next();
    if (network_->direction(input_port)->isInput()) {
      Pin *input_pin = network_->findPin(top_inst, input_port);
      if (!sdc_->isClock(input_pin))
        input_pins.push_back(input_pin);
    }
  }
  delete port_iter;
  if (input_pins.empty())
    return;

  InputMargins margins;
  InputOutputDelays output_delays;
  PinSeq fallback_pins;
  bool flat_ok = false;
  if (scenes_.size() == 1) {
    // Every input arrives at 0 on the default arrival clock, as the
    // input delay findTimingFromInput sets does, for the whole search.
    bool use_default_arrival_clock = variables_->useDefaultArrivalClock();
    variables_->setUseDefaultArrivalClock(true);
    search_->arrivalsInvalid();
    search_->findAllArrivals();
    FlatTimingModel flat(*scenes_.begin(), thread_count_, this);
    flat_ok = flat.findTiming(input_pins, margins, output_delays, fallback_pins);
    variables_->setUseDefaultArrivalClock(use_default_arrival_clock);
    search_->arrivalsInvalid();
    search_->findAllArrivals();
  }
  std::unordered_set<const Pin *> searched;
  if (flat_ok)
    searched.insert(fallback_pins.begin(), fallback_pins.end());
  else
    searched.insert(input_pins.begin(), input_pins.end());
  for (const Pin *input_pin : input_pins) {
    if (searched.contains(input_pin))
      findTimingFromInput(input_pin);
    else {
      auto margins_itr = margins.find(input_pin);
      makeSetupHoldTimingArcs(
          input_pin,
          margins_itr == margins.end() ? ClockEdgeDelays() : margins_itr->second);
      makeInputOutputTimingArcs(input_pin, output_delays[input_pin]);
    }
  }
}

void
MakeTimingModel::findTimingFromInput(const Pin *input_pin)
{
  {
    MakeEndTimingArcs end_visitor(sta_);
    OutputPinDelays output_delays;
    for (const RiseFall *input_rf : RiseFall::range()) {
      const RiseFallBoth *input_rf1 = input_rf->asRiseFallBoth();
      sta_->setInputDelay(input_pin, input_rf1, sdc_->defaultArrivalClock(),
                          sdc_->defaultArrivalClockEdge()->transition(), nullptr,
                          false, false, MinMaxAll::all(), true, 0.0, sdc_);

      PinSet *from_pins = new PinSet(network_);
      from_pins->insert(input_pin);
      ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr,
                                                    nullptr, input_rf1, sdc_);
      search_->findFilteredArrivals(from, nullptr, nullptr, false, false);

      end_visitor.setInputRf(input_rf);
      VertexSeq endpoints = search_->filteredEndpoints();
      if (thread_count_ == 1) {
        VisitPathEnds visit_ends(sta_);
        for (Vertex *end : endpoints)
          visit_ends.visitPathEnds(end, scenes_, MinMaxAll::all(), true,
                                   &end_visitor);
      }
      else {
        // One visitor per thread, merged after: a margin is the max over
        // the path ends, whichever thread found them.
        std::vector<MakeEndTimingArcs> end_visitors(thread_count_,
                                                    MakeEndTimingArcs(sta_));
        std::vector<VisitPathEnds> visit_ends(thread_count_,
                                              VisitPathEnds(sta_));
        for (MakeEndTimingArcs &visitor : end_visitors)
          visitor.setInputRf(input_rf);
        for (Vertex *end : endpoints) {
          dispatch_queue_->dispatch([this, end, &end_visitors, &visit_ends](size_t i) {
            visit_ends[i].visitPathEnds(end, scenes_, MinMaxAll::all(), true,
                                        &end_visitors[i]);
          });
        }
        dispatch_queue_->finishTasks();
        for (const MakeEndTimingArcs &visitor : end_visitors)
          end_visitor.mergeMargins(visitor);
      }
      findOutputDelays(input_rf, endpoints, output_delays);
      search_->deleteFilteredArrivals();

      sta_->removeInputDelay(input_pin, input_rf1, sdc_->defaultArrivalClock(),
                             sdc_->defaultArrivalClockEdge()->transition(),
                             MinMaxAll::all(), sdc_);
    }
    makeSetupHoldTimingArcs(input_pin, end_visitor.margins());
    makeInputOutputTimingArcs(input_pin, output_delays);
  }
}

void
MakeTimingModel::findOutputDelays(const RiseFall *input_rf,
                                  const VertexSeq &endpoints,
                                  OutputPinDelays &output_pin_delays)
{
  // Top-level output ports have no fanout, so they are endpoints and
  // the filtered endpoints hold every output reached from the input.
  for (Vertex *end : endpoints) {
    const Pin *output_pin = end->pin();
    if (network_->isTopLevelPort(output_pin)
        && network_->direction(output_pin)->isOutput()) {
      VertexPathIterator path_iter(end, this);
      while (path_iter.hasNext()) {
        Path *path = path_iter.next();
        if (search_->matchesFilter(path, nullptr)
            && scenes_.contains(path->scene(this))) {
          const RiseFall *output_rf = path->transition(sta_);
          const MinMax *min_max = path->minMax(sta_);
          Arrival delay = path->arrival();
          OutputDelays &delays = output_pin_delays[output_pin];
          delays.delays.mergeValue(output_rf, min_max,
                                   delayAsFloat(delay, min_max, sta_));
          delays.rf_path_exists[input_rf->index()][output_rf->index()] = true;
        }
      }
    }
  }
}

void
MakeTimingModel::makeSetupHoldTimingArcs(const Pin *input_pin,
                                         const ClockEdgeDelays &clk_margins)
{
  for (const auto &[clk_edge, margins] : clk_margins) {
    for (const MinMax *min_max : MinMax::range()) {
      bool setup = (min_max == MinMax::max());
      TimingArcAttrsPtr attrs = nullptr;
      for (const RiseFall *input_rf : RiseFall::range()) {
        float margin;
        bool exists;
        margins.value(input_rf, min_max, margin, exists);
        if (exists) {
          debugPrint(debug_, "make_timing_model", 2, "{} {} {} -> clock {} {}",
                     sta_->network()->pathName(input_pin), input_rf->shortName(),
                     min_max == MinMax::max() ? "setup" : "hold", clk_edge->name(),
                     delayAsString(margin, sta_));
          ScaleFactorType scale_type =
              setup ? ScaleFactorType::setup : ScaleFactorType::hold;
          TimingModel *check_model =
              makeScalarCheckModel(margin, scale_type, input_rf);
          if (attrs == nullptr)
            attrs = std::make_shared<TimingArcAttrs>();
          attrs->setModel(input_rf, check_model);
        }
      }
      if (attrs) {
        LibertyPort *input_port = modelPort(input_pin);
        for (const Pin *clk_pin : clk_edge->clock()->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          if (clk_port) {
            const RiseFall *clk_rf = clk_edge->transition();
            const TimingRole *role =
                setup ? TimingRole::setup() : TimingRole::hold();
            lib_builder_->makeFromTransitionArcs(cell_, clk_port, input_port,
                                                 nullptr, clk_rf, role, attrs);
          }
        }
      }
    }
  }
}

void
MakeTimingModel::makeInputOutputTimingArcs(const Pin *input_pin,
                                           OutputPinDelays &output_pin_delays)
{
  for (const auto &[output_pin, output_delays] : output_pin_delays) {
    TimingArcAttrsPtr attrs = nullptr;
    for (const RiseFall *output_rf : RiseFall::range()) {
      const MinMax *min_max = MinMax::max();
      float delay;
      bool exists;
      output_delays.delays.value(output_rf, min_max, delay, exists);
      if (exists) {
        debugPrint(debug_, "make_timing_model", 2, "{} -> {} {} delay {}",
                   network_->pathName(input_pin), network_->pathName(output_pin),
                   output_rf->shortName(), delayAsString(delay, sta_));
        TimingModel *gate_model = makeGateModelTable(output_pin, delay, output_rf);
        if (attrs == nullptr)
          attrs = std::make_shared<TimingArcAttrs>();
        attrs->setModel(output_rf, gate_model);
      }
    }
    if (attrs) {
      LibertyPort *output_port = modelPort(output_pin);
      LibertyPort *input_port = modelPort(input_pin);
      attrs->setTimingSense(output_delays.timingSense());
      lib_builder_->makeCombinationalArcs(cell_, input_port, output_port, true, true,
                                          attrs);
    }
  }
}

////////////////////////////////////////////////////////////////

// clocked register -> output paths
void
MakeTimingModel::findClkedOutputPaths()
{
  InstancePinIterator *output_iter = network_->pinIterator(network_->topInstance());
  while (output_iter->hasNext()) {
    Pin *output_pin = output_iter->next();
    if (network_->direction(output_pin)->isOutput()) {
      ClockEdgeDelays clk_delays;
      LibertyPort *output_port = modelPort(output_pin);
      Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
      // Only the paths of the model's scene.
      VertexPathIterator path_iter(output_vertex, scene_, nullptr, nullptr, this);
      while (path_iter.hasNext()) {
        Path *path = path_iter.next();
        const ClockEdge *clk_edge = path->clkEdge(sta_);
        if (clk_edge) {
          const RiseFall *output_rf = path->transition(sta_);
          const MinMax *min_max = path->minMax(sta_);
          Arrival delay = path->arrival();
          RiseFallMinMax &delays = clk_delays[clk_edge];
          delays.mergeValue(output_rf, min_max, delayAsFloat(delay, min_max, sta_));
        }
      }
      for (const auto &[clk_edge, delays] : clk_delays) {
        for (const Pin *clk_pin : clk_edge->clock()->pins()) {
          LibertyPort *clk_port = modelPort(clk_pin);
          if (clk_port) {
            const RiseFall *clk_rf = clk_edge->transition();
            TimingArcAttrsPtr attrs = nullptr;
            for (const RiseFall *output_rf : RiseFall::range()) {
              if (delays.hasValue(output_rf, min_max_)) {
                float delay = delays.value(output_rf, min_max_) - clk_edge->time();
                TimingModel *gate_model =
                    makeGateModelTable(output_pin, delay, output_rf);
                if (attrs == nullptr)
                  attrs = std::make_shared<TimingArcAttrs>();
                attrs->setModel(output_rf, gate_model);
              }
            }
            if (attrs) {
              lib_builder_->makeFromTransitionArcs(cell_, clk_port, output_port,
                                                   nullptr, clk_rf,
                                                   TimingRole::regClkToQ(), attrs);
            }
          }
        }
      }
    }
  }
  delete output_iter;
}

////////////////////////////////////////////////////////////////

void
MakeTimingModel::findClkTreeDelays()
{
  Instance *top_inst = network_->topInstance();
  Cell *top_cell = network_->cell(top_inst);
  CellPortIterator *port_iter = network_->portBitIterator(top_cell);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    if (network_->direction(port)->isInput()) {
      std::string port_name = network_->name(port);
      LibertyPort *lib_port = cell_->findLibertyPort(port_name);
      Pin *pin = network_->findPin(top_inst, port);
      if (pin && sdc_->isClock(pin)) {
        lib_port->setIsClock(true);
        ClockSet *clks = sdc_->findClocks(pin);
        if (clks->size() == 1) {
          for (const Clock *clk : *clks) {
            ClkDelays delays = sta_->findClkDelays(clk, scene_, true);
            for (const MinMax *min_max : MinMax::range()) {
              makeClkTreePaths(lib_port, min_max, TimingSense::positive_unate,
                               delays);
              makeClkTreePaths(lib_port, min_max, TimingSense::negative_unate,
                               delays);
            }
          }
        }
      }
    }
  }
  delete port_iter;
}

void
MakeTimingModel::makeClkTreePaths(LibertyPort *lib_port,
                                  const MinMax *min_max,
                                  TimingSense sense,
                                  const ClkDelays &delays)
{
  TimingArcAttrsPtr attrs = nullptr;
  for (const RiseFall *clk_rf : RiseFall::range()) {
    const RiseFall *end_rf =
        (sense == TimingSense::positive_unate) ? clk_rf : clk_rf->opposite();
    Path clk_path;
    Delay insertion, delay, latency;
    float lib_clk_delay;
    bool exists;
    delays.delay(clk_rf, end_rf, min_max, insertion, delay, lib_clk_delay, latency,
                 clk_path, exists);
    if (exists) {
      // findClkDelays chose this sink on its latency, which for a macro
      // with its own clock tree is the arrival at its clock pin plus that
      // tree (lib_clk_delay). The model's clock tree path is that sum; the
      // pin arrival alone can fall below the other bound (max below min).
      Delay tree_delay = delaySum(delay, lib_clk_delay, sta_);
      TimingModel *model = makeGateModelScalar(tree_delay, end_rf);
      if (attrs == nullptr)
        attrs = std::make_shared<TimingArcAttrs>();
      attrs->setModel(end_rf, model);
    }
  }
  if (attrs) {
    attrs->setTimingSense(sense);
    const TimingRole *role = (min_max == MinMax::min())
        ? TimingRole::clockTreePathMin()
        : TimingRole::clockTreePathMax();
    lib_builder_->makeClockTreePathArcs(cell_, lib_port, role, attrs);
  }
}

////////////////////////////////////////////////////////////////

LibertyPort *
MakeTimingModel::modelPort(const Pin *pin)
{
  return cell_->findLibertyPort(network_->name(network_->port(pin)));
}

TimingModel *
MakeTimingModel::makeScalarCheckModel(float value,
                                      ScaleFactorType scale_factor_type,
                                      const RiseFall *rf)
{
  TablePtr table = std::make_shared<Table>(value);
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *check_table = new TableModel(table, tbl_template, scale_factor_type, rf);
  TableModels *check_tables = new TableModels(check_table);
  CheckTableModel *check = new CheckTableModel(cell_, check_tables);
  return check;
}

TimingModel *
MakeTimingModel::makeGateModelScalar(Delay delay,
                                     Slew slew,
                                     const RiseFall *rf)
{
  TablePtr delay_table = std::make_shared<Table>(delayAsFloat(delay));
  TablePtr slew_table = std::make_shared<Table>(delayAsFloat(slew));
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *delay_model = new TableModel(delay_table, tbl_template,
                                           ScaleFactorType::cell, rf);
  TableModels *delay_models = new TableModels(delay_model);
  TableModel *slew_model = new TableModel(slew_table, tbl_template,
                                          ScaleFactorType::cell, rf);
  TableModels *slew_models = new TableModels(slew_model);
  GateTableModel *gate_model = new GateTableModel(cell_, delay_models, slew_models,
                                                  nullptr, nullptr);
  return gate_model;
}

TimingModel *
MakeTimingModel::makeGateModelScalar(Delay delay,
                                     const RiseFall *rf)
{
  TablePtr delay_table = std::make_shared<Table>(delayAsFloat(delay));
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *delay_model = new TableModel(delay_table, tbl_template,
                                           ScaleFactorType::cell, rf);
  TableModels *models = new TableModels(delay_model);
  GateTableModel *gate_model = new GateTableModel(cell_, models, nullptr,
                                                  nullptr, nullptr);
  return gate_model;
}

// Eval the driver pin model along its load capacitance
// axis and add the input to output 'delay' to the table values.
TimingModel *
MakeTimingModel::makeGateModelTable(const Pin *output_pin,
                                    Delay delay,
                                    const RiseFall *rf)
{
  const Pvt *pvt = sdc_->operatingConditions(min_max_);
  PinSet *drvrs = network_->drivers(network_->net(network_->term(output_pin)));
  DcalcAPIndex ap_index = scene_->dcalcAnalysisPtIndex(min_max_);
  const Pin *drvr_pin = *drvrs->begin();
  const LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  if (drvr_port) {
    const LibertyCell *drvr_cell = drvr_port->libertyCell();
    for (TimingArcSet *arc_set : drvr_cell->timingArcSetsTo(drvr_port)) {
      for (TimingArc *drvr_arc : arc_set->arcs()) {
        // Use the first timing arc to simplify life.
        if (drvr_arc->toEdge()->asRiseFall() == rf) {
          const LibertyPort *gate_in_port = drvr_arc->from();
          const Instance *drvr_inst = network_->instance(drvr_pin);
          const Pin *gate_in_pin = network_->findPin(drvr_inst, gate_in_port);
          if (gate_in_pin) {
            Vertex *gate_in_vertex = graph_->pinLoadVertex(gate_in_pin);
            Slew in_slew = graph_->slew(
                gate_in_vertex, drvr_arc->fromEdge()->asRiseFall(), ap_index);
            float in_slew1 = delayAsFloat(in_slew);
            GateTableModel *drvr_gate_model =
                drvr_arc->gateTableModel(scene_, min_max_);
            if (drvr_gate_model) {
              float output_load_cap = graph_delay_calc_->loadCap(output_pin,
                                                                 scene_,
                                                                 min_max_);
              float drvr_self_delay, drvr_self_slew;
              drvr_gate_model->gateDelay(pvt, in_slew1, output_load_cap,
                                         drvr_self_delay, drvr_self_slew);

              const TableModel *drvr_table = drvr_gate_model->delayModels()->model();
              const TableTemplate *drvr_template = drvr_table->tblTemplate();
              const TableAxis *drvr_load_axis = loadCapacitanceAxis(drvr_table);
              if (drvr_load_axis) {
                const FloatSeq &drvr_axis_values = drvr_load_axis->values();
                FloatSeq *load_values = new FloatSeq;
                FloatSeq *slew_values = new FloatSeq;
                for (float load_cap : drvr_axis_values) {
                  // get slew from driver input pin
                  float gate_delay, gate_slew;
                  drvr_gate_model->gateDelay(pvt, in_slew1, load_cap,
                                             gate_delay, gate_slew);
                  // Remove the self delay driving the output pin net load cap.
                  load_values->push_back(delayAsFloat(delay)
                                         + gate_delay
                                         - drvr_self_delay);
                  slew_values->push_back(delayAsFloat(gate_slew));
                }

                FloatSeq axis_values = drvr_axis_values;
                TableAxisPtr load_axis = std::make_shared<TableAxis>(
                    TableAxisVariable::total_output_net_capacitance,
                    std::move(axis_values));

                TablePtr delay_table =
                    std::make_shared<Table>(load_values, load_axis);
                TablePtr slew_table =
                    std::make_shared<Table>(slew_values, load_axis);

                TableTemplate *model_template =
                    ensureTableTemplate(drvr_template, load_axis);
                TableModel *delay_model = new TableModel(delay_table, model_template,
                                                         ScaleFactorType::cell, rf);
                TableModels *delay_models = new TableModels(delay_model);
                TableModel *slew_model = new TableModel(slew_table, model_template,
                                                        ScaleFactorType::cell, rf);
                TableModels *slew_models = new TableModels(slew_model);
                GateTableModel *gate_model = new GateTableModel(cell_,
                                                                delay_models,
                                                                slew_models,
                                                                nullptr, nullptr);
                return gate_model;
              }
            }
          }
        }
      }
    }
  }
  Vertex *output_vertex = graph_->pinLoadVertex(output_pin);
  Slew slew = graph_->slew(output_vertex, rf, ap_index);
  return makeGateModelScalar(delay, slew, rf);
}

TableTemplate *
MakeTimingModel::ensureTableTemplate(const TableTemplate *drvr_template,
                                     const TableAxisPtr &load_axis)
{
  TableTemplate *model_template = findKey(template_map_, drvr_template);
  if (model_template == nullptr) {
    std::string template_name = "template_";
    template_name += std::to_string(tbl_template_index_++);

    model_template =
        library_->makeTableTemplate(template_name, TableTemplateType::delay);
    model_template->setAxis1(load_axis);
    template_map_[drvr_template] = model_template;
  }
  return model_template;
}

const TableAxis *
MakeTimingModel::loadCapacitanceAxis(const TableModel *table)
{
  if (table->axis1()
      && table->axis1()->variable()
          == TableAxisVariable::total_output_net_capacitance)
    return table->axis1();
  else if (table->axis2()
           && table->axis2()->variable()
               == TableAxisVariable::total_output_net_capacitance)
    return table->axis2();
  else if (table->axis3()
           && table->axis3()->variable()
               == TableAxisVariable::total_output_net_capacitance)
    return table->axis3();
  else
    return nullptr;
}

OutputDelays::OutputDelays()
{
  rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()] = false;
  rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()] = false;
  rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()] = false;
  rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()] = false;
}

TimingSense
OutputDelays::timingSense() const
{
  if (rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
      && rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()]
      && !rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
      && !rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()])
    return TimingSense::positive_unate;
  else if (rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
           && rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()]
           && !rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
           && !rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()])
    return TimingSense::negative_unate;
  else if (rf_path_exists[RiseFall::riseIndex()][RiseFall::riseIndex()]
           || rf_path_exists[RiseFall::riseIndex()][RiseFall::fallIndex()]
           || rf_path_exists[RiseFall::fallIndex()][RiseFall::riseIndex()]
           || rf_path_exists[RiseFall::fallIndex()][RiseFall::fallIndex()])
    return TimingSense::non_unate;
  else
    return TimingSense::none;
}

}  // namespace sta
